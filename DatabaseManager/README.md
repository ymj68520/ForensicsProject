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
