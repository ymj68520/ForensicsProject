// LinuxArtifactsParsers.cpp
// Implementation of various Linux artifact parsers

#include "LinuxFilesAnalyzer.h"
#include "LinuxLogParser.h"
#include "LinuxUserParser.h"
#include "LinuxHistoryParser.h"
#include "AuditLog/AuditLog.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

// ============================================================================
// System Log Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeSystemLogs() {
    std::cout << "Analyzing system logs (syslog, messages)..." << std::endl;

    // Find syslog files
    auto syslogFiles = queryFilesByPattern("%/var/log/syslog%");
    auto messagesFiles = queryFilesByPattern("%/var/log/messages%");

    for (const auto& file : syslogFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "syslog");
            linuxDb_->insertLogEntries(entries);
            AuditLog::instance().log("SYSTEM", "LINUX_SYSLOG_PARSED", 
                                      "Parsed " + std::to_string(entries.size()) + " syslog entries");
        }
    }

    for (const auto& file : messagesFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "messages");
            linuxDb_->insertLogEntries(entries);
        }
    }
}

void LinuxFilesAnalyzer::analyzeAuthLogs() {
    std::cout << "Analyzing authentication logs (auth.log, secure)..." << std::endl;

    auto authFiles = queryFilesByPattern("%/var/log/auth.log%");
    auto secureFiles = queryFilesByPattern("%/var/log/secure%");

    for (const auto& file : authFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "auth");
            linuxDb_->insertLogEntries(entries);
            AuditLog::instance().log("SYSTEM", "LINUX_AUTH_PARSED", 
                                      "Parsed " + std::to_string(entries.size()) + " auth.log entries");
        }
    }

    for (const auto& file : secureFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "secure");
            linuxDb_->insertLogEntries(entries);
        }
    }
}

void LinuxFilesAnalyzer::analyzeKernelLogs() {
    std::cout << "Analyzing kernel logs..." << std::endl;

    auto kernFiles = queryFilesByPattern("%/var/log/kern.log%");
    auto dmesgFiles = queryFilesByPattern("%/var/log/dmesg%");

    for (const auto& file : kernFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "kern");
            linuxDb_->insertLogEntries(entries);
        }
    }

    for (const auto& file : dmesgFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "dmesg");
            linuxDb_->insertLogEntries(entries);
        }
    }
}

void LinuxFilesAnalyzer::analyzeApplicationLogs() {
    std::cout << "Analyzing application logs..." << std::endl;

    // dpkg logs for package installations
    auto dpkgFiles = queryFilesByPattern("%/var/log/dpkg.log%");
    for (const auto& file : dpkgFiles) {
        std::string extractPath = getExtractPath("var/log/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "dpkg");
            linuxDb_->insertLogEntries(entries);
        }
    }

    // apt history
    auto aptFiles = queryFilesByPattern("%/var/log/apt/history.log%");
    for (const auto& file : aptFiles) {
        std::string extractPath = getExtractPath("var/log/apt/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseLogFile(extractPath, "apt");
            linuxDb_->insertLogEntries(entries);
        }
    }
}

std::vector<LinuxLogEntry> LinuxFilesAnalyzer::parseLogFile(const std::string& logPath,
                                                             const std::string& logType) {
    std::vector<LinuxLogEntry> entries;

    std::ifstream file(logPath);
    if (!file.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        LinuxLogEntry entry;
        if (logType == "kern" || logType == "dmesg") {
            entry = LinuxLogParser::parseKernelLogLine(line, logPath);
        } else if (logType == "dpkg") {
            entry = LinuxLogParser::parseDpkgLogLine(line, logPath);
        } else if (logType == "apt") {
            entry = LinuxLogParser::parseAptHistoryLine(line, logPath);
        } else {
            entry = LinuxLogParser::parseSyslogLine(line, logPath);
        }

        entries.push_back(entry);
    }

    return entries;
}

// ============================================================================
// User and Authentication Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeUserAccounts() {
    std::cout << "Analyzing user accounts..." << std::endl;

    // Find and parse /etc/passwd
    auto passwdFiles = queryFilesByPattern("%/etc/passwd");
    std::vector<LinuxUserInfo> users;

    for (const auto& file : passwdFiles) {
        std::string extractPath = getExtractPath("etc/passwd");
        if (extractFileToPath(file.inode, extractPath)) {
            std::ifstream f(extractPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                users = LinuxUserParser::parsePasswdFile(buffer.str());
            }
        }
        break; // Only one passwd file
    }

    // Find and parse /etc/shadow (if accessible)
    auto shadowFiles = queryFilesByPattern("%/etc/shadow");
    for (const auto& file : shadowFiles) {
        std::string extractPath = getExtractPath("etc/shadow");
        if (extractFileToPath(file.inode, extractPath)) {
            std::ifstream f(extractPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                LinuxUserParser::parseShadowFile(buffer.str(), users);
            }
        }
        break;
    }

    // Save users to database
    linuxDb_->insertUserInfos(users);
    AuditLog::instance().log("SYSTEM", "LINUX_USERS_PARSED", 
                              "Parsed " + std::to_string(users.size()) + " user accounts");

    // Find and parse /etc/group
    auto groupFiles = queryFilesByPattern("%/etc/group");
    for (const auto& file : groupFiles) {
        std::string extractPath = getExtractPath("etc/group");
        if (extractFileToPath(file.inode, extractPath)) {
            std::ifstream f(extractPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                auto groups = LinuxUserParser::parseGroupFile(buffer.str());
                for (const auto& group : groups) {
                    linuxDb_->insertGroupInfo(group);
                }
            }
        }
        break;
    }
}

void LinuxFilesAnalyzer::analyzeLoginHistory() {
    std::cout << "Analyzing login history (wtmp, btmp)..." << std::endl;

    // Parse wtmp (successful logins)
    auto wtmpFiles = queryFilesByPattern("%/var/log/wtmp%");
    for (const auto& file : wtmpFiles) {
        std::string extractPath = getExtractPath("var/log/wtmp");
        if (extractFileToPath(file.inode, extractPath)) {
            auto records = LinuxUserParser::parseWtmpFile(extractPath);
            linuxDb_->insertLoginRecords(records);
            AuditLog::instance().log("SYSTEM", "LINUX_WTMP_PARSED", 
                                      "Parsed " + std::to_string(records.size()) + " login records from wtmp");
        }
    }

    // Parse btmp (failed logins)
    auto btmpFiles = queryFilesByPattern("%/var/log/btmp%");
    for (const auto& file : btmpFiles) {
        std::string extractPath = getExtractPath("var/log/btmp");
        if (extractFileToPath(file.inode, extractPath)) {
            auto records = LinuxUserParser::parseBtmpFile(extractPath);
            linuxDb_->insertLoginRecords(records);
            AuditLog::instance().log("SYSTEM", "LINUX_BTMP_PARSED", 
                                      "Parsed " + std::to_string(records.size()) + " failed login records from btmp");
        }
    }
}

void LinuxFilesAnalyzer::analyzeSSHArtifacts() {
    std::cout << "Analyzing SSH artifacts..." << std::endl;

    // Find authorized_keys files
    auto authKeysFiles = queryFilesByPattern("%/.ssh/authorized_keys");
    for (const auto& file : authKeysFiles) {
        std::string extractPath = getExtractPath("ssh/" + std::to_string(file.inode) + "_authorized_keys");
        if (extractFileToPath(file.inode, extractPath)) {
            // Extract username from path
            std::string username = "unknown";
            size_t homePos = file.path.find("/home/");
            size_t rootPos = file.path.find("/root/");
            
            if (homePos != std::string::npos) {
                size_t start = homePos + 6;
                size_t end = file.path.find('/', start);
                if (end != std::string::npos) {
                    username = file.path.substr(start, end - start);
                }
            } else if (rootPos != std::string::npos) {
                username = "root";
            }

            auto keys = parseAuthorizedKeys(extractPath, username);
            linuxDb_->insertSSHKeys(keys);
        }
    }
}

std::vector<SSHKeyInfo> LinuxFilesAnalyzer::parseAuthorizedKeys(const std::string& keysPath,
                                                                  const std::string& username) {
    std::vector<SSHKeyInfo> keys;

    std::ifstream file(keysPath);
    if (!file.is_open()) {
        return keys;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        SSHKeyInfo key;
        key.username = username;
        key.keyPath = keysPath;

        // Format: [options] key-type base64-key [comment]
        // Common key types: ssh-rsa, ssh-ed25519, ecdsa-sha2-nistp256

        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (ss >> token) {
            tokens.push_back(token);
        }

        if (tokens.empty()) continue;

        size_t keyTypeIdx = 0;

        // Check if first token is options (doesn't start with ssh- or ecdsa-)
        if (!tokens[0].empty() && 
            tokens[0].find("ssh-") != 0 && 
            tokens[0].find("ecdsa-") != 0) {
            key.options = tokens[0];
            keyTypeIdx = 1;
        }

        if (keyTypeIdx < tokens.size()) {
            key.keyType = tokens[keyTypeIdx];
        }

        if (keyTypeIdx + 1 < tokens.size()) {
            key.publicKey = tokens[keyTypeIdx + 1];
        }

        // Rest is comment
        if (keyTypeIdx + 2 < tokens.size()) {
            key.comment = tokens[keyTypeIdx + 2];
            for (size_t i = keyTypeIdx + 3; i < tokens.size(); ++i) {
                key.comment += " " + tokens[i];
            }
        }

        keys.push_back(key);
    }

    return keys;
}

// ============================================================================
// Shell History Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeShellHistory() {
    std::cout << "Analyzing shell history..." << std::endl;

    // Find bash history files
    auto bashHistoryFiles = queryFilesByPattern("%/.bash_history");
    for (const auto& file : bashHistoryFiles) {
        std::string extractPath = getExtractPath("history/" + std::to_string(file.inode) + "_bash_history");
        if (extractFileToPath(file.inode, extractPath)) {
            // Extract username from path
            std::string username = "unknown";
            size_t homePos = file.path.find("/home/");
            size_t rootPos = file.path.find("/root/");
            
            if (homePos != std::string::npos) {
                size_t start = homePos + 6;
                size_t end = file.path.find('/', start);
                if (end != std::string::npos) {
                    username = file.path.substr(start, end - start);
                }
            } else if (rootPos != std::string::npos) {
                username = "root";
            }

            std::ifstream f(extractPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                auto entries = LinuxHistoryParser::parseBashHistory(buffer.str(), username, file.path);
                linuxDb_->insertShellHistories(entries);
                AuditLog::instance().log("SYSTEM", "LINUX_BASH_HISTORY_PARSED", 
                                          "Parsed " + std::to_string(entries.size()) + 
                                          " commands from " + username + "'s bash history");
            }
        }
    }

    // Find zsh history files
    auto zshHistoryFiles = queryFilesByPattern("%/.zsh_history");
    for (const auto& file : zshHistoryFiles) {
        std::string extractPath = getExtractPath("history/" + std::to_string(file.inode) + "_zsh_history");
        if (extractFileToPath(file.inode, extractPath)) {
            std::string username = "unknown";
            size_t homePos = file.path.find("/home/");
            if (homePos != std::string::npos) {
                size_t start = homePos + 6;
                size_t end = file.path.find('/', start);
                if (end != std::string::npos) {
                    username = file.path.substr(start, end - start);
                }
            }

            std::ifstream f(extractPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                auto entries = LinuxHistoryParser::parseZshHistory(buffer.str(), username, file.path);
                linuxDb_->insertShellHistories(entries);
            }
        }
    }
}

std::vector<ShellHistoryEntry> LinuxFilesAnalyzer::parseHistoryFile(const std::string& historyPath,
                                                                      const std::string& username,
                                                                      const std::string& shellType) {
    std::ifstream file(historyPath);
    if (!file.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return LinuxHistoryParser::parseHistoryFile(buffer.str(), username, historyPath, shellType);
}

// ============================================================================
// System Configuration Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeCronJobs() {
    std::cout << "Analyzing cron jobs..." << std::endl;

    // System crontab
    auto crontabFiles = queryFilesByPattern("%/etc/crontab");
    for (const auto& file : crontabFiles) {
        std::string extractPath = getExtractPath("etc/crontab");
        if (extractFileToPath(file.inode, extractPath)) {
            auto jobs = parseCronFile(extractPath, "root");
            linuxDb_->insertCronJobs(jobs);
        }
    }

    // User crontabs
    auto userCrontabs = queryFilesByPattern("%/var/spool/cron/crontabs/%");
    for (const auto& file : userCrontabs) {
        std::string extractPath = getExtractPath("cron/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto jobs = parseCronFile(extractPath, file.name);
            linuxDb_->insertCronJobs(jobs);
        }
    }

    // cron.d directory
    auto cronDFiles = queryFilesByPattern("%/etc/cron.d/%");
    for (const auto& file : cronDFiles) {
        std::string extractPath = getExtractPath("cron.d/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            auto jobs = parseCronFile(extractPath, "root");
            linuxDb_->insertCronJobs(jobs);
        }
    }
}

std::vector<CronJobEntry> LinuxFilesAnalyzer::parseCronFile(const std::string& cronPath,
                                                              const std::string& username) {
    std::vector<CronJobEntry> jobs;

    std::ifstream file(cronPath);
    if (!file.is_open()) {
        return jobs;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Skip variable assignments
        if (line.find('=') != std::string::npos && line.find(' ') > line.find('=')) {
            continue;
        }

        // Parse cron line
        std::istringstream ss(line);
        std::string minute, hour, dayOfMonth, month, dayOfWeek, user, command;

        ss >> minute >> hour >> dayOfMonth >> month >> dayOfWeek;

        // Check if this is system crontab format (has username field)
        std::string rest;
        std::getline(ss >> std::ws, rest);

        if (cronPath.find("/etc/crontab") != std::string::npos ||
            cronPath.find("/etc/cron.d/") != std::string::npos) {
            // System format: min hour dom mon dow user command
            size_t firstSpace = rest.find(' ');
            if (firstSpace != std::string::npos) {
                user = rest.substr(0, firstSpace);
                command = rest.substr(firstSpace + 1);
            }
        } else {
            // User crontab format: min hour dom mon dow command
            user = username;
            command = rest;
        }

        if (!command.empty()) {
            CronJobEntry job;
            job.username = user.empty() ? username : user;
            job.minute = minute;
            job.hour = hour;
            job.dayOfMonth = dayOfMonth;
            job.month = month;
            job.dayOfWeek = dayOfWeek;
            job.command = command;
            job.cronFile = cronPath;
            job.cronType = cronPath.find("cron.d") != std::string::npos ? "cron.d" :
                          cronPath.find("crontab") != std::string::npos ? "system" : "user";

            jobs.push_back(job);
        }
    }

    return jobs;
}

void LinuxFilesAnalyzer::analyzeSystemdServices() {
    std::cout << "Analyzing systemd services..." << std::endl;
    // Placeholder - systemd analysis would require parsing unit files
}

void LinuxFilesAnalyzer::analyzeInstalledPackages() {
    std::cout << "Analyzing installed packages..." << std::endl;

    // dpkg status file (Debian-based)
    auto dpkgStatus = queryFilesByPattern("%/var/lib/dpkg/status");
    for (const auto& file : dpkgStatus) {
        std::string extractPath = getExtractPath("dpkg/status");
        if (extractFileToPath(file.inode, extractPath)) {
            auto packages = parseDpkgStatus(extractPath);
            linuxDb_->insertPackageInfos(packages);
            AuditLog::instance().log("SYSTEM", "LINUX_PACKAGES_PARSED", 
                                      "Parsed " + std::to_string(packages.size()) + " installed packages");
        }
    }
}

std::vector<PackageInfo> LinuxFilesAnalyzer::parseDpkgStatus(const std::string& statusPath) {
    std::vector<PackageInfo> packages;

    std::ifstream file(statusPath);
    if (!file.is_open()) {
        return packages;
    }

    PackageInfo currentPkg;
    currentPkg.packageManager = "dpkg";
    bool hasPackage = false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            // End of package entry
            if (hasPackage && !currentPkg.name.empty()) {
                packages.push_back(currentPkg);
            }
            currentPkg = PackageInfo();
            currentPkg.packageManager = "dpkg";
            hasPackage = false;
            continue;
        }

        if (line.find("Package:") == 0) {
            currentPkg.name = line.substr(9);
            hasPackage = true;
        } else if (line.find("Version:") == 0) {
            currentPkg.version = line.substr(9);
        } else if (line.find("Architecture:") == 0) {
            currentPkg.architecture = line.substr(14);
        } else if (line.find("Status:") == 0) {
            currentPkg.status = line.substr(8);
        } else if (line.find("Description:") == 0) {
            currentPkg.description = line.substr(13);
        } else if (line.find("Maintainer:") == 0) {
            currentPkg.maintainer = line.substr(12);
        }
    }

    // Save last package
    if (hasPackage && !currentPkg.name.empty()) {
        packages.push_back(currentPkg);
    }

    return packages;
}

void LinuxFilesAnalyzer::analyzeNetworkConfiguration() {
    std::cout << "Analyzing network configuration..." << std::endl;
    // Placeholder for network configuration analysis
}

// ============================================================================
// Security Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeFirewallRules() {
    std::cout << "Analyzing firewall rules..." << std::endl;
    // Placeholder for firewall rules analysis
}

void LinuxFilesAnalyzer::analyzeAuditLogs() {
    std::cout << "Analyzing audit logs..." << std::endl;

    auto auditFiles = queryFilesByPattern("%/var/log/audit/audit.log%");
    for (const auto& file : auditFiles) {
        std::string extractPath = getExtractPath("audit/" + file.name);
        if (extractFileToPath(file.inode, extractPath)) {
            // Basic parsing - audit logs have a specific format
            auto entries = parseLogFile(extractPath, "audit");
            linuxDb_->insertLogEntries(entries);
        }
    }
}

// ============================================================================
// Browser Analysis
// ============================================================================

void LinuxFilesAnalyzer::analyzeBrowserData() {
    std::cout << "Analyzing browser data..." << std::endl;

    // Chrome/Chromium profiles
    auto chromeProfiles = queryFilesByPattern("%/.config/google-chrome/%");
    auto chromiumProfiles = queryFilesByPattern("%/.config/chromium/%");

    // Firefox profiles
    auto firefoxProfiles = queryFilesByPattern("%/.mozilla/firefox/%");

    // Placeholder - actual browser parsing would use similar logic to Windows browser parser
    // but with Linux-appropriate paths

    for (const auto& file : chromeProfiles) {
        if (file.path.find("Default") != std::string::npos ||
            file.path.find("Profile") != std::string::npos) {
            LinuxBrowserProfile profile;
            profile.browserType = LinuxBrowserType::CHROME;
            profile.browserName = "Google Chrome";
            profile.profilePath = file.path;
            
            // Extract username from path
            size_t homePos = file.path.find("/home/");
            if (homePos != std::string::npos) {
                size_t start = homePos + 6;
                size_t end = file.path.find('/', start);
                if (end != std::string::npos) {
                    profile.username = file.path.substr(start, end - start);
                }
            }
            
            linuxDb_->insertBrowserProfile(profile);
        }
    }

    for (const auto& file : firefoxProfiles) {
        if (file.name.find(".default") != std::string::npos) {
            LinuxBrowserProfile profile;
            profile.browserType = LinuxBrowserType::FIREFOX;
            profile.browserName = "Firefox";
            profile.profilePath = file.path;
            profile.profileName = file.name;
            
            size_t homePos = file.path.find("/home/");
            if (homePos != std::string::npos) {
                size_t start = homePos + 6;
                size_t end = file.path.find('/', start);
                if (end != std::string::npos) {
                    profile.username = file.path.substr(start, end - start);
                }
            }
            
            linuxDb_->insertBrowserProfile(profile);
        }
    }
}

// ============================================================================
// User Parsing Wrappers
// ============================================================================

std::vector<LinuxUserInfo> LinuxFilesAnalyzer::parsePasswdFile(const std::string& passwdPath) {
    std::ifstream file(passwdPath);
    if (!file.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return LinuxUserParser::parsePasswdFile(buffer.str());
}

std::vector<LinuxUserInfo> LinuxFilesAnalyzer::parseShadowFile(const std::string& shadowPath,
                                                                 std::vector<LinuxUserInfo>& users) {
    std::ifstream file(shadowPath);
    if (!file.is_open()) {
        return users;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    LinuxUserParser::parseShadowFile(buffer.str(), users);
    return users;
}

std::vector<LinuxGroupInfo> LinuxFilesAnalyzer::parseGroupFile(const std::string& groupPath) {
    std::ifstream file(groupPath);
    if (!file.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return LinuxUserParser::parseGroupFile(buffer.str());
}

std::vector<LinuxLoginRecord> LinuxFilesAnalyzer::parseWtmpFile(const std::string& wtmpPath) {
    return LinuxUserParser::parseWtmpFile(wtmpPath);
}

std::vector<LinuxLoginRecord> LinuxFilesAnalyzer::parseBtmpFile(const std::string& btmpPath) {
    return LinuxUserParser::parseBtmpFile(btmpPath);
}
