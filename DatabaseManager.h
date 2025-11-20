#pragma once
#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <sqlite3.h>
#include <memory>
#include <vector>
#include <tsk/libtsk.h>

struct FileRecord {
    int64_t inode;
    std::string name;
    std::string path;
    int64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    int64_t crtime;
    std::string type;
    std::string md5;
    int isDeleted;
    int isAllocated;
    std::string permissions;
    int uid;
    int gid;
};

struct EventRecord {
    int64_t timestamp;
    std::string eventType;
    std::string filePath;
    int64_t inode;
    std::string description;
};

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();

    bool initialize();
    bool insertFileRecord(const FileRecord& record);
    bool insertEventRecord(const EventRecord& record);
    bool insertPartitionInfo(int partNum, int64_t start, int64_t length,
        const std::string& desc, const std::string& fsType);

    sqlite3* getDb() const { return db_; }
    const std::string& getDbPath() const { return dbPath_; }

private:
    std::string dbPath_;
    sqlite3* db_;

    bool createTables();
    bool executeSQL(const std::string& sql);
};

#endif // DATABASE_MANAGER_H

