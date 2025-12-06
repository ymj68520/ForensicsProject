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

    // Initialize android analysis database
    std::string androidDbPath = outputDbPath_.empty() ? imagePath_ + "_android.db" : outputDbPath_;
    androidDb_ = std::make_unique<AndroidAnalysisDatabase>(androidDbPath);
    if (!androidDb_->initialize()) {
        std::cerr << "Failed to initialize AndroidAnalysisDatabase" << std::endl;
        return false;
    }

    return true;
}

void AndroidAnalyzer::analyzeAndroidData() {
    std::cout << "Starting Android data analysis..." << std::endl;

    // Analyze system directory
    analyzeSystemDirectory(imagePath_ + "/system");
    
    // Analyze installed packages
    parseInstalledPackages("data/system/packages.xml");
    
    // Analyze Usage Stats
    parseUsageStats("data/system/usagestats/daily");
    
    // Analyze WiFi Config
    // Try modern XML first, then legacy conf
    // We try to extract both, parseWifiConfig handles extraction failure gracefully
    parseWifiConfig("data/misc/wifi/WifiConfigStore.xml");
    parseWifiConfig("data/misc/wifi/wpa_supplicant.conf");

    // Define known Android app data paths
    std::vector<std::pair<std::string, std::string>> knownApps = {
        {"data/data/com.android.providers.telephony/databases/mmssms.db", "SMS"},
        {"data/data/com.android.providers.contacts/databases/contacts2.db", "Contacts"},
        {"data/data/com.android.providers.contacts/databases/calllog.db", "CallLog"},
        {"data/data/com.whatsapp/databases/msgstore.db", "WhatsApp"},
        {"data/data/org.telegram.messenger/files/cache4.db", "Telegram"},
        {"data/data/com.tencent.mm/MicroMsg/testuser/EnMicroMsg.db", "WeChat"},
        {"data/data/com.android.chrome/app_chrome/Default/History", "Chrome"}
    };

    for (const auto& app : knownApps) {
        std::string tempPath = "/tmp/" + std::filesystem::path(app.first).filename().string();
        // For WeChat/Telegram we might have multiple matches if using wildcards, but here we use fixed paths for the demo
        if (extractAndParseDB(app.first, tempPath)) {
            if (app.second == "SMS") {
                parseSMS(tempPath);
            } else if (app.second == "Contacts") {
                parseContacts(tempPath);
            } else if (app.second == "CallLog") {
                parseCallLog(tempPath);
            } else if (app.second == "WhatsApp") {
                parseWhatsApp(tempPath);
            } else if (app.second == "Telegram") {
                parseTelegram(tempPath);
            } else if (app.second == "WeChat") {
                parseWeChat(tempPath);
            } else if (app.second == "Chrome") {
                parseChromeHistory(tempPath);
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
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return;

    std::string sql = "SELECT _id, address, body, date, type FROM sms;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["address"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["body"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            androidDb_->insertSMS(record);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

void AndroidAnalyzer::parseContacts(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return;

    std::string sql = "SELECT raw_contacts._id, display_name, data1 FROM raw_contacts JOIN data ON raw_contacts._id = data.raw_contact_id WHERE data.mimetype_id = (SELECT _id FROM mimetypes WHERE mimetype = 'vnd.android.cursor.item/phone_v2');";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int rowCount = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["display_name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["phone"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            androidDb_->insertContact(record);
            rowCount++;
        }
        if (rowCount == 0) {
            std::cout << "No contacts found matching query in " << dbPath << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Failed to prepare Contacts SQL: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
}

void AndroidAnalyzer::parseCallLog(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return;

    std::string sql = "SELECT _id, number, date, duration, type FROM calls;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["number"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 2));
            record["duration"] = std::to_string(sqlite3_column_int(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            androidDb_->insertCallLog(record);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

std::vector<ChatMessage> AndroidAnalyzer::parseWhatsApp(const std::string& dbPath) {
    std::vector<ChatMessage> messages;
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return messages;

    std::string sql = "SELECT _id, key_remote_jid, data, timestamp FROM messages LIMIT 1000;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage msg;
            msg.sender = "Unknown"; 
            const char* receiver = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.receiver = receiver ? receiver : "";
            const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            msg.content = content ? content : "";
            msg.timestamp = std::to_string(sqlite3_column_int64(stmt, 3));
            msg.appName = "WhatsApp";
            messages.push_back(msg);
            androidDb_->insertWhatsAppMessage(msg);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return messages;
}

std::vector<ChatMessage> AndroidAnalyzer::parseTelegram(const std::string& dbPath) {
    std::vector<ChatMessage> messages;
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return messages;

    // Simulation: Telegram schema varies, using a simplified query for cache4.db or similar
    // Assuming table 'messages' with fields _id, uid, data, date
    std::string sql = "SELECT _id, uid, data, date FROM messages LIMIT 1000;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage msg;
            const char* sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.sender = sender ? sender : "Unknown";
            msg.receiver = "Me"; // Simplified
            const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            msg.content = content ? content : "";
            msg.timestamp = std::to_string(sqlite3_column_int64(stmt, 3));
            msg.appName = "Telegram";
            messages.push_back(msg);
            androidDb_->insertTelegramMessage(msg);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return messages;
}

std::vector<ChatMessage> AndroidAnalyzer::parseWeChat(const std::string& dbPath) {
    std::vector<ChatMessage> messages;
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return messages;

    // Simulation: WeChat EnMicroMsg.db is typically encrypted SQLCipher.
    // Assuming we are dealing with a decrypted DB or a simulation with 'message' table.
    std::string sql = "SELECT msgId, content, createTime, talker FROM message LIMIT 1000;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage msg;
            const char* sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            msg.sender = sender ? sender : "Unknown";
            msg.receiver = "Me";
            const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            msg.content = content ? content : "";
            msg.timestamp = std::to_string(sqlite3_column_int64(stmt, 2));
            msg.appName = "WeChat";
            messages.push_back(msg);
            androidDb_->insertWeChatMessage(msg);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return messages;
}

void AndroidAnalyzer::parseChromeHistory(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return;

    std::string sql = "SELECT url, title, visit_count, last_visit_time FROM urls;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChromeHistoryItem item;
            const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            item.url = url ? url : "";
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            item.title = title ? title : "";
            item.visitCount = sqlite3_column_int64(stmt, 2);
            item.lastVisitTime = sqlite3_column_int64(stmt, 3);
            androidDb_->insertChromeHistory(item);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

void AndroidAnalyzer::parseWifiConfig(const std::string& configPath) {
    std::string tempPath = "/tmp/wifi_config_" + std::to_string(std::time(nullptr));
    if (!fileExtractor_->extractFileByPath(configPath, tempPath)) return;

    std::ifstream file(tempPath);
    std::string line;
    std::string ssid, psk, keyMgmt;
    
    bool isXml = (configPath.find(".xml") != std::string::npos);

    if (isXml) {
        // Simple XML scraping for WifiConfigStore.xml
        while (std::getline(file, line)) {
            if (line.find("name=\"SSID\"") != std::string::npos) {
                size_t start = line.find(">");
                size_t end = line.find("</");
                if (start != std::string::npos && end != std::string::npos) {
                    ssid = line.substr(start + 1, end - start - 1);
                    // Remove extra quotes if present (&quot;)
                    if (ssid.find("&quot;") != std::string::npos) {
                        ssid = std::regex_replace(ssid, std::regex("&quot;"), "");
                    }
                }
            } else if (line.find("name=\"PreSharedKey\"") != std::string::npos) {
                size_t start = line.find(">");
                size_t end = line.find("</");
                if (start != std::string::npos && end != std::string::npos) {
                    psk = line.substr(start + 1, end - start - 1);
                    if (psk.find("&quot;") != std::string::npos) {
                        psk = std::regex_replace(psk, std::regex("&quot;"), "");
                    }
                }
            } else if (line.find("name=\"KeyMgmt\"") != std::string::npos) {
                size_t start = line.find(">");
                size_t end = line.find("</");
                if (start != std::string::npos && end != std::string::npos) {
                    keyMgmt = line.substr(start + 1, end - start - 1);
                }
            }
            
            if (!ssid.empty() && (!psk.empty() || !keyMgmt.empty())) {
                WifiNetwork net{ssid, psk, keyMgmt};
                androidDb_->insertWifiNetwork(net);
                ssid = ""; psk = ""; keyMgmt = "";
            }
        }
    } else {
        // wpa_supplicant.conf parser
        while (std::getline(file, line)) {
            if (line.find("ssid=") != std::string::npos) {
                ssid = line.substr(line.find("=") + 1);
                if (ssid.size() >= 2 && ssid.front() == '"' && ssid.back() == '"') {
                    ssid = ssid.substr(1, ssid.size() - 2);
                }
            } else if (line.find("psk=") != std::string::npos) {
                psk = line.substr(line.find("=") + 1);
            } else if (line.find("key_mgmt=") != std::string::npos) {
                keyMgmt = line.substr(line.find("=") + 1);
            } else if (line.find("}") != std::string::npos) {
                if (!ssid.empty()) {
                    WifiNetwork net{ssid, psk, keyMgmt};
                    androidDb_->insertWifiNetwork(net);
                    ssid = ""; psk = ""; keyMgmt = "";
                }
            }
        }
    }
    std::filesystem::remove(tempPath);
}

void AndroidAnalyzer::parseInstalledPackages(const std::string& xmlPath) {
    std::string tempPath = "/tmp/packages_" + std::to_string(std::time(nullptr)) + ".xml";
    if (!fileExtractor_->extractFileByPath(xmlPath, tempPath)) return;

    std::ifstream file(tempPath);
    std::string line;
    while (std::getline(file, line)) {
        // Very basic XML scraping - look for <package name="..." ...>
        if (line.find("<package") != std::string::npos) {
            auto extractAttr = [&](const std::string& attr) -> std::string {
                size_t pos = line.find(attr + "=\"");
                if (pos == std::string::npos) return "";
                size_t end = line.find("\"", pos + attr.length() + 2);
                return line.substr(pos + attr.length() + 2, end - (pos + attr.length() + 2));
            };

            InstalledPackageInfo pkg;
            pkg.packageName = extractAttr("name");
            pkg.codePath = extractAttr("codePath");
            pkg.version = extractAttr("version");
            pkg.installer = extractAttr("installer");
            
            if (!pkg.packageName.empty()) {
                androidDb_->insertInstalledPackage(pkg);
            }
        }
    }
    std::filesystem::remove(tempPath);
}

void AndroidAnalyzer::parseUsageStats(const std::string& usageStatsPath) {
    std::string tempDir = "/tmp/usagestats_" + std::to_string(std::time(nullptr));
    fs::create_directories(tempDir);

    // Extract simplified usage stats file (simulated as "1001" for daily)
    std::string targetFile = usageStatsPath + "/1001"; 
    std::string tempFile = tempDir + "/1001";
    
    if (fileExtractor_->extractFileByPath(targetFile, tempFile)) {
        std::ifstream file(tempFile);
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("<package") != std::string::npos) {
                auto extractAttr = [&](const std::string& attr) -> std::string {
                    size_t pos = line.find(attr + "=\"");
                    if (pos == std::string::npos) return "";
                    size_t end = line.find("\"", pos + attr.length() + 2);
                    return line.substr(pos + attr.length() + 2, end - (pos + attr.length() + 2));
                };

                UsageStatRecord stat;
                stat.packageName = extractAttr("name");
                try { stat.totalTimeInForeground = std::stoll(extractAttr("timeActive")); } catch (...) { stat.totalTimeInForeground = 0; }
                try { stat.lastTimeUsed = std::stoll(extractAttr("lastTimeActive")); } catch (...) { stat.lastTimeUsed = 0; }
                stat.firstTimeStamp = 0;

                if (!stat.packageName.empty()) {
                    androidDb_->insertUsageStat(stat);
                }
            }
        }
    }
    fs::remove_all(tempDir);
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
                if (!androidDb_->insertBuildProperty(prop)) {
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
                if (!androidDb_->insertSystemApp(appRecord)) {
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
            if (!androidDb_->insertFrameworkFile(fileRecord)) {
                std::cerr << "Failed to insert framework file: " << fileName << std::endl;
            }
        }
    }
}

// AndroidAnalysisDatabase implementation
AndroidAnalysisDatabase::AndroidAnalysisDatabase(const std::string& dbPath)
    : dbPath_(dbPath), db_(nullptr) {
}

AndroidAnalysisDatabase::~AndroidAnalysisDatabase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool AndroidAnalysisDatabase::initialize() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open android analysis database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return createTables();
}

bool AndroidAnalysisDatabase::createTables() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS system_build_properties (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            property_key TEXT NOT NULL UNIQUE,
            property_value TEXT
        );
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
        CREATE TABLE IF NOT EXISTS framework_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            file_path TEXT NOT NULL UNIQUE,
            file_type TEXT,
            file_size INTEGER
        );
        CREATE TABLE IF NOT EXISTS sms_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            thread_id INTEGER,
            address TEXT,
            person TEXT,
            date INTEGER,
            date_sent INTEGER,
            read INTEGER,
            status INTEGER,
            type INTEGER,
            body TEXT,
            service_center TEXT
        );
        CREATE TABLE IF NOT EXISTS contacts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            raw_contact_id INTEGER,
            display_name TEXT,
            phone_number TEXT,
            email TEXT,
            account_type TEXT,
            account_name TEXT
        );
        CREATE TABLE IF NOT EXISTS call_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            number TEXT,
            date INTEGER,
            duration INTEGER,
            type INTEGER,
            name TEXT,
            geocoded_location TEXT
        );
        CREATE TABLE IF NOT EXISTS whatsapp_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender TEXT,
            receiver TEXT,
            content TEXT,
            timestamp INTEGER,
            media_url TEXT,
            media_type TEXT
        );
        CREATE TABLE IF NOT EXISTS telegram_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender TEXT,
            receiver TEXT,
            content TEXT,
            timestamp INTEGER,
            media_url TEXT,
            media_type TEXT
        );
        CREATE TABLE IF NOT EXISTS wechat_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender TEXT,
            receiver TEXT,
            content TEXT,
            timestamp INTEGER,
            media_url TEXT,
            media_type TEXT
        );
        CREATE TABLE IF NOT EXISTS wifi_networks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ssid TEXT NOT NULL,
            pre_shared_key TEXT,
            key_mgmt TEXT,
            last_connected INTEGER
        );
        CREATE TABLE IF NOT EXISTS chrome_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            title TEXT,
            visit_count INTEGER,
            last_visit_time INTEGER,
            typed_count INTEGER
        );
        CREATE TABLE IF NOT EXISTS installed_packages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            package_name TEXT NOT NULL UNIQUE,
            code_path TEXT,
            native_library_path TEXT,
            first_install_time INTEGER,
            last_update_time INTEGER,
            version TEXT,
            installer TEXT
        );
        CREATE TABLE IF NOT EXISTS usage_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            package_name TEXT,
            total_time_foreground INTEGER,
            last_time_used INTEGER,
            interval_start INTEGER
        );
    )";

    return executeSQL(schema);
}

bool AndroidAnalysisDatabase::insertBuildProperty(const SystemBuildProperty& prop) {
    const char* sql = "INSERT OR REPLACE INTO system_build_properties (property_key, property_value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, prop.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, prop.value.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertSystemApp(const SystemAppRecord& app) {
    const char* sql = "INSERT OR IGNORE INTO system_apps (package_name, apk_path, version_name, version_code, is_system_app, is_privileged) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, app.packageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, app.apkPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, app.versionName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, app.versionCode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, app.isSystemApp);
    sqlite3_bind_int(stmt, 6, app.isPrivileged);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertFrameworkFile(const FrameworkFileRecord& file) {
    const char* sql = "INSERT OR IGNORE INTO framework_files (file_name, file_path, file_type, file_size) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, file.fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, file.fileType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, file.fileSize);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertSMS(const std::map<std::string, std::string>& record) {
    const char* sql = "INSERT INTO sms_messages (address, body, date, type) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    
    auto get = [&](const std::string& k) { auto it = record.find(k); return it != record.end() ? it->second.c_str() : ""; };
    
    sqlite3_bind_text(stmt, 1, get("address"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, get("body"), -1, SQLITE_TRANSIENT);
    try { sqlite3_bind_int64(stmt, 3, std::stoll(get("date"))); } catch (...) { sqlite3_bind_int64(stmt, 3, 0); }
    try { sqlite3_bind_int(stmt, 4, std::stoi(get("type"))); } catch (...) { sqlite3_bind_int(stmt, 4, 0); }
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertContact(const std::map<std::string, std::string>& record) {
    const char* sql = "INSERT INTO contacts (display_name, phone_number) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    auto get = [&](const std::string& k) { auto it = record.find(k); return it != record.end() ? it->second.c_str() : ""; };
    sqlite3_bind_text(stmt, 1, get("display_name"), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, get("phone"), -1, SQLITE_TRANSIENT);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertCallLog(const std::map<std::string, std::string>& record) {
    const char* sql = "INSERT INTO call_logs (number, date, duration, type) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    auto get = [&](const std::string& k) { auto it = record.find(k); return it != record.end() ? it->second.c_str() : ""; };
    sqlite3_bind_text(stmt, 1, get("number"), -1, SQLITE_TRANSIENT);
    try { sqlite3_bind_int64(stmt, 2, std::stoll(get("date"))); } catch (...) { sqlite3_bind_int64(stmt, 2, 0); }
    try { sqlite3_bind_int(stmt, 3, std::stoi(get("duration"))); } catch (...) { sqlite3_bind_int(stmt, 3, 0); }
    try { sqlite3_bind_int(stmt, 4, std::stoi(get("type"))); } catch (...) { sqlite3_bind_int(stmt, 4, 0); }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertWhatsAppMessage(const ChatMessage& msg) {
    const char* sql = "INSERT INTO whatsapp_messages (sender, receiver, content, timestamp) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, msg.sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, msg.receiver.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    try { sqlite3_bind_int64(stmt, 4, std::stoll(msg.timestamp)); } catch (...) { sqlite3_bind_int64(stmt, 4, 0); }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertTelegramMessage(const ChatMessage& msg) {
    const char* sql = "INSERT INTO telegram_messages (sender, receiver, content, timestamp) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, msg.sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, msg.receiver.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    try { sqlite3_bind_int64(stmt, 4, std::stoll(msg.timestamp)); } catch (...) { sqlite3_bind_int64(stmt, 4, 0); }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertWeChatMessage(const ChatMessage& msg) {
    const char* sql = "INSERT INTO wechat_messages (sender, receiver, content, timestamp) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, msg.sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, msg.receiver.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    try { sqlite3_bind_int64(stmt, 4, std::stoll(msg.timestamp)); } catch (...) { sqlite3_bind_int64(stmt, 4, 0); }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertWifiNetwork(const WifiNetwork& net) {
    const char* sql = "INSERT INTO wifi_networks (ssid, pre_shared_key, key_mgmt) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, net.ssid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, net.preSharedKey.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, net.keyMgmt.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertChromeHistory(const ChromeHistoryItem& item) {
    const char* sql = "INSERT INTO chrome_history (url, title, visit_count, last_visit_time) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, item.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, item.visitCount);
    sqlite3_bind_int64(stmt, 4, item.lastVisitTime);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertInstalledPackage(const InstalledPackageInfo& pkg) {
    const char* sql = "INSERT OR REPLACE INTO installed_packages (package_name, code_path, version, installer) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, pkg.packageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pkg.codePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pkg.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, pkg.installer.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::insertUsageStat(const UsageStatRecord& stat) {
    const char* sql = "INSERT INTO usage_stats (package_name, total_time_foreground, last_time_used, interval_start) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, stat.packageName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, stat.totalTimeInForeground);
    sqlite3_bind_int64(stmt, 3, stat.lastTimeUsed);
    sqlite3_bind_int64(stmt, 4, stat.firstTimeStamp);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool AndroidAnalysisDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}