// AndroidAnalyzer.h
// 常用 C++ 头文件集合，供项目中常见类型和工具使用
#pragma once

// IO、字符串与流
#include <iostream>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <iomanip>

// 容器与迭代器
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <array>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <queue>
#include <stack>
#include <tuple>
#include <utility>
#include <iterator>

// 字符串处理与正则
#include <string>
#include <cstring>
#include <cctype>
#include <regex>

// 算法与函数对象
#include <algorithm>
#include <functional>
#include <numeric>
#include <filesystem>
#include <sqlite3.h>
#include <tsk/libtsk.h>

#include "../DatabaseManager/DatabaseManager.h"
#include "../DatabaseManager/FileExtractor/FileExtractor.h"

namespace fs = std::filesystem;

struct AppData {
    std::string packageName;
    std::string installPath;
    std::vector<std::string> dbFiles;
};

struct ChatMessage {
    std::string sender;
    std::string receiver;
    std::string content;
    std::string timestamp;
    std::string appName;
};

struct ApkSignatureInfo {
    std::string apkPath;
    bool hasSignature;
    std::string signerName; // Simplified for this example
    std::string certificateFingerprint;
};

struct SystemAppInfo {
    std::string packageName;
    std::string apkPath;
    std::string versionName;
    std::string versionCode;
    ApkSignatureInfo signatureInfo;
    bool isSystemApp;
    bool isPrivileged;
};

struct AndroidAppData {
    std::string packageName;
    std::string dbPath;
    std::string dataType; // e.g., "SMS", "Contacts", "CallLog"
    std::vector<std::map<std::string, std::string>> records;
};

struct SystemBuildProperty {
    std::string key;
    std::string value;
};

struct SystemAppRecord {
    std::string packageName;
    std::string apkPath;
    std::string versionName;
    std::string versionCode;
    bool isSystemApp;
    bool isPrivileged;
};

struct FrameworkFileRecord {
    std::string fileName;
    std::string filePath;
    std::string fileType; // jar, dex, so, etc.
    int64_t fileSize;
};

class SystemAnalysisDatabase {
public:
    explicit SystemAnalysisDatabase(const std::string& dbPath);
    ~SystemAnalysisDatabase();

    bool initialize();
    bool insertBuildProperty(const SystemBuildProperty& prop);
    bool insertSystemApp(const SystemAppRecord& app);
    bool insertFrameworkFile(const FrameworkFileRecord& file);

private:
    std::string dbPath_;
    sqlite3* db_;

    bool createTables();
    bool executeSQL(const std::string& sql);
};

class AndroidAnalyzer {
public:
    AndroidAnalyzer();
    AndroidAnalyzer(const std::string& imagePath, DatabaseManager* dbManager);
    ~AndroidAnalyzer();

    // Main entry point to analyze a directory (mounted image or extracted backup)
    void analyze(const std::string& rootPath);

    // Android specific analysis
    bool initialize();
    void analyzeAndroidData();
    void analyzeSystemDirectory(const std::string& systemPath);

    // Specific analyzers
    std::vector<ChatMessage> parseWhatsApp(const std::string& dbPath);
    std::vector<ChatMessage> parseWeChat(const std::string& dbPath);
    
    // APK Analysis
    ApkSignatureInfo analyzeApk(const std::string& apkPath);

private:
    void scanUserData(const std::string& dataPath);
    void processAppDirectory(const std::string& appPath);
    bool isSQLiteDatabase(const std::string& filePath);
    
    // Helper to execute SQL query
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& dbPath, const std::string& query);

    // Android data parsing methods
    bool extractAndParseDB(const std::string& dbPathInImage, const std::string& tempPath);
    void parseSMS(const std::string& dbPath);
    void parseContacts(const std::string& dbPath);
    void parseCallLog(const std::string& dbPath);
    void parseGenericAppData(const std::string& packageName, const std::string& dbPath);
    void insertParsedData(const AndroidAppData& data);

    // System analysis methods
    void scanSystemApps(const std::string& appDirPath);
    SystemAppInfo analyzeSystemApk(const std::string& apkPath, bool isPrivileged);
    void analyzeBuildProperties(const std::string& buildPropPath);
    void scanFrameworkDirectory(const std::string& frameworkPath);
    void extractAndScanSystemApps(const std::string& imageAppDir, const std::string& tempAppDir);
    void extractAndScanFramework(const std::string& imageFrameworkDir, const std::string& tempFrameworkDir);

    // Private members
    std::string imagePath_;
    DatabaseManager* dbManager_;
    std::unique_ptr<FileExtractor> fileExtractor_;
    std::unique_ptr<SystemAnalysisDatabase> systemDb_;
};