#include "FileClassifier.h"
#include <iostream>
#include <algorithm>
#include <cctype>

FileClassifier::FileClassifier(const std::string& sourceDbPath,
	const std::string& fileDbPath)
	: sourceDbPath_(sourceDbPath), fileDbPath_(fileDbPath),
	sourceDb_(nullptr), fileDb_(nullptr) {
	initializeExtensionMap();
}

FileClassifier::~FileClassifier() {
	closeDatabases();
}

bool FileClassifier::classifyAndExtract() {
	if (!openDatabases()) {
		return false;
	}

	if (!createCategoryTables()) {
		return false;
	}

	if (!classifyFiles()) {
		return false;
	}

	std::cout << "Files classified successfully" << std::endl;

	return true;
}

bool FileClassifier::openDatabases() {
	int rc = sqlite3_open(sourceDbPath_.c_str(), &sourceDb_);
	if (rc != SQLITE_OK) {
		std::cerr << "Cannot open source database: " << sqlite3_errmsg(sourceDb_) << std::endl;
		return false;
	}

	rc = sqlite3_open(fileDbPath_.c_str(), &fileDb_);
	if (rc != SQLITE_OK) {
		std::cerr << "Cannot open file database: " << sqlite3_errmsg(fileDb_) << std::endl;
		return false;
	}

	return true;
}

bool FileClassifier::createCategoryTables() {
	// Create main files table first
	std::string createFilesTable = R"(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            inode INTEGER,
            name TEXT,
            path TEXT,
            size INTEGER,
            extension TEXT,
            category TEXT,
            type TEXT,
            mtime INTEGER,
            ctime INTEGER,
            is_deleted INTEGER,
            md5 TEXT
        );
    )";

	char* errMsg = nullptr;
	int rc = sqlite3_exec(fileDb_, createFilesTable.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		std::cerr << "Failed to create files table: " << errMsg << std::endl;
		sqlite3_free(errMsg);
		return false;
	}

	// Create indices for files table
	sqlite3_exec(fileDb_, "CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);", nullptr, nullptr, nullptr);
	sqlite3_exec(fileDb_, "CREATE INDEX IF NOT EXISTS idx_files_category ON files(category);", nullptr, nullptr, nullptr);

	// Create table for each category
	std::vector<FileCategory> categories = {
		FileCategory::IMAGE,
		FileCategory::VIDEO,
		FileCategory::AUDIO,
		FileCategory::DOCUMENT,
		FileCategory::ARCHIVE,
		FileCategory::EXECUTABLE,
		FileCategory::DATABASE,
		FileCategory::SOURCE_CODE,
		FileCategory::WEB,
		FileCategory::EMAIL,
		FileCategory::SYSTEM,
		FileCategory::ENCRYPTED,
		FileCategory::UNKNOWN
	};

	for (const auto& category : categories) {
		std::string tableName = getCategoryTableName(category);

		std::string createTableSql = "CREATE TABLE IF NOT EXISTS " + tableName + R"( (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            inode INTEGER,
            name TEXT,
            path TEXT,
            size INTEGER,
            extension TEXT,
            mtime INTEGER,
            ctime INTEGER,
            is_deleted INTEGER,
            md5 TEXT
        );)";

		char* errMsg = nullptr;
		int rc = sqlite3_exec(fileDb_, createTableSql.c_str(), nullptr, nullptr, &errMsg);

		if (rc != SQLITE_OK) {
			std::cerr << "Failed to create table " << tableName << ": " << errMsg << std::endl;
			sqlite3_free(errMsg);
			return false;
		}

		// Create indices
		std::string indexSql = "CREATE INDEX IF NOT EXISTS idx_" + tableName +
			"_path ON " + tableName + "(path);";
		sqlite3_exec(fileDb_, indexSql.c_str(), nullptr, nullptr, nullptr);

		indexSql = "CREATE INDEX IF NOT EXISTS idx_" + tableName +
			"_extension ON " + tableName + "(extension);";
		sqlite3_exec(fileDb_, indexSql.c_str(), nullptr, nullptr, nullptr);

		indexSql = "CREATE INDEX IF NOT EXISTS idx_" + tableName +
			"_size ON " + tableName + "(size);";
		sqlite3_exec(fileDb_, indexSql.c_str(), nullptr, nullptr, nullptr);
	}

	// Create summary view
	std::string createSummaryView = R"(
        CREATE VIEW IF NOT EXISTS file_summary AS
        SELECT
            'Images' as category,
            COUNT(*) as file_count,
            SUM(size) as total_size,
            ROUND(AVG(size), 2) as avg_size,
            MAX(size) as max_size
        FROM images
        UNION ALL
        SELECT 'Videos', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM videos
        UNION ALL
        SELECT 'Audio', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM audio_files
        UNION ALL
        SELECT 'Documents', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM documents
        UNION ALL
        SELECT 'Archives', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM archives
        UNION ALL
        SELECT 'Executables', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM executables
        UNION ALL
        SELECT 'Databases', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM databases
        UNION ALL
        SELECT 'Source Code', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM source_code
        UNION ALL
        SELECT 'Web Files', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM web_files
        UNION ALL
        SELECT 'Email', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM email_files
        UNION ALL
        SELECT 'System Files', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM system_files
        UNION ALL
        SELECT 'Encrypted', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM encrypted_files
        UNION ALL
        SELECT 'Unknown', COUNT(*), SUM(size), ROUND(AVG(size), 2), MAX(size) FROM unknown_files;
    )";

	sqlite3_exec(fileDb_, createSummaryView.c_str(), nullptr, nullptr, nullptr);

	// Create extension statistics view
	std::string createExtensionView = R"(
        CREATE VIEW IF NOT EXISTS extension_statistics AS
        SELECT extension, COUNT(*) as count, SUM(size) as total_size
        FROM (
            SELECT extension, size FROM images
            UNION ALL SELECT extension, size FROM videos
            UNION ALL SELECT extension, size FROM audio_files
            UNION ALL SELECT extension, size FROM documents
            UNION ALL SELECT extension, size FROM archives
            UNION ALL SELECT extension, size FROM executables
            UNION ALL SELECT extension, size FROM databases
            UNION ALL SELECT extension, size FROM source_code
            UNION ALL SELECT extension, size FROM web_files
            UNION ALL SELECT extension, size FROM email_files
            UNION ALL SELECT extension, size FROM system_files
            UNION ALL SELECT extension, size FROM encrypted_files
            UNION ALL SELECT extension, size FROM unknown_files
        )
        GROUP BY extension
        ORDER BY count DESC;
    )";

	sqlite3_exec(fileDb_, createExtensionView.c_str(), nullptr, nullptr, nullptr);

	// Create deleted files view
	std::string createDeletedView = R"(
        CREATE VIEW IF NOT EXISTS deleted_files AS
        SELECT 'Images' as category, name, path, size, extension FROM images WHERE is_deleted = 1
        UNION ALL
        SELECT 'Videos', name, path, size, extension FROM videos WHERE is_deleted = 1
        UNION ALL
        SELECT 'Audio', name, path, size, extension FROM audio_files WHERE is_deleted = 1
        UNION ALL
        SELECT 'Documents', name, path, size, extension FROM documents WHERE is_deleted = 1
        UNION ALL
        SELECT 'Archives', name, path, size, extension FROM archives WHERE is_deleted = 1
        UNION ALL
        SELECT 'Executables', name, path, size, extension FROM executables WHERE is_deleted = 1
        UNION ALL
        SELECT 'Databases', name, path, size, extension FROM databases WHERE is_deleted = 1
        UNION ALL
        SELECT 'Source Code', name, path, size, extension FROM source_code WHERE is_deleted = 1
        UNION ALL
        SELECT 'Web Files', name, path, size, extension FROM web_files WHERE is_deleted = 1
        UNION ALL
        SELECT 'Email', name, path, size, extension FROM email_files WHERE is_deleted = 1
        UNION ALL
        SELECT 'System Files', name, path, size, extension FROM system_files WHERE is_deleted = 1
        UNION ALL
        SELECT 'Encrypted', name, path, size, extension FROM encrypted_files WHERE is_deleted = 1
        UNION ALL
        SELECT 'Unknown', name, path, size, extension FROM unknown_files WHERE is_deleted = 1;
    )";

	sqlite3_exec(fileDb_, createDeletedView.c_str(), nullptr, nullptr, nullptr);

	return true;
}

bool FileClassifier::classifyFiles() {
	const char* query = R"(
        SELECT inode, name, path, size, mtime, ctime, type, is_deleted, md5
        FROM files
        WHERE type = 'REG';
    )";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(sourceDb_, query, -1, &stmt, nullptr);

	if (rc != SQLITE_OK) {
		std::cerr << "Failed to prepare query: " << sqlite3_errmsg(sourceDb_) << std::endl;
		return false;
	}

	std::unordered_map<FileCategory, int> categoryCounts;

	// Begin transaction for better performance
	sqlite3_exec(fileDb_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int64_t inode = sqlite3_column_int64(stmt, 0);
		std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		std::string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		int64_t size = sqlite3_column_int64(stmt, 3);
		int64_t mtime = sqlite3_column_int64(stmt, 4);
		int64_t ctime = sqlite3_column_int64(stmt, 5);
		std::string type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
		int isDeleted = sqlite3_column_int(stmt, 7);
		const char* md5Ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
		std::string md5 = md5Ptr ? md5Ptr : "";

		// Determine category
		FileCategory category = determineCategory(name, type);
		categoryCounts[category]++;

		// Get extension
		std::string extension;
		size_t dotPos = name.find_last_of('.');
		if (dotPos != std::string::npos) {
			extension = name.substr(dotPos + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		}

		// Insert into appropriate table
		std::string tableName = getCategoryTableName(category);
		std::string insertSql = "INSERT INTO " + tableName +
			" (inode, name, path, size, extension, mtime, ctime, is_deleted, md5) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

		sqlite3_stmt* insertStmt;
		sqlite3_prepare_v2(fileDb_, insertSql.c_str(), -1, &insertStmt, nullptr);

		sqlite3_bind_int64(insertStmt, 1, inode);
		sqlite3_bind_text(insertStmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insertStmt, 3, path.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(insertStmt, 4, size);
		sqlite3_bind_text(insertStmt, 5, extension.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(insertStmt, 6, mtime);
		sqlite3_bind_int64(insertStmt, 7, ctime);
		sqlite3_bind_int(insertStmt, 8, isDeleted);
		sqlite3_bind_text(insertStmt, 9, md5.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(insertStmt);
		sqlite3_finalize(insertStmt);

		// Also insert into main files table
		std::string categoryName = getCategoryName(category);
		std::string filesInsertSql = "INSERT INTO files "
			"(inode, name, path, size, extension, category, type, mtime, ctime, is_deleted, md5) "
			"VALUES (?, ?, ?, ?, ?, ?, 'REG', ?, ?, ?, ?);";

		sqlite3_prepare_v2(fileDb_, filesInsertSql.c_str(), -1, &insertStmt, nullptr);

		sqlite3_bind_int64(insertStmt, 1, inode);
		sqlite3_bind_text(insertStmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insertStmt, 3, path.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(insertStmt, 4, size);
		sqlite3_bind_text(insertStmt, 5, extension.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insertStmt, 6, categoryName.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(insertStmt, 7, mtime);
		sqlite3_bind_int64(insertStmt, 8, ctime);
		sqlite3_bind_int(insertStmt, 9, isDeleted);
		sqlite3_bind_text(insertStmt, 10, md5.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(insertStmt);
		sqlite3_finalize(insertStmt);
	}

	sqlite3_finalize(stmt);

	// Commit transaction
	sqlite3_exec(fileDb_, "COMMIT;", nullptr, nullptr, nullptr);

	// Print statistics
	std::cout << "  File classification statistics:" << std::endl;
	for (const auto& [category, count] : categoryCounts) {
		if (count > 0) {
			std::cout << "    - " << getCategoryName(category) << ": " << count << " files" << std::endl;
		}
	}

	return true;
}

FileCategory FileClassifier::determineCategory(const std::string& filename,
	const std::string& type) {
	// Extract extension
	std::string extension;
	size_t dotPos = filename.find_last_of('.');
	if (dotPos != std::string::npos) {
		extension = filename.substr(dotPos + 1);
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	}

	// Check extension map
	auto it = extensionMap_.find(extension);
	if (it != extensionMap_.end()) {
		return it->second;
	}

	return FileCategory::UNKNOWN;
}

std::string FileClassifier::getCategoryName(FileCategory category) {
	switch (category) {
	case FileCategory::IMAGE: return "Images";
	case FileCategory::VIDEO: return "Videos";
	case FileCategory::AUDIO: return "Audio Files";
	case FileCategory::DOCUMENT: return "Documents";
	case FileCategory::ARCHIVE: return "Archives";
	case FileCategory::EXECUTABLE: return "Executables";
	case FileCategory::DATABASE: return "Databases";
	case FileCategory::SOURCE_CODE: return "Source Code";
	case FileCategory::WEB: return "Web Files";
	case FileCategory::EMAIL: return "Email Files";
	case FileCategory::SYSTEM: return "System Files";
	case FileCategory::ENCRYPTED: return "Encrypted Files";
	case FileCategory::UNKNOWN: return "Unknown Files";
	default: return "Unknown";
	}
}

std::string FileClassifier::getCategoryTableName(FileCategory category) {
	switch (category) {
	case FileCategory::IMAGE: return "images";
	case FileCategory::VIDEO: return "videos";
	case FileCategory::AUDIO: return "audio_files";
	case FileCategory::DOCUMENT: return "documents";
	case FileCategory::ARCHIVE: return "archives";
	case FileCategory::EXECUTABLE: return "executables";
	case FileCategory::DATABASE: return "databases";
	case FileCategory::SOURCE_CODE: return "source_code";
	case FileCategory::WEB: return "web_files";
	case FileCategory::EMAIL: return "email_files";
	case FileCategory::SYSTEM: return "system_files";
	case FileCategory::ENCRYPTED: return "encrypted_files";
	case FileCategory::UNKNOWN: return "unknown_files";
	default: return "unknown_files";
	}
}

void FileClassifier::initializeExtensionMap() {
	// Image files
	std::vector<std::string> imageExts = {
		"jpg", "jpeg", "png", "gif", "bmp", "tiff", "tif", "ico", "svg",
		"webp", "raw", "cr2", "nef", "arw", "dng", "psd", "ai", "eps",
		"heic", "heif", "jfif", "exif"
	};
	for (const auto& ext : imageExts) {
		extensionMap_[ext] = FileCategory::IMAGE;
	}

	// Video files
	std::vector<std::string> videoExts = {
		"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpg",
		"mpeg", "3gp", "f4v", "swf", "vob", "ogv", "m2ts", "mts", "ts",
		"divx", "xvid", "rm", "rmvb", "asf"
	};
	for (const auto& ext : videoExts) {
		extensionMap_[ext] = FileCategory::VIDEO;
	}

	// Audio files
	std::vector<std::string> audioExts = {
		"mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus", "ape",
		"alac", "aiff", "au", "mid", "midi", "ra", "rm", "amr", "ac3"
	};
	for (const auto& ext : audioExts) {
		extensionMap_[ext] = FileCategory::AUDIO;
	}

	// Document files
	std::vector<std::string> documentExts = {
		"pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "ods",
		"odp", "rtf", "txt", "csv", "log", "tex", "wpd", "wps", "pages",
		"numbers", "key", "epub", "mobi", "azw", "djvu", "fb2"
	};
	for (const auto& ext : documentExts) {
		extensionMap_[ext] = FileCategory::DOCUMENT;
	}

	// Archive files
	std::vector<std::string> archiveExts = {
		"zip", "rar", "7z", "tar", "gz", "bz2", "xz", "iso", "dmg",
		"pkg", "deb", "rpm", "cab", "msi", "jar", "war", "ear", "apk",
		"tgz", "tbz2", "lz", "lzma", "z", "arj", "ace"
	};
	for (const auto& ext : archiveExts) {
		extensionMap_[ext] = FileCategory::ARCHIVE;
	}

	// Executable files
	std::vector<std::string> executableExts = {
		"exe", "dll", "so", "dylib", "app", "bin", "com", "bat", "cmd",
		"sh", "bash", "ps1", "vbs", "wsf", "msi", "scr", "cpl", "sys",
		"drv", "ocx", "elf", "out", "run"
	};
	for (const auto& ext : executableExts) {
		extensionMap_[ext] = FileCategory::EXECUTABLE;
	}

	// Database files
	std::vector<std::string> databaseExts = {
		"db", "sqlite", "sqlite3", "mdb", "accdb", "dbf", "sql", "bak",
		"mdf", "ldf", "frm", "ibd", "pdb", "fdb", "gdb", "nsf"
	};
	for (const auto& ext : databaseExts) {
		extensionMap_[ext] = FileCategory::DATABASE;
	}

	// Source code files
	std::vector<std::string> sourceCodeExts = {
		"c", "cpp", "cc", "cxx", "h", "hpp", "java", "py", "js", "ts",
		"php", "rb", "go", "rs", "swift", "kt", "scala", "pl", "lua",
		"r", "m", "mm", "cs", "vb", "asm", "s", "pas", "f", "f90",
		"sql", "sh", "bash", "ps1", "bat", "cmd", "makefile", "cmake"
	};
	for (const auto& ext : sourceCodeExts) {
		extensionMap_[ext] = FileCategory::SOURCE_CODE;
	}

	// Web files
	std::vector<std::string> webExts = {
		"html", "htm", "xhtml", "css", "scss", "sass", "less", "xml",
		"json", "yaml", "yml", "jsp", "asp", "aspx", "php", "cgi",
		"rss", "atom", "wsdl", "xsl", "xslt", "dtd"
	};
	for (const auto& ext : webExts) {
		extensionMap_[ext] = FileCategory::WEB;
	}

	// Email files
	std::vector<std::string> emailExts = {
		"eml", "msg", "pst", "ost", "mbox", "emlx", "mbx", "dbx"
	};
	for (const auto& ext : emailExts) {
		extensionMap_[ext] = FileCategory::EMAIL;
	}

	// System files
	std::vector<std::string> systemExts = {
		"ini", "cfg", "conf", "config", "reg", "dat", "tmp", "temp",
		"cache", "bak", "old", "swp", "swo", "lock", "pid", "core"
	};
	for (const auto& ext : systemExts) {
		extensionMap_[ext] = FileCategory::SYSTEM;
	}

	// Encrypted files
	std::vector<std::string> encryptedExts = {
		"gpg", "pgp", "asc", "enc", "encrypted", "aes", "crypt", "locked",
		"secure", "p12", "pfx", "pem", "key", "crt", "cer", "der"
	};
	for (const auto& ext : encryptedExts) {
		extensionMap_[ext] = FileCategory::ENCRYPTED;
	}
}

void FileClassifier::closeDatabases() {
	if (sourceDb_) {
		sqlite3_close(sourceDb_);
		sourceDb_ = nullptr;
	}

	if (fileDb_) {
		sqlite3_close(fileDb_);
		fileDb_ = nullptr;
	}
}