#include "AndroidAnalyzer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

AndroidAnalyzer::AndroidAnalyzer() {
}

AndroidAnalyzer::AndroidAnalyzer(const std::string& imagePath, DatabaseManager* dbManager)
    : imagePath_(imagePath), dbManager_(dbManager) {
}

AndroidAnalyzer::~AndroidAnalyzer() {
}

bool AndroidAnalyzer::initialize() {
    fileExtractor_ = std::make_unique<FileExtractor>(imagePath_, dbManager_->getDbPath());
    if (!fileExtractor_->initialize()) {
        std::cerr << "Failed to initialize FileExtractor" << std::endl;
        return false;
    }

    // Initialize system analysis database
    std::string systemDbPath = imagePath_ + "_system_analysis.db";
    systemDb_ = std::make_unique<SystemAnalysisDatabase>(systemDbPath);
    if (!systemDb_->initialize()) {
        std::cerr << "Failed to initialize SystemAnalysisDatabase" << std::endl;
        return false;
    }

    return true;
}

void AndroidAnalyzer::analyzeAndroidData() {
    std::cout << "Starting Android data analysis..." << std::endl;

    // Analyze system directory
    analyzeSystemDirectory(imagePath_ + "/system");

    // Define known Android app data paths
    std::vector<std::pair<std::string, std::string>> knownApps = {
        {"data/data/com.android.providers.telephony/databases/mmssms.db", "SMS"},
        {"data/data/com.android.providers.contacts/databases/contacts2.db", "Contacts"},
        {"data/data/com.android.providers.contacts/databases/calllog.db", "CallLog"},
        {"data/data/com.whatsapp/databases/msgstore.db", "WhatsApp"},
        {"data/data/com.whatsapp/databases/wa.db", "WhatsApp"}
    };

    for (const auto& app : knownApps) {
        std::string tempPath = "/tmp/" + std::filesystem::path(app.first).filename().string();
        if (extractAndParseDB(app.first, tempPath)) {
            AndroidAppData data;
            data.packageName = app.first.substr(0, app.first.find_last_of('/'));
            data.dbPath = app.first;
            data.dataType = app.second;

            if (app.second == "SMS") {
                parseSMS(tempPath);
            } else if (app.second == "Contacts") {
                parseContacts(tempPath);
            } else if (app.second == "CallLog") {
                parseCallLog(tempPath);
            } else if (app.second == "WhatsApp") {
                parseWhatsApp(tempPath);
            }

            // Clean up temp file
            std::filesystem::remove(tempPath);
        }
    }

    std::cout << "Android data analysis completed." << std::endl;
}

bool AndroidAnalyzer::extractAndParseDB(const std::string& dbPathInImage, const std::string& tempPath) {
    // Extract the database file
    if (!fileExtractor_->extractFileByPath(dbPathInImage, tempPath)) {
        std::cerr << "Failed to extract database: " << dbPathInImage << std::endl;
        return false;
    }

    return true;
}

void AndroidAnalyzer::parseSMS(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open SMS database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, address, body, date, type FROM sms;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "SMS";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["address"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["body"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseContacts(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open Contacts database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, display_name, data1 FROM raw_contacts JOIN data ON raw_contacts._id = data.raw_contact_id WHERE data.mimetype_id = (SELECT _id FROM mimetypes WHERE mimetype = 'vnd.android.cursor.item/phone_v2');";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "Contacts";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["display_name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["phone"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseCallLog(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open CallLog database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, number, date, duration, type FROM calls;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "CallLog";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["number"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 2));
            record["duration"] = std::to_string(sqlite3_column_int(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

std::vector<ChatMessage> AndroidAnalyzer::parseWhatsApp(const std::string& dbPath) {
    std::vector<ChatMessage> messages;
    // WhatsApp parsing - simplified example
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open WhatsApp database: " << sqlite3_errmsg(db) << std::endl;
        return messages;
    }

    // Example: parse messages (this is a simplified version)
    std::string sql = "SELECT _id, key_remote_jid, data, timestamp FROM messages LIMIT 100;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage msg;
            msg.sender = "Unknown"; // Simplified
            msg.receiver = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            msg.timestamp = std::to_string(sqlite3_column_int64(stmt, 3));
            msg.appName = "WhatsApp";
            messages.push_back(msg);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return messages;
}

void AndroidAnalyzer::parseGenericAppData(const std::string& packageName, const std::string& dbPath) {
    // Generic parsing for other apps - list tables and sample data
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        return;
    }

    // Get table names
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::cout << "Found table in " << packageName << ": " << tableName << std::endl;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::insertParsedData(const AndroidAppData& data) {
    std::cout << "Parsed " << data.records.size() << " records from " << data.dataType << std::endl;

    // Insert into database or output to file
    // For now, just print to console
    for (const auto& record : data.records) {
        std::cout << data.dataType << " Record: ";
        for (const auto& field : record) {
            std::cout << field.first << "=" << field.second << " ";
        }
        std::cout << std::endl;
    }
}

void AndroidAnalyzer::analyzeSystemDirectory(const std::string& systemPath) {
    std::cout << "Starting system directory analysis..." << std::endl;

    // Create temp directory for extracted system files
    std::string tempSystemDir = "/tmp/system_analysis_" + std::to_string(std::time(nullptr));
    fs::create_directories(tempSystemDir);

    try {
        // Extract and analyze build.prop
        std::string buildPropTemp = tempSystemDir + "/build.prop";
        if (fileExtractor_->extractFileByPath("system/build.prop", buildPropTemp)) {
            analyzeBuildProperties(buildPropTemp);
        } else {
            std::cout << "Build properties file not found in image: system/build.prop" << std::endl;
        }

        // Extract and scan system apps
        extractAndScanSystemApps("system/app", tempSystemDir + "/app");
        extractAndScanSystemApps("system/priv-app", tempSystemDir + "/priv-app");

        // Extract and scan framework files
        extractAndScanFramework("system/framework", tempSystemDir + "/framework");

    } catch (const std::exception& e) {
        std::cerr << "Error during system directory analysis: " << e.what() << std::endl;
    }

    // Clean up temp directory
    fs::remove_all(tempSystemDir);

    std::cout << "System directory analysis completed." << std::endl;
}

void AndroidAnalyzer::scanSystemApps(const std::string& appDirPath) {
    if (!fs::exists(appDirPath) || !fs::is_directory(appDirPath)) {
        std::cout << "System app directory not found: " << appDirPath << std::endl;
        return;
    }

    bool isPrivileged = (appDirPath.find("priv-app") != std::string::npos);

    for (const auto& entry : fs::recursive_directory_iterator(appDirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".apk") {
            std::string apkPath = entry.path().string();
            SystemAppInfo appInfo = analyzeSystemApk(apkPath, isPrivileged);
            if (!appInfo.packageName.empty()) {
                std::cout << "System App: " << appInfo.packageName << " at " << apkPath << std::endl;
                // Store or process appInfo as needed
            }
        }
    }
}

SystemAppInfo AndroidAnalyzer::analyzeSystemApk(const std::string& apkPath, bool isPrivileged) {
    SystemAppInfo info;
    info.apkPath = apkPath;
    info.isSystemApp = true;
    info.isPrivileged = isPrivileged;

    // Extract package name from APK (simplified - in real implementation, parse AndroidManifest.xml)
    // For now, use filename as package name approximation
    std::string filename = fs::path(apkPath).stem().string();
    info.packageName = filename;

    // Analyze signature
    info.signatureInfo = analyzeApk(apkPath);

    // Version info would require parsing APK manifest - placeholder
    info.versionName = "Unknown";
    info.versionCode = "Unknown";

    return info;
}

void AndroidAnalyzer::analyzeBuildProperties(const std::string& buildPropPath) {
    if (!fs::exists(buildPropPath)) {
        std::cout << "Build properties file not found: " << buildPropPath << std::endl;
        return;
    }

    std::ifstream file(buildPropPath);
    std::string line;
    std::cout << "Build Properties:" << std::endl;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            std::cout << line << std::endl;
            
            // Parse key=value format
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);
                
                // Store in database
                SystemBuildProperty prop{key, value};
                if (!systemDb_->insertBuildProperty(prop)) {
                    std::cerr << "Failed to insert build property: " << key << std::endl;
                }
            }
        }
    }
}

void AndroidAnalyzer::scanFrameworkDirectory(const std::string& frameworkPath) {
    if (!fs::exists(frameworkPath) || !fs::is_directory(frameworkPath)) {
        std::cout << "Framework directory not found: " << frameworkPath << std::endl;
        return;
    }

    std::cout << "Framework files:" << std::endl;
    for (const auto& entry : fs::directory_iterator(frameworkPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".jar" || ext == ".dex" || ext == ".so") {
                std::cout << entry.path().filename().string() << std::endl;
            }
        }
    }
}

ApkSignatureInfo AndroidAnalyzer::analyzeApk(const std::string& apkPath) {
    ApkSignatureInfo info;
    info.apkPath = apkPath;
    info.hasSignature = false; // Simplified - in real implementation, check for signature
    info.signerName = "Unknown";
    info.certificateFingerprint = "Unknown";
    // TODO: Implement actual APK signature analysis
    return info;
}

void AndroidAnalyzer::extractAndScanSystemApps(const std::string& imageAppDir, const std::string& tempAppDir) {
    // For now, we'll extract known system APK files
    // In a full implementation, we would scan the directory first
    std::vector<std::string> knownSystemApks = {
        "system/app/SystemUI.apk",
        "system/priv-app/Settings.apk"
    };

    fs::create_directories(tempAppDir);

    for (const auto& apkPath : knownSystemApks) {
        std::string tempApkPath = tempAppDir + "/" + fs::path(apkPath).filename().string();
        if (fileExtractor_->extractFileByPath(apkPath, tempApkPath)) {
            bool isPrivileged = (apkPath.find("priv-app") != std::string::npos);
            SystemAppInfo appInfo = analyzeSystemApk(tempApkPath, isPrivileged);
            if (!appInfo.packageName.empty()) {
                std::cout << "System App: " << appInfo.packageName << " at " << apkPath << std::endl;
                
                // Store in database
                SystemAppRecord appRecord{
                    appInfo.packageName,
                    apkPath,
                    appInfo.versionName,
                    appInfo.versionCode,
                    appInfo.isSystemApp,
                    appInfo.isPrivileged
                };
                if (!systemDb_->insertSystemApp(appRecord)) {
                    std::cerr << "Failed to insert system app: " << appInfo.packageName << std::endl;
                }
            }
        } else {
            std::cout << "System APK not found: " << apkPath << std::endl;
        }
    }
}

void AndroidAnalyzer::extractAndScanFramework(const std::string& imageFrameworkDir, const std::string& tempFrameworkDir) {
    // Extract known framework files
    std::vector<std::string> knownFrameworkFiles = {
        "system/framework/framework.jar",
        "system/framework/framework.dex",
        "system/framework/framework.so"
    };

    fs::create_directories(tempFrameworkDir);
    std::cout << "Framework files:" << std::endl;

    for (const auto& frameworkFile : knownFrameworkFiles) {
        std::string tempFilePath = tempFrameworkDir + "/" + fs::path(frameworkFile).filename().string();
        if (fileExtractor_->extractFileByPath(frameworkFile, tempFilePath)) {
            std::string fileName = fs::path(frameworkFile).filename().string();
            std::cout << fileName << std::endl;
            
            // Get file size
            int64_t fileSize = 0;
            if (fs::exists(tempFilePath)) {
                fileSize = fs::file_size(tempFilePath);
            }
            
            // Determine file type
            std::string fileType;
            if (fileName.find(".jar") != std::string::npos) {
                fileType = "jar";
            } else if (fileName.find(".dex") != std::string::npos) {
                fileType = "dex";
            } else if (fileName.find(".so") != std::string::npos) {
                fileType = "so";
            } else {
                fileType = "unknown";
            }
            
            // Store in database
            FrameworkFileRecord fileRecord{fileName, frameworkFile, fileType, fileSize};
            if (!systemDb_->insertFrameworkFile(fileRecord)) {
                std::cerr << "Failed to insert framework file: " << fileName << std::endl;
            }
        }
    }
}

// SystemAnalysisDatabase implementation
SystemAnalysisDatabase::SystemAnalysisDatabase(const std::string& dbPath)
    : dbPath_(dbPath), db_(nullptr) {
}

SystemAnalysisDatabase::~SystemAnalysisDatabase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool SystemAnalysisDatabase::initialize() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open system analysis database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return createTables();
}

bool SystemAnalysisDatabase::createTables() {
    // Build properties table
    std::string createBuildPropsTable = R"(
        CREATE TABLE IF NOT EXISTS system_build_properties (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            property_key TEXT NOT NULL,
            property_value TEXT,
            UNIQUE(property_key)
        );
    )";

    if (!executeSQL(createBuildPropsTable)) {
        return false;
    }

    // System apps table
    std::string createSystemAppsTable = R"(
        CREATE TABLE IF NOT EXISTS system_apps (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            package_name TEXT NOT NULL,
            apk_path TEXT NOT NULL,
            version_name TEXT,
            version_code TEXT,
            is_system_app INTEGER DEFAULT 1,
            is_privileged INTEGER DEFAULT 0,
            UNIQUE(package_name, apk_path)
        );
    )";

    if (!executeSQL(createSystemAppsTable)) {
        return false;
    }

    // Framework files table
    std::string createFrameworkTable = R"(
        CREATE TABLE IF NOT EXISTS framework_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_type TEXT,
            file_size INTEGER,
            UNIQUE(file_path)
        );
    )";

    if (!executeSQL(createFrameworkTable)) {
        return false;
    }

    return true;
}

bool SystemAnalysisDatabase::insertBuildProperty(const SystemBuildProperty& prop) {
    const char* sql = R"(
        INSERT OR REPLACE INTO system_build_properties (property_key, property_value)
        VALUES (?, ?);
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare build property statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, prop.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, prop.value.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SystemAnalysisDatabase::insertSystemApp(const SystemAppRecord& app) {
    const char* sql = R"(
        INSERT OR IGNORE INTO system_apps (package_name, apk_path, version_name, version_code, is_system_app, is_privileged)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare system app statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, app.packageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, app.apkPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, app.versionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, app.versionCode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, app.isSystemApp ? 1 : 0);
    sqlite3_bind_int(stmt, 6, app.isPrivileged ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SystemAnalysisDatabase::insertFrameworkFile(const FrameworkFileRecord& file) {
    const char* sql = R"(
        INSERT OR IGNORE INTO framework_files (file_name, file_path, file_type, file_size)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare framework file statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, file.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, file.fileType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, file.fileSize);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool SystemAnalysisDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}