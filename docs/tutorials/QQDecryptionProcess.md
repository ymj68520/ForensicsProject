# QQ 数据库解密过程（NTQQ nt_msg.db，QQ 9.x）

本文档记录从小米整机备份（`QQ(com.tencent.mobileqq).bak`，MIUI BACKUP v2，
862MB）中提取并解密安卓 QQ 9.x（NT 架构，NTQQ）聊天数据库的完整过程。
全程**离线推导密钥**，不需要联网、不需要服务器换取密钥、不需要 root。

复现工具位于 `~/projects/testimgs/MIUIBackup/`：

| 脚本 | 用途 |
|---|---|
| `qq_extract.py` / `qq_extract_all.py` | 流式解包备份（自动定位 tar 偏移），提取数据库与密钥材料 |
| `qq_decrypt.py` | 批量解密 nt_db 全部库（剥头 + sqlcipher_export 导出明文） |

服务内实现同源：`python_service/httpserver/services/qq_decrypt.py`。

## 1. 备份结构

备份为 65 字节文本头（`MIUI BACKUP\n2\ncom.tencent.mobileqq QQ …`）+ tar，
但 tar 起始偏移不是固定 65（首块校验和异常），需要**用 `ustar` 魔数回推
块起始**（偏移 = 魔数位置 − 257）。本备份实测偏移 67，共 4976 条目。

关键数据位于 `apps/com.tencent.mobileqq/`：

- `db/nt_db/nt_qq_<目录哈希>/`：21 个 SQLCipher 加密库（nt_msg.db 等）
- `f/uid/<QQ号>###u_xxx`：QQ 号 ↔ nt_uid 映射文件
- `db/2874289874.db`：旧协议库（明文，无消息，仅结构）

## 2. 密钥推导链（核心）

```
nt_uid ──md5──▶ uid_hash ──+ "nt_kernel"──▶ md5 ──▶ 目录哈希（交叉验证）
                │
                └── + rand（库头 field2）──▶ md5 ──▶ 数据库密钥
```

| 步骤 | 值（实测账号） | 来源 |
|---|---|---|
| QQ 号 (uin) | `2874289874` | 目录名 / `f/uid/` 文件名 |
| nt_uid | `u_UJEcIMaQtYqv4WxT5Bl30Q` | `f/uid/2874289874###u_UJEcIMaQtYqv4WxT5Bl30Q` |
| uid_hash | `687055092041a237e863c7789f085471` | `md5(nt_uid)` |
| 目录哈希 | `b9e935074f88f6d3fa3dd22adce78e2b` | `md5(uid_hash + "nt_kernel")` ✅ 与 `nt_qq_` 目录名一致 |
| rand | `7VklrMEZ` | nt_msg.db 头部 protobuf field2 |
| **key** | **`55a19994a4b6ed150c98ec6d7889c4af`** | `md5(uid_hash + rand)` |

### 库头自定义结构

每个加密库前 1024 字节是自定义头：

```
"SQLite header 3\0"   ← 非标准魔数（标准是 "SQLite format 3\0"）
… 页大小等 …
"QQ_NT DB" <len32>    ← protobuf 载荷：
  field2 = rand（8 位随机串，每库相同）
  field3 = 版本（"1.1.0.1"）
  field4 = HMAC 算法（"HMAC_SHA1"）
  field5 = 时间戳
```

salt 在这 1024 字节之后，因此**解密前必须先剥头**。

## 3. SQLCipher 参数（遍历实测确认）

直接按社区文档的默认参数（PBKDF2_HMAC_SHA1）打开会报 "file is not a
database"。对 kdf/hmac/key 形态/迭代次数全组合遍历后确认：

| 参数 | 值 |
|---|---|
| 算法 | SQLCipher 4 |
| cipher_page_size | 4096 |
| kdf_iter | 4000 |
| cipher_hmac_algorithm | HMAC_SHA1 |
| cipher_kdf_algorithm | **PBKDF2_HMAC_SHA512**（关键差异：不是 SHA1） |
| key 形式 | 32 位 hex **字符串**（`PRAGMA key = '<hex>'`，非 raw） |

PRAGMA 顺序必须是：`cipher_page_size` → `key` → `kdf_iter` →
`cipher_hmac_algorithm` → `cipher_kdf_algorithm`，顺序错误同样解密失败。

## 4. WAL 处理

`-wal` 帧也是加密的，但**不需要手工解密**：把剥头后的主库与原始 `-wal`
放在同目录，sqlcipher 连接打开时会自动解密并回放帧（实测消息数 177 →
181，增量来自 WAL）。

## 5. 结果

- 21 个库中 **20 个解密成功**（`sqlcipher_export` 导出为明文，
  `PRAGMA integrity_check = ok`）→ `qq_decrypted_output/`；
- `nt_msg.db`：c2c_msg_table 2 条 + group_msg_table 181 条，文本经
  40800 protobuf 解码全部可读（群聊昵称如「馒头」「玄德公一乐」）；
- `gpro_v1-6_u_xxx.db`（频道库）使用不同密钥，与聊天无关，跳过；
- 明文联系人昵称（profile_info.db）、群名（group_info.db）一并导出。

## 6. 消息体结构（供解析参考）

| 列 | 含义 |
|---|---|
| 40001 | 消息唯一 ID |
| 40011 / 40013 | 外层类型 / 方向（0=收，1/2=发，3=系统） |
| 40020 / 40033 | 发送者 uid / 发送者 QQ 号 |
| 40030 | 对方 QQ 号（私聊）或群号 |
| 40050 | 时间（Unix 秒） |
| 40090 / 40093 | 群名片 / 昵称 |
| 40800 | `MsgBody.content(40800)` → `MsgContent{45002 内容类型, 45003 媒体类型, 45101 文本}` |

## 7. 陷阱记录

1. **tar 偏移**：首块校验和异常，不能假定偏移 65，用 `ustar` 魔数定位。
2. **KDF 是 SHA512**：社区文档早期写 SHA1，实测必须 `PBKDF2_HMAC_SHA512`，
   否则 hmac check failed for pgno=1。
3. **剥头**：不剥 1024B 自定义头直接喂 sqlcipher 会得到相同报错。
4. **profile_info_v6 是缓存**：含上千条无关名片（名片墙缓存），做关系图时
   只能取消息中实际出现过的联系人，否则图被污染（节点从 36 膨胀到 1176）。
5. **服务进程重启**：改代码后用 `pgrep -f "uvicorn httpserver.main"` 精确
   匹配杀进程，`pgrep -x uvicorn` 会漏掉以完整命令行启动的实例。

## 参考资料

- QQBackup/QQDecrypt（docs/decrypt/extract/NTQQ (Android).md）：离线密钥推导链
- QQBackup/nt_msg_db_util（db_docs/）：40800 等字段语义
- `1.decrypt.py` 的 PRAGMA 顺序说明
