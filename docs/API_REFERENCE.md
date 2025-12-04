# API 参考 (自动汇总)

本文档汇总了项目内主要类的 public API 签名，便于快速查阅和二次开发。

## ImageAnalyzer
- `ImageAnalyzer(const std::string& imagePath)`
- `~ImageAnalyzer()`
- `bool analyze()`
- `bool extractToDatabase(const std::string& dbPath)`
- `void setXFSMode(XFSMode mode)`
- `TSK_IMG_INFO* getImageInfo() const`
- `TSK_FS_INFO* getFileSystemInfo() const`

## DatabaseManager
- `DatabaseManager(const std::string& dbPath)`
- `~DatabaseManager()`
- `bool initialize()`
- `bool insertFileRecord(const FileRecord& record)`
- `bool insertEventRecord(const EventRecord& record)`
- `bool insertPartitionInfo(int partNum, int64_t start, int64_t length, const std::string& desc, const std::string& fsType)`
- `sqlite3* getDb() const`

## EventExtractor
- `EventExtractor(const std::string& sourceDbPath, const std::string& eventDbPath)`
- `~EventExtractor()`
- `bool extractEvents()`

## FileClassifier
- `FileClassifier(const std::string& sourceDbPath, const std::string& fileDbPath)`
- `~FileClassifier()`
- `bool classifyAndExtract()`

## FileExtractor
- `FileExtractor(const std::string& imagePath, const std::string& dbPath)`
- `~FileExtractor()`
- `bool initialize()`
- `int extractByName(const std::string& pattern, const std::string& outputDir)`
- `int extractByExtension(const std::string& extensions, const std::string& outputDir)`
- `int extractAll(const std::string& outputDir, bool includeDeleted = false)`
- `bool extractFileByInode(int64_t inode, const std::string& outputPath)`

## HTTPServer
- `HTTPServer(asio::io_context& ioc)`
- `void run(int port = 8080)`

## TaskManager
- `static TaskManager& instance()`
- `std::string create_task(const std::string& path)`
- `void update_status(const std::string& id, TaskStatus status, const std::string& msg = "")`
- `void set_result_db(const std::string& id, const std::string& db_path)`
- `AnalysisTask get_task(const std::string& id)`

## SQLiteHelper
- `static nlohmann::json get_file_summary(const std::string& db_path)`

## Utils
- `template<typename Func> auto run_async(Func&& func)` - simple wrapper for `std::async`


> 说明: 本文档基于当前头文件提取的签名，若头文件改动请同步更新此文件。
