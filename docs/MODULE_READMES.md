# 模块 README 汇总

此文件将原先分散在各子模块目录下的 README 合并为单个集中参考，便于维护。下面包含各模块的 README 内容摘录。

---

<!-- ImageAnalyzer -->

# ImageAnalyzer 模块

位置: `ImageAnalyzer/`

目的
- 打开与解析磁盘镜像，检测分区与文件系统，遍历文件并将元数据写入数据库。
- 对 XFS 提供备用解析策略（原生挂载 / 纯解析器）。

主要文件
- `ImageAnalyzer.h`, `ImageAnalyzer.cpp`
- `XFSHelper.h`, `XFSHelper.cpp`
- `NativeFilesystemWalker.h`, `NativeFilesystemWalker.cpp`

公共 API（摘录）
- `ImageAnalyzer(const std::string& imagePath)`
- `bool analyze()`
- `bool extractToDatabase(const std::string& dbPath)`
- `void setXFSMode(XFSMode mode)`
- `TSK_IMG_INFO* getImageInfo() const`
- `TSK_FS_INFO* getFileSystemInfo() const`

使用示例
```bash
# 普通分析
./forensic_analyzer disk_image.dd

# 使用纯 XFS 解析
./forensic_analyzer disk_image.e01 --xfs-mode pure
```

实现要点
- 使用 TSK API 作为首选解析路径（`tsk_img_open` / `tsk_fs_open_img` / `tsk_fs_dir_walk`）
- 当 TSK 无法读取 XFS 时，根据 `xfsMode_` 回退至 `NativeFilesystemWalker`（Linux，需 root）或 `XFSHelper`（纯解析器）
- `processFile` 将 TSK 元数据转换为 `FileRecord` 并调用 `DatabaseManager::insertFileRecord`

测试建议
- 准备小型 NTFS/EXT/XFS 镜像，分别验证 `analyze()` 和 `extractToDatabase()`
- 验证 XFS 三种模式 (auto/native/pure) 在失败场景下的回退行为

扩展点
- 在 `processFile` 中增加 xattr 提取
- 为 XFSHelper 增加并发扫描与增量提取

---

<!-- DatabaseManager -->

# DatabaseManager 模块

位置: `DatabaseManager/`

目的
- 封装 SQLite 操作，管理数据库文件、表结构、索引和常用插入/查询 API。

主要文件
- `DatabaseManager.h`, `DatabaseManager.cpp`

核心类型
- `FileRecord` (inode, name, path, size, atime, mtime, ctime, crtime, type, md5, isDeleted, isAllocated, permissions, uid, gid)
- `EventRecord` (timestamp, eventType, filePath, inode, description)

公共 API（摘录）
- `DatabaseManager(const std::string& dbPath)`
- `bool initialize()`
- `bool insertFileRecord(const FileRecord& record)`
- `bool insertEventRecord(const EventRecord& record)`
- `bool insertPartitionInfo(int partNum, int64_t start, int64_t length, const std::string& desc, const std::string& fsType)`
- `sqlite3* getDb() const`

实现要点
- 在 `initialize()` 中创建核心表（files、partitions）并建立索引
- 使用事务提高批量写入性能（EventExtractor 已示例）
- 可选启用 WAL 模式以改善读写并发

测试建议
- 使用 SQLite `:memory:` 做单元测试，验证表、索引和插入行为
- 模拟大量记录，验证插入性能并调整索引与事务策略

扩展点
- 支持 schema migration/versioning
- 提供更丰富的查询封装接口

---

<!-- EventExtractor -->

# EventExtractor 子模块

位置: `DatabaseManager/EventExtractor/`

目的
- 从 `_raw.db` 的 `files` 表生成时间线事件并写入 `_events.db`，创建视图便于分析。

主要文件
- `EventExtractor.h`, `EventExtractor.cpp`

公共 API（摘录）
- `EventExtractor(const std::string& sourceDbPath, const std::string& eventDbPath)`
- `bool extractEvents()`

行为说明
- 读取 `files` 表的 atime/mtime/ctime/crtime 等元数据
- 将时间字段逻辑映射为 CREATED/MODIFIED/ACCESSED/CHANGED/DELETED 事件
- 同时维护主 `events` 表和按事件类型的专用表（creation_events 等）
- 创建 `timeline`、`event_statistics`、`hourly_activity` 等视图

实现要点
- 使用事务批量插入以提升性能
- 对缺少 crtime 的文件需容错处理
- 可在插入前做去重策略（例如基于 inode+timestamp+type）

测试建议
- 使用包含已知时间戳的小数据库验证事件数量与排序
- 测试重复记录的去重策略

扩展点
- 增加基于哈希的变更检测事件
- 支持将事件导出为外部时间线格式（如 Plaso 等）

---

<!-- FileClassifier -->

# FileClassifier 子模块

位置: `DatabaseManager/FileClassifier/`

目的
- 将 `_raw.db` 中的文件按扩展名/类型分类到 `_files.db`，并创建统计视图以便快速检索。

主要文件
- `FileClassifier.h`, `FileClassifier.cpp`

公共 API（摘录）
- `FileClassifier(const std::string& sourceDbPath, const std::string& fileDbPath)`
- `bool classifyAndExtract()`

实现要点
- 初始化扩展名到 `FileCategory` 的映射（`extensionMap_`）
- 创建主 `files` 表与 13 个分类表（images/videos/.../unknown_files）
- 分类时使用预编译语句和事务优化大规模插入

分类规则
- 首先依据扩展名（小写化）匹配常用类型
- 可选 fallback: 使用内容检测（libmagic）以提高准确性

测试建议
- 用包含各种扩展名的样例数据库测试 `file_summary` 和 `extension_statistics` 视图

扩展点
- 集成 MIME/魔数检测作为扩展名的后备判定
- 支持自定义分类映射配置文件（JSON/YAML）以便用户扩展

---

<!-- FileExtractor -->

# FileExtractor 子模块

位置: `DatabaseManager/FileExtractor/`

目的
- 基于数据库记录从镜像中提取文件内容，支持按名称/扩展/全部/包含已删除的提取。

主要文件
- `FileExtractor.h`, `FileExtractor.cpp`

公共 API（摘录）
- `FileExtractor(const std::string& imagePath, const std::string& dbPath)`
- `bool initialize()`
- `int extractByName(const std::string& pattern, const std::string& outputDir)`
- `int extractByExtension(const std::string& extensions, const std::string& outputDir)`
- `int extractAll(const std::string& outputDir, bool includeDeleted = false)`
- `bool extractFileByInode(int64_t inode, const std::string& outputPath)`

实现要点
- 使用 TSK API 读取文件数据（按块读取并流写入磁盘）
- 支持通配符（*, ?）匹配文件名
- 保持输出文件的时间戳/权限（如果可行），并记录提取日志

错误处理
- 对 I/O 或读取失败进行日志记录并继续处理其他文件
- 对命名冲突采用后缀重命名策略

测试建议
- 提取后对比文件大小与 MD5（可选）以验证完整性

扩展点
- 增加并发提取配置（限制并发写线程数）
- 提取后自动计算并写入 MD5/SHA256

---

<!-- HTTPServer -->

# HTTPServer 模块

位置: `HTTPServer/`

目的
- 提供 RESTful API 管理异步分析任务并查询结果，基于 Crow + Boost.Asio。

主要文件
- `HTTPserver.h`, `HTTPserver.cpp`
- `TaskManager.h`
- `SQLiteHelper.h`
- `Utils.h`

API 端点
- `POST /tasks` - 创建分析任务
- `GET /tasks/{id}` - 查询任务状态
- `GET /tasks/{id}/results` - 获取任务结果（若完成）

运行示例
```bash
./forensic_analyzer --http-server 8080
curl -X POST http://localhost:8080/tasks -H "Content-Type: application/json" -d '{"image_path":"/path/image.e01"}'
```

实现要点
- `HTTPserver` 使用 `crow::App<>` 定义路由并返回 JSON
- `TaskManager` 为任务状态的唯一来源
- 对大结果集使用分页避免内存峰值

安全建议
- 在生产环境前端加 nginx/TLS 并做鉴权
- 将执行任务与 HTTP 服务分离到 worker 后台进程或队列

扩展点
- 对接消息队列（RabbitMQ/Redis）实现分布式任务执行
- 添加认证与访问控制

---

<!-- TaskManager -->

# TaskManager 子模块

位置: `HTTPServer/TaskManager.h`

目的
- 单例线程安全的任务生命周期管理。

主要文件
- `TaskManager.h`

公共 API（摘录）
- `static TaskManager& instance()`
- `std::string create_task(const std::string& path)`
- `void update_status(const std::string& id, TaskStatus status, const std::string& msg = "")`
- `void set_result_db(const std::string& id, const std::string& db_path)`
- `AnalysisTask get_task(const std::string& id)`

实现要点
- 使用 `std::map<std::string, AnalysisTask>` 存储任务，`std::mutex` 保护并发访问
- 使用 Boost.Uuid 生成唯一的 task_id

---

<!-- SQLiteHelper -->

# SQLiteHelper 子模块

位置: `HTTPServer/SQLiteHelper.h`

目的
- 将 SQLite 查询结果转换为 JSON，并提供常用查询封装（分页、过滤）。

主要文件
- `SQLiteHelper.h`

建议接口
- `nlohmann::json queryToJson(sqlite3* db, const std::string& sql, int limit = 100, int offset = 0)`
- `nlohmann::json getFileCategories(sqlite3* db)`
- `nlohmann::json getFilesByCategory(sqlite3* db, const std::string& category, int limit, int offset)`

实现要点
- 对每次查询进行行数限制并支持 offset 分页
- 对文本字段进行 UTF-8 验证

---

<!-- Utils -->

# Utils 子模块

位置: `HTTPServer/Utils.h`

目的
- 通用工具函数集合，包含协程/异步包装、错误转换和常用字符串处理。

主要文件
- `Utils.h`

示例工具
- `run_async(Func&& func)` - 使用 `std::async` 启动后台任务
- `AsyncTask` - 简单的 coroutine promise/返回对象示例

并发注意
- 在协程或多线程环境共享资源时使用局部 DB 句柄或互斥保护

---

<!-- AndroidAnalyzer -->

# AndroidAnalyzer 模块

位置: `AndroidAnalyzer/`

目的
- 解析 Android 镜像/备份中应用数据（如 SMS、联系人、通话记录、应用数据等）。

主要文件
- `AndroidAnalyzer.h`, `AndroidAnalyzer.cpp`

实现要点
- 重点处理 `/data/data/*` 下的 SQLite 数据库和应用私有文件
- 提供基于文件路径和已知数据库结构的解析器

测试建议
- 使用脱敏或合规的测试备份验证解析结果字段

扩展点
- 添加对常见应用（WhatsApp, WeChat 等）数据库的专用解析器
- 支持 APK 签名与证书分析

---

*文件生成时间: 2025-12-04*
