// LinuxAnalysisDatabase.h
// Database manager for storing Linux forensic analysis results

#pragma once
#ifndef LINUX_ANALYSIS_DATABASE_H
#define LINUX_ANALYSIS_DATABASE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "LinuxDataTypes.h"

class LinuxAnalysisDatabase {
public:
    explicit LinuxAnalysisDatabase(const std::string& dbPath);
    ~LinuxAnalysisDatabase();

    // Initialize database and create tables
    bool initialize();

    // Log entry operations
    bool insertLogEntry(const LinuxLogEntry& entry);
    bool insertLogEntries(const std::vector<LinuxLogEntry>& entries);
    std::vector<LinuxLogEntry> queryLogEntries(const std::string& whereClause = "");

    // User account operations
    bool insertUserInfo(const LinuxUserInfo& user);
    bool insertUserInfos(const std::vector<LinuxUserInfo>& users);
    std::vector<LinuxUserInfo> queryUserAccounts(const std::string& whereClause = "");

    // Group operations
    bool insertGroupInfo(const LinuxGroupInfo& group);
    std::vector<LinuxGroupInfo> queryGroups(const std::string& whereClause = "");

    // Login record operations
    bool insertLoginRecord(const LinuxLoginRecord& record);
    bool insertLoginRecords(const std::vector<LinuxLoginRecord>& records);
    std::vector<LinuxLoginRecord> queryLoginRecords(const std::string& whereClause = "");

    // Shell history operations
    bool insertShellHistory(const ShellHistoryEntry& entry);
    bool insertShellHistories(const std::vector<ShellHistoryEntry>& entries);
    std::vector<ShellHistoryEntry> queryShellHistory(const std::string& whereClause = "");

    // Cron job operations
    bool insertCronJob(const CronJobEntry& job);
    bool insertCronJobs(const std::vector<CronJobEntry>& jobs);
    std::vector<CronJobEntry> queryCronJobs(const std::string& whereClause = "");

    // SSH key operations
    bool insertSSHKey(const SSHKeyInfo& key);
    bool insertSSHKeys(const std::vector<SSHKeyInfo>& keys);
    std::vector<SSHKeyInfo> querySSHKeys(const std::string& whereClause = "");

    // SSH known host operations
    bool insertSSHKnownHost(const SSHKnownHost& host);
    std::vector<SSHKnownHost> querySSHKnownHosts(const std::string& whereClause = "");

    // Package info operations
    bool insertPackageInfo(const PackageInfo& pkg);
    bool insertPackageInfos(const std::vector<PackageInfo>& pkgs);
    std::vector<PackageInfo> queryPackages(const std::string& whereClause = "");

    // Network connection operations
    bool insertNetworkConnection(const NetworkConnection& conn);
    std::vector<NetworkConnection> queryNetworkConnections(const std::string& whereClause = "");

    // Systemd service operations
    bool insertSystemdService(const SystemdServiceInfo& service);
    std::vector<SystemdServiceInfo> querySystemdServices(const std::string& whereClause = "");

    // Kernel module operations
    bool insertKernelModule(const KernelModuleInfo& module);
    std::vector<KernelModuleInfo> queryKernelModules(const std::string& whereClause = "");

    // Firewall rule operations
    bool insertFirewallRule(const FirewallRule& rule);
    std::vector<FirewallRule> queryFirewallRules(const std::string& whereClause = "");

    // Audit log operations
    bool insertAuditLog(const LinuxAuditLogEntry& entry);
    bool insertAuditLogs(const std::vector<LinuxAuditLogEntry>& entries);
    std::vector<LinuxAuditLogEntry> queryAuditLogs(const std::string& whereClause = "");

    // Browser profile operations
    bool insertBrowserProfile(const LinuxBrowserProfile& profile);
    std::vector<LinuxBrowserProfile> queryBrowserProfiles(const std::string& whereClause = "");

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

#endif // LINUX_ANALYSIS_DATABASE_H
