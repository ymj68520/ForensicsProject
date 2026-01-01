// WindowsAnalysisDatabase.h
// Database manager for storing Windows forensic analysis results

#pragma once
#ifndef WINDOWS_ANALYSIS_DATABASE_H
#define WINDOWS_ANALYSIS_DATABASE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "WindowsDataTypes.h"

class WindowsAnalysisDatabase {
public:
    explicit WindowsAnalysisDatabase(const std::string& dbPath);
    ~WindowsAnalysisDatabase();

    // Initialize database and create tables
    bool initialize();

    // Registry value operations
    bool insertRegistryValue(const RegistryValue& value);
    bool insertRegistryValues(const std::vector<RegistryValue>& values);
    std::vector<RegistryValue> queryRegistryValues(const std::string& whereClause = "");

    // Event log operations
    bool insertEventLogEntry(const EventLogEntry& entry);
    bool insertEventLogEntries(const std::vector<EventLogEntry>& entries);
    std::vector<EventLogEntry> queryEventLogs(const std::string& whereClause = "");

    // Prefetch file operations
    bool insertPrefetchInfo(const PrefetchInfo& info);
    std::vector<PrefetchInfo> queryPrefetchFiles(const std::string& whereClause = "");

    // LNK file operations
    bool insertLnkFileInfo(const LnkFileInfo& info);
    std::vector<LnkFileInfo> queryLnkFiles(const std::string& whereClause = "");

    // Jump list operations
    bool insertJumpListEntry(const JumpListEntry& entry);
    std::vector<JumpListEntry> queryJumpListEntries(const std::string& whereClause = "");

    // User account operations
    bool insertUserInfo(const WindowsUserInfo& user);
    std::vector<WindowsUserInfo> queryUserAccounts(const std::string& whereClause = "");

    // USB device operations
    bool insertUSBDevice(const USBDeviceInfo& device);
    std::vector<USBDeviceInfo> queryUSBDevices(const std::string& whereClause = "");

    // Recycle bin operations
    bool insertRecycleBinEntry(const RecycleBinEntry& entry);
    std::vector<RecycleBinEntry> queryRecycleBinEntries(const std::string& whereClause = "");

    // Browser artifact operations
    bool insertBrowserArtifact(const BrowserArtifact& artifact);
    std::vector<BrowserArtifact> queryBrowserArtifacts(const std::string& whereClause = "");

    // MFT entry operations
    bool insertMftEntry(const MftEntryInfo& entry);
    std::vector<MftEntryInfo> queryMftEntries(const std::string& whereClause = "");

    // Windows service operations
    bool insertWindowsService(const WindowsServiceInfo& service);
    std::vector<WindowsServiceInfo> queryWindowsServices(const std::string& whereClause = "");

    // Scheduled task operations
    bool insertScheduledTask(const ScheduledTaskInfo& task);
    std::vector<ScheduledTaskInfo> queryScheduledTasks(const std::string& whereClause = "");

    // Amcache entry operations
    bool insertAmcacheEntry(const AmcacheEntry& entry);
    std::vector<AmcacheEntry> queryAmcacheEntries(const std::string& whereClause = "");

    // SRUM entry operations
    bool insertSrumEntry(const SrumEntry& entry);
    std::vector<SrumEntry> querySrumEntries(const std::string& whereClause = "");

    // Transaction management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Get database handle
    sqlite3* getDb() const { return db_; }
    const std::string& getDbPath() const { return dbPath_; }

private:
    std::string dbPath_;
    sqlite3* db_;

    bool createTables();
    bool executeSQL(const std::string& sql);
};

#endif // WINDOWS_ANALYSIS_DATABASE_H
