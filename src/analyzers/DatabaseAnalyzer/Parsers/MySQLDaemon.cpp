#include "MySQLDaemon.h"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <signal.h>
#include <thread>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace ForensicAnalyzer {
namespace Database {

namespace {

std::string find_mysqld() {
    if (const char* configured = std::getenv("MYSQLD_PATH");
        configured && *configured) {
        if (::access(configured, X_OK) == 0) return configured;
    }
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return {};
    std::string pathList(pathEnv);
    size_t start = 0;
    while (start <= pathList.size()) {
        const size_t end = pathList.find(':', start);
        const std::string dir = pathList.substr(start, end == std::string::npos ? end : end - start);
        const fs::path candidate = fs::path(dir.empty() ? "." : dir) / "mysqld";
        if (::access(candidate.c_str(), X_OK) == 0) return candidate.string();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

bool trusted_data_dir(const fs::path& input, fs::path& output) {
    std::error_code ec;
    output = fs::weakly_canonical(input, ec);
    if (ec || !fs::is_directory(output, ec)) return false;
    const auto perms = fs::status(output, ec).permissions();
    if (ec || (perms & fs::perms::others_write) != fs::perms::none) return false;
    return true;
}

}  // namespace

MySQLDaemon::MySQLDaemon(const std::string& dataDir) : dataDir_(dataDir) {
    fs::path trusted;
    if (trusted_data_dir(dataDir, trusted)) {
        dataDir_ = trusted.string();
    }
    socketPath_ = (fs::path(dataDir_) / "mysql.sock").string();
    logPath_ = (fs::path(dataDir_) / "mysqld.log").string();
}

MySQLDaemon::~MySQLDaemon() {
    stop();
}

bool MySQLDaemon::checkMySqlAvailable() {
    return !find_mysqld().empty();
}

bool MySQLDaemon::start() {
    fs::path trusted;
    if (!trusted_data_dir(dataDir_, trusted)) {
        lastError_ = "MySQL datadir must be an existing empty directory without world-write permissions.";
        return false;
    }
    const std::string mysqld = find_mysqld();
    if (mysqld.empty()) {
        lastError_ = "mysqld binary not found in PATH or MYSQLD_PATH.";
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        lastError_ = "Failed to fork mysqld process.";
        return false;
    } else if (pid == 0) {
        ::umask(0077);
        std::vector<std::string> args{
            mysqld, "--skip-grant-tables", "--skip-networking",
            "--socket=" + socketPath_, "--datadir=" + dataDir_,
            "--log-error=" + logPath_};
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        ::execv(mysqld.c_str(), argv.data());
        _exit(127);
    } else {
        daemonPid_ = pid;
        for (int i = 0; i < 100; i++) {
            if (fs::exists(socketPath_)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!fs::exists(socketPath_)) {
            lastError_ = "mysqld failed to start or socket not created in time.";
            stop();
            return false;
        }

        conn_ = mysql_init(NULL);
        if (conn_ == NULL) {
            lastError_ = "mysql_init() failed.";
            stop();
            return false;
        }

        if (mysql_real_connect(conn_, "localhost", "root", "", NULL, 0,
                               socketPath_.c_str(), 0) == NULL) {
            lastError_ = std::string("mysql_real_connect() failed: ") + mysql_error(conn_);
            stop();
            return false;
        }

        isRunning_ = true;
        return true;
    }
}

void MySQLDaemon::stop() {
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }

    if (daemonPid_ != -1) {
        kill(daemonPid_, SIGTERM);
        for (int i = 0; i < 50; ++i) {
            const pid_t result = waitpid(daemonPid_, nullptr, WNOHANG);
            if (result == daemonPid_ || (result == -1 && errno == ECHILD)) {
                daemonPid_ = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (daemonPid_ != -1) {
            kill(daemonPid_, SIGKILL);
            waitpid(daemonPid_, nullptr, 0);
            daemonPid_ = -1;
        }
    }
    
    // Clean up temporary files
    if (fs::exists(socketPath_)) {
        fs::remove(socketPath_);
    }
    isRunning_ = false;
}

} // namespace Database
} // namespace ForensicAnalyzer
