// AndroidDataTypes.h
// Common data types and structures for Android analysis

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "fileSystem.h"

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

struct WifiNetwork {
    std::string ssid;
    std::string preSharedKey;
    std::string keyMgmt;
};

struct ChromeHistoryItem {
    std::string url;
    std::string title;
    int64_t visitCount;
    int64_t lastVisitTime;
};

struct InstalledPackageInfo {
    std::string packageName;
    std::string codePath;
    std::string nativeLibraryPath;
    int64_t firstInstallTime;
    int64_t lastUpdateTime;
    std::string version;
    std::string installer;
};

struct UsageStatRecord {
    std::string packageName;
    int64_t totalTimeInForeground;
    int64_t lastTimeUsed;
    int64_t firstTimeStamp;
};
