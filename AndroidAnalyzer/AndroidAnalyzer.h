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
#include "../sqlite3/sqlite3.h"

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

class AndroidAnalyzer {
public:
    AndroidAnalyzer();
    ~AndroidAnalyzer();

    // Main entry point to analyze a directory (mounted image or extracted backup)
    void analyze(const std::string& rootPath);

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
};

#include <sqlite3.h>
#include <filesystem>
#include <tsk/libtsk.h>
#include "../DatabaseManager/DatabaseManager.h"
#include "../DatabaseManager/FileExtractor/FileExtractor.h"

struct AndroidAppData {
    std::string packageName;
    std::string dbPath;
    std::string dataType; // e.g., "SMS", "Contacts", "CallLog"
    std::vector<std::map<std::string, std::string>> records;
};

class AndroidAnalyzer {
public:
    AndroidAnalyzer(const std::string& imagePath, DatabaseManager* dbManager);
    ~AndroidAnalyzer();

    bool initialize();
    void analyzeAndroidData();
    void parseSMS(const std::string& dbPath);
    void parseContacts(const std::string& dbPath);
    void parseCallLog(const std::string& dbPath);
    void parseWhatsApp(const std::string& dbPath);
    void parseGenericAppData(const std::string& packageName, const std::string& dbPath);

private:
    std::string imagePath_;
    DatabaseManager* dbManager_;
    std::unique_ptr<FileExtractor> fileExtractor_;
    std::vector<AndroidAppData> appDataList_;

    bool extractAndParseDB(const std::string& dbPathInImage, const std::string& tempPath);
    void insertParsedData(const AndroidAppData& data);
};