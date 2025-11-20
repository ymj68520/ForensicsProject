#pragma once
#ifndef FILE_CLASSIFIER_H
#define FILE_CLASSIFIER_H

#include <string>
#include <sqlite3.h>
#include <unordered_map>
#include <vector>
#include <set>

enum class FileCategory {
    IMAGE,
    VIDEO,
    AUDIO,
    DOCUMENT,
    ARCHIVE,
    EXECUTABLE,
    DATABASE,
    SOURCE_CODE,
    WEB,
    EMAIL,
    SYSTEM,
    ENCRYPTED,
    UNKNOWN
};

class FileClassifier {
public:
    explicit FileClassifier(const std::string& sourceDbPath,
        const std::string& fileDbPath);
    ~FileClassifier();

    bool classifyAndExtract();

private:
    std::string sourceDbPath_;
    std::string fileDbPath_;
    sqlite3* sourceDb_;
    sqlite3* fileDb_;

    std::unordered_map<std::string, FileCategory> extensionMap_;

    bool openDatabases();
    bool createCategoryTables();
    bool classifyFiles();
    FileCategory determineCategory(const std::string& filename,
        const std::string& type);
    std::string getCategoryName(FileCategory category);
    std::string getCategoryTableName(FileCategory category);
    void initializeExtensionMap();
    void closeDatabases();
};

#endif // FILE_CLASSIFIER_H


