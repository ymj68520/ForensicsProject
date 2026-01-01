// WindowsAnalysisDatabase.cpp
// Implementation of WindowsAnalysisDatabase

#include "WindowsAnalysisDatabase.h"
#include <iostream>
#include <sstream>

WindowsAnalysisDatabase::WindowsAnalysisDatabase(const std::string& dbPath)
    : dbPath_(dbPath), db_(nullptr) {
}

WindowsAnalysisDatabase::~WindowsAnalysisDatabase() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool WindowsAnalysisDatabase::initialize() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return createTables();
}

bool WindowsAnalysisDatabase::createTables() {
    const char* sql = R"(
        -- Registry Values
        CREATE TABLE IF NOT EXISTS registry_values (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            hive_path TEXT,
            hive_type TEXT,
            key_path TEXT,
            value_name TEXT,
            value_type TEXT,
            value_data TEXT,
            last_modified INTEGER,
            forensic_importance TEXT
        );

        -- Event Log Entries
        CREATE TABLE IF NOT EXISTS event_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            record_id INTEGER,
            log_source TEXT,
            event_id INTEGER,
            level TEXT,
            timestamp INTEGER,
            source TEXT,
            message TEXT,
            computer_name TEXT,
            user_sid TEXT,
            channel TEXT
        );

        -- Prefetch Files
        CREATE TABLE IF NOT EXISTS prefetch_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT,
            executable_name TEXT,
            executable_path TEXT,
            prefetch_hash TEXT,
            run_count INTEGER,
            last_run_time INTEGER,
            creation_time INTEGER,
            referenced_files TEXT,
            referenced_directories TEXT
        );

        -- LNK Files
        CREATE TABLE IF NOT EXISTS lnk_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            lnk_path TEXT,
            target_path TEXT,
            working_directory TEXT,
            arguments TEXT,
            icon_location TEXT,
            creation_time INTEGER,
            modification_time INTEGER,
            access_time INTEGER,
            target_size INTEGER,
            drive_type TEXT,
            volume_serial TEXT,
            netbios_name TEXT,
            relative_path TEXT,
            description TEXT
        );

        -- Jump List Entries
        CREATE TABLE IF NOT EXISTS jump_list_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            app_id TEXT,
            entry_path TEXT,
            entry_name TEXT,
            access_time INTEGER,
            creation_time INTEGER,
            access_count INTEGER,
            is_pinned INTEGER
        );

        -- User Accounts
        CREATE TABLE IF NOT EXISTS user_accounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rid INTEGER,
            username TEXT,
            full_name TEXT,
            comment TEXT,
            last_login INTEGER,
            password_last_set INTEGER,
            account_expires INTEGER,
            password_expires INTEGER,
            account_flags TEXT,
            is_admin INTEGER,
            home_directory TEXT,
            profile_path TEXT
        );

        -- USB Devices
        CREATE TABLE IF NOT EXISTS usb_devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vendor_id TEXT,
            product_id TEXT,
            serial_number TEXT,
            device_description TEXT,
            friendly_name TEXT,
            device_class TEXT,
            first_connected INTEGER,
            last_connected INTEGER,
            last_drive_letter TEXT
        );
        
        -- Recycle Bin
        CREATE TABLE IF NOT EXISTS recycle_bin (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            recycle_file_path TEXT,
            original_path TEXT,
            file_name TEXT,
            deletion_time INTEGER,
            original_size INTEGER,
            user_sid TEXT
        );

        -- Browser Artifacts
        CREATE TABLE IF NOT EXISTS browser_artifacts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            browser_name TEXT,
            artifact_type TEXT,
            url TEXT,
            title TEXT,
            timestamp INTEGER,
            visit_count INTEGER,
            local_path TEXT,
            file_size INTEGER
        );

        -- MFT Entries
        CREATE TABLE IF NOT EXISTS mft_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            entry_number INTEGER,
            file_name TEXT,
            file_path TEXT,
            parent_entry INTEGER,
            logical_size INTEGER,
            physical_size INTEGER,
            creation_time INTEGER,
            modification_time INTEGER,
            access_time INTEGER,
            mft_modification_time INTEGER,
            fn_creation_time INTEGER,
            fn_modification_time INTEGER,
            is_directory INTEGER,
            is_deleted INTEGER,
            has_ads INTEGER,
            permissions TEXT
        );

        -- Windows Services
        CREATE TABLE IF NOT EXISTS windows_services (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            service_name TEXT,
            display_name TEXT,
            image_path TEXT,
            start_type TEXT,
            service_type TEXT,
            account_name TEXT,
            description TEXT,
            is_running INTEGER
        );

        -- Scheduled Tasks
        CREATE TABLE IF NOT EXISTS scheduled_tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_name TEXT,
            task_path TEXT,
            author TEXT,
            description TEXT,
            action_type TEXT,
            action_path TEXT,
            arguments TEXT,
            trigger_type TEXT,
            last_run_time INTEGER,
            next_run_time INTEGER,
            status TEXT,
            run_as TEXT
        );

        -- Amcache Entries
        CREATE TABLE IF NOT EXISTS amcache_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT,
            file_hash TEXT,
            file_name TEXT,
            company_name TEXT,
            product_name TEXT,
            product_version TEXT,
            file_description TEXT,
            file_size INTEGER,
            link_time INTEGER,
            last_modified INTEGER,
            language TEXT
        );

        -- SRUM Entries
        CREATE TABLE IF NOT EXISTS srum_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            app_name TEXT,
            user_name TEXT,
            timestamp INTEGER,
            bytes_received INTEGER,
            bytes_sent INTEGER,
            foreground_duration INTEGER,
            background_duration INTEGER,
            cpu_time_ms INTEGER
        );
    )";

    return executeSQL(sql);
}

bool WindowsAnalysisDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool WindowsAnalysisDatabase::beginTransaction() {
    return executeSQL("BEGIN TRANSACTION;");
}

bool WindowsAnalysisDatabase::commitTransaction() {
    return executeSQL("COMMIT;");
}

bool WindowsAnalysisDatabase::rollbackTransaction() {
    return executeSQL("ROLLBACK;");
}

// Helper macro for binding text
#define BIND_TEXT(stmt, index, text) \
    sqlite3_bind_text(stmt, index, text.c_str(), -1, SQLITE_TRANSIENT)

// Helper macro for binding int64
#define BIND_INT64(stmt, index, val) \
    sqlite3_bind_int64(stmt, index, val)

// Helper macro for binding int
#define BIND_INT(stmt, index, val) \
    sqlite3_bind_int(stmt, index, val)

bool WindowsAnalysisDatabase::insertRegistryValue(const RegistryValue& value) {
    const char* sql = "INSERT INTO registry_values (hive_path, hive_type, key_path, value_name, value_type, value_data, last_modified, forensic_importance) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, value.hivePath);
    BIND_TEXT(stmt, 2, value.hiveType);
    BIND_TEXT(stmt, 3, value.keyPath);
    BIND_TEXT(stmt, 4, value.valueName);
    BIND_TEXT(stmt, 5, value.valueType);
    BIND_TEXT(stmt, 6, value.valueData);
    BIND_INT64(stmt, 7, value.lastModified);
    BIND_TEXT(stmt, 8, value.forensicImportance);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertEventLogEntry(const EventLogEntry& entry) {
    const char* sql = "INSERT INTO event_logs (record_id, log_source, event_id, level, timestamp, source, message, computer_name, user_sid, channel) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_INT64(stmt, 1, entry.recordId);
    BIND_TEXT(stmt, 2, entry.logSource);
    BIND_INT(stmt, 3, entry.eventId);
    BIND_TEXT(stmt, 4, entry.level);
    BIND_INT64(stmt, 5, entry.timestamp);
    BIND_TEXT(stmt, 6, entry.source);
    BIND_TEXT(stmt, 7, entry.message);
    BIND_TEXT(stmt, 8, entry.computerName);
    BIND_TEXT(stmt, 9, entry.userSid);
    BIND_TEXT(stmt, 10, entry.channel);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertPrefetchInfo(const PrefetchInfo& info) {
    const char* sql = "INSERT INTO prefetch_files (file_path, executable_name, executable_path, prefetch_hash, run_count, last_run_time, creation_time, referenced_files, referenced_directories) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, info.filePath);
    BIND_TEXT(stmt, 2, info.executableName);
    BIND_TEXT(stmt, 3, info.executablePath);
    BIND_TEXT(stmt, 4, info.prefetchHash);
    BIND_INT(stmt, 5, info.runCount);
    BIND_INT64(stmt, 6, info.lastRunTime);
    BIND_INT64(stmt, 7, info.creationTime);

    // Convert vectors to comma-separated strings
    std::string refFilesStr, refDirsStr;
    for (size_t i = 0; i < info.referencedFiles.size(); i++) {
        if (i > 0) refFilesStr += ",";
        refFilesStr += info.referencedFiles[i];
    }
    for (size_t i = 0; i < info.referencedDirectories.size(); i++) {
        if (i > 0) refDirsStr += ",";
        refDirsStr += info.referencedDirectories[i];
    }

    BIND_TEXT(stmt, 8, refFilesStr);
    BIND_TEXT(stmt, 9, refDirsStr);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertLnkFileInfo(const LnkFileInfo& info) {
    const char* sql = "INSERT INTO lnk_files (lnk_path, target_path, working_directory, arguments, icon_location, creation_time, modification_time, access_time, target_size, drive_type, volume_serial, netbios_name, relative_path, description) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, info.lnkPath);
    BIND_TEXT(stmt, 2, info.targetPath);
    BIND_TEXT(stmt, 3, info.workingDirectory);
    BIND_TEXT(stmt, 4, info.arguments);
    BIND_TEXT(stmt, 5, info.iconLocation);
    BIND_INT64(stmt, 6, info.creationTime);
    BIND_INT64(stmt, 7, info.modificationTime);
    BIND_INT64(stmt, 8, info.accessTime);
    BIND_INT64(stmt, 9, info.targetSize);
    BIND_TEXT(stmt, 10, info.driveType);
    BIND_TEXT(stmt, 11, info.volumeSerial);
    BIND_TEXT(stmt, 12, info.netBiosName);
    BIND_TEXT(stmt, 13, info.relativePath);
    BIND_TEXT(stmt, 14, info.description);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertJumpListEntry(const JumpListEntry& entry) {
    const char* sql = "INSERT INTO jump_list_entries (app_id, entry_path, entry_name, access_time, creation_time, access_count, is_pinned) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, entry.appId);
    BIND_TEXT(stmt, 2, entry.entryPath);
    BIND_TEXT(stmt, 3, entry.entryName);
    BIND_INT64(stmt, 4, entry.accessTime);
    BIND_INT64(stmt, 5, entry.creationTime);
    BIND_INT(stmt, 6, entry.accessCount);
    BIND_INT(stmt, 7, entry.isPinned ? 1 : 0);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertUserInfo(const WindowsUserInfo& user) {
    const char* sql = "INSERT INTO user_accounts (rid, username, full_name, comment, last_login, password_last_set, account_expires, password_expires, account_flags, is_admin, home_directory, profile_path) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_INT(stmt, 1, user.rid);
    BIND_TEXT(stmt, 2, user.username);
    BIND_TEXT(stmt, 3, user.fullName);
    BIND_TEXT(stmt, 4, user.comment);
    BIND_INT64(stmt, 5, user.lastLogin);
    BIND_INT64(stmt, 6, user.passwordLastSet);
    BIND_INT64(stmt, 7, user.accountExpires);
    BIND_INT64(stmt, 8, user.passwordExpires);
    BIND_TEXT(stmt, 9, user.accountFlags);
    BIND_INT(stmt, 10, user.isAdmin ? 1 : 0);
    BIND_TEXT(stmt, 11, user.homeDirectory);
    BIND_TEXT(stmt, 12, user.profilePath);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertUSBDevice(const USBDeviceInfo& device) {
    const char* sql = "INSERT INTO usb_devices (vendor_id, product_id, serial_number, device_description, friendly_name, device_class, first_connected, last_connected, last_drive_letter) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, device.vendorId);
    BIND_TEXT(stmt, 2, device.productId);
    BIND_TEXT(stmt, 3, device.serialNumber);
    BIND_TEXT(stmt, 4, device.deviceDescription);
    BIND_TEXT(stmt, 5, device.friendlyName);
    BIND_TEXT(stmt, 6, device.deviceClass);
    BIND_INT64(stmt, 7, device.firstConnected);
    BIND_INT64(stmt, 8, device.lastConnected);
    BIND_TEXT(stmt, 9, device.lastDriveLetter);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertRecycleBinEntry(const RecycleBinEntry& entry) {
    const char* sql = "INSERT INTO recycle_bin (recycle_file_path, original_path, file_name, deletion_time, original_size, user_sid) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, entry.recycleFilePath);
    BIND_TEXT(stmt, 2, entry.originalPath);
    BIND_TEXT(stmt, 3, entry.fileName);
    BIND_INT64(stmt, 4, entry.deletionTime);
    BIND_INT64(stmt, 5, entry.originalSize);
    BIND_TEXT(stmt, 6, entry.userSid);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertBrowserArtifact(const BrowserArtifact& artifact) {
    const char* sql = "INSERT INTO browser_artifacts (browser_name, artifact_type, url, title, timestamp, visit_count, local_path, file_size) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, artifact.browserName);
    BIND_TEXT(stmt, 2, artifact.artifactType);
    BIND_TEXT(stmt, 3, artifact.url);
    BIND_TEXT(stmt, 4, artifact.title);
    BIND_INT64(stmt, 5, artifact.timestamp);
    BIND_INT(stmt, 6, artifact.visitCount);
    BIND_TEXT(stmt, 7, artifact.localPath);
    BIND_INT64(stmt, 8, artifact.fileSize);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertMftEntry(const MftEntryInfo& entry) {
    const char* sql = "INSERT INTO mft_entries (entry_number, file_name, file_path, parent_entry, logical_size, physical_size, creation_time, modification_time, access_time, mft_modification_time, fn_creation_time, fn_modification_time, is_directory, is_deleted, has_ads, permissions) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_INT64(stmt, 1, entry.entryNumber);
    BIND_TEXT(stmt, 2, entry.fileName);
    BIND_TEXT(stmt, 3, entry.filePath);
    BIND_INT64(stmt, 4, entry.parentEntry);
    BIND_INT64(stmt, 5, entry.logicalSize);
    BIND_INT64(stmt, 6, entry.physicalSize);
    BIND_INT64(stmt, 7, entry.creationTime);
    BIND_INT64(stmt, 8, entry.modificationTime);
    BIND_INT64(stmt, 9, entry.accessTime);
    BIND_INT64(stmt, 10, entry.mftModificationTime);
    BIND_INT64(stmt, 11, entry.fnCreationTime);
    BIND_INT64(stmt, 12, entry.fnModificationTime);
    BIND_INT(stmt, 13, entry.isDirectory ? 1 : 0);
    BIND_INT(stmt, 14, entry.isDeleted ? 1 : 0);
    BIND_INT(stmt, 15, entry.hasAds ? 1 : 0);
    BIND_TEXT(stmt, 16, entry.permissions);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertWindowsService(const WindowsServiceInfo& service) {
    const char* sql = "INSERT INTO windows_services (service_name, display_name, image_path, start_type, service_type, account_name, description, is_running) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, service.serviceName);
    BIND_TEXT(stmt, 2, service.displayName);
    BIND_TEXT(stmt, 3, service.imagePath);
    BIND_TEXT(stmt, 4, service.startType);
    BIND_TEXT(stmt, 5, service.serviceType);
    BIND_TEXT(stmt, 6, service.accountName);
    BIND_TEXT(stmt, 7, service.description);
    BIND_INT(stmt, 8, service.isRunning ? 1 : 0);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertScheduledTask(const ScheduledTaskInfo& task) {
    const char* sql = "INSERT INTO scheduled_tasks (task_name, task_path, author, description, action_type, action_path, arguments, trigger_type, last_run_time, next_run_time, status, run_as) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, task.taskName);
    BIND_TEXT(stmt, 2, task.taskPath);
    BIND_TEXT(stmt, 3, task.author);
    BIND_TEXT(stmt, 4, task.description);
    BIND_TEXT(stmt, 5, task.actionType);
    BIND_TEXT(stmt, 6, task.actionPath);
    BIND_TEXT(stmt, 7, task.arguments);
    BIND_TEXT(stmt, 8, task.triggerType);
    BIND_INT64(stmt, 9, task.lastRunTime);
    BIND_INT64(stmt, 10, task.nextRunTime);
    BIND_TEXT(stmt, 11, task.status);
    BIND_TEXT(stmt, 12, task.runAs);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertAmcacheEntry(const AmcacheEntry& entry) {
    const char* sql = "INSERT INTO amcache_entries (file_path, file_hash, file_name, company_name, product_name, product_version, file_description, file_size, link_time, last_modified, language) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, entry.filePath);
    BIND_TEXT(stmt, 2, entry.fileHash);
    BIND_TEXT(stmt, 3, entry.fileName);
    BIND_TEXT(stmt, 4, entry.companyName);
    BIND_TEXT(stmt, 5, entry.productName);
    BIND_TEXT(stmt, 6, entry.productVersion);
    BIND_TEXT(stmt, 7, entry.fileDescription);
    BIND_INT64(stmt, 8, entry.fileSize);
    BIND_INT64(stmt, 9, entry.linkTime);
    BIND_INT64(stmt, 10, entry.lastModified);
    BIND_TEXT(stmt, 11, entry.language);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool WindowsAnalysisDatabase::insertSrumEntry(const SrumEntry& entry) {
    const char* sql = "INSERT INTO srum_entries (app_name, user_name, timestamp, bytes_received, bytes_sent, foreground_duration, background_duration, cpu_time_ms) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    BIND_TEXT(stmt, 1, entry.appName);
    BIND_TEXT(stmt, 2, entry.userName);
    BIND_INT64(stmt, 3, entry.timestamp);
    BIND_INT64(stmt, 4, entry.bytesReceived);
    BIND_INT64(stmt, 5, entry.bytesSent);
    BIND_INT64(stmt, 6, entry.foregroundDuration);
    BIND_INT64(stmt, 7, entry.backgroundDuration);
    BIND_INT64(stmt, 8, entry.cpuTimeMs);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

// Batch insert methods
bool WindowsAnalysisDatabase::insertRegistryValues(const std::vector<RegistryValue>& values) {
    if (values.empty()) return true;
    
    beginTransaction();
    for (const auto& value : values) {
        if (!insertRegistryValue(value)) {
            rollbackTransaction();
            return false;
        }
    }
    return commitTransaction();
}

bool WindowsAnalysisDatabase::insertEventLogEntries(const std::vector<EventLogEntry>& entries) {
    if (entries.empty()) return true;
    
    beginTransaction();
    for (const auto& entry : entries) {
        if (!insertEventLogEntry(entry)) {
            rollbackTransaction();
            return false;
        }
    }
    return commitTransaction();
}

// Stub implementations for query methods (not used in current implementation)
std::vector<RegistryValue> WindowsAnalysisDatabase::queryRegistryValues(const std::string& whereClause) {
    return {};
}

std::vector<EventLogEntry> WindowsAnalysisDatabase::queryEventLogs(const std::string& whereClause) {
    return {};
}

std::vector<PrefetchInfo> WindowsAnalysisDatabase::queryPrefetchFiles(const std::string& whereClause) {
    return {};
}

std::vector<LnkFileInfo> WindowsAnalysisDatabase::queryLnkFiles(const std::string& whereClause) {
    return {};
}

std::vector<JumpListEntry> WindowsAnalysisDatabase::queryJumpListEntries(const std::string& whereClause) {
    return {};
}

std::vector<WindowsUserInfo> WindowsAnalysisDatabase::queryUserAccounts(const std::string& whereClause) {
    return {};
}

std::vector<USBDeviceInfo> WindowsAnalysisDatabase::queryUSBDevices(const std::string& whereClause) {
    return {};
}

std::vector<RecycleBinEntry> WindowsAnalysisDatabase::queryRecycleBinEntries(const std::string& whereClause) {
    return {};
}

std::vector<BrowserArtifact> WindowsAnalysisDatabase::queryBrowserArtifacts(const std::string& whereClause) {
    return {};
}

std::vector<MftEntryInfo> WindowsAnalysisDatabase::queryMftEntries(const std::string& whereClause) {
    return {};
}

std::vector<WindowsServiceInfo> WindowsAnalysisDatabase::queryWindowsServices(const std::string& whereClause) {
    return {};
}

std::vector<ScheduledTaskInfo> WindowsAnalysisDatabase::queryScheduledTasks(const std::string& whereClause) {
    return {};
}

std::vector<AmcacheEntry> WindowsAnalysisDatabase::queryAmcacheEntries(const std::string& whereClause) {
    return {};
}

std::vector<SrumEntry> WindowsAnalysisDatabase::querySrumEntries(const std::string& whereClause) {
    return {};
}

