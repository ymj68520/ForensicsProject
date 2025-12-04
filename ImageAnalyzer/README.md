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
