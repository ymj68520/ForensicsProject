#include "adbExtractor.h"
#include <cstdint>
#include <ctime>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cerrno>   // For errno
#include <cstring>  // For strerror

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

void AndroidDirectoryExtractor::createDirectory(const std::string &path){
    std::string temp = path;
    size_t pos = 0;
    while(true){
        pos = temp.find_first_of("/\\", pos);
        if(pos == std::string::npos){
            if (mkdir_cross(temp.c_str()) != 0 && errno != EEXIST) {
                std::cerr << "Failed to create directory: " << temp << " Error: " << strerror(errno) << std::endl;
            }
            break;
        }
        std::string dir = temp.substr(0, pos);
        if(!dir.empty()){
            if (mkdir_cross(dir.c_str()) != 0 && errno != EEXIST) {
                std::cerr << "Failed to create directory: " << dir << " Error: " << strerror(errno) << std::endl;
            }
        }
        pos++;
    }
}

bool AndroidDirectoryExtractor::extractFileRecursive(const std::string& remote_path, const std::string& local_base) {
    // Check if it's a directory (using root privileges)
    std::string check;
    if(has_root && use_root_for_extraction){
        check = adb.executeShellAsRoot("[ -d " + remote_path + " ] && echo DIR || echo FILE");
    }
    else{
        check = adb.executeShell("[ -d " + remote_path + " ] && echo DIR || echo FILE");
    }

    uint32_t mode = 0, size = 0, time = 0; // Declared here
    bool stat_success = adb.statFile(remote_path, mode, size, time);

    // Fallback to Shell-based STAT if Sync STAT fails (e.g. permission denied)
    if (!stat_success && has_root) {
        stat_success = adb.statFileShell(remote_path, mode, size, time);
        if (stat_success) {
           // std::cout << "Debug: Used Shell STAT for " << remote_path << std::endl;
        }
    }

    if (!stat_success) {
        // stat failed. This might be permission denied or file not found.
        std::cerr << "Failed to stat: " << remote_path << " (Permission denied?)" << std::endl;
        return false;
    }

    // Check for Directory (S_IFDIR = 0x4000)
    if ((mode & 0xF000) == 0x4000) {
        createDirectory(local_base);
        std::vector<ADBClient::SyncEntry> files = adb.listDirectory(remote_path);

        // Fallback to Shell List if Sync List returns empty/fail but we know it's a dir
        if (files.empty() && has_root) {
             std::vector<ADBClient::SyncEntry> shell_files = adb.listDirectoryShell(remote_path);
             if (!shell_files.empty()) {
                 files = shell_files;
             }
        }

        for (const auto& entry : files) {
            if (entry.name == "." || entry.name == "..") continue;

            std::string remote_full = remote_path + "/" + entry.name;
            std::string local_full = local_base + "/" + entry.name;

            extractFileRecursive(remote_full, local_full);
        }
        return true;
    }
    // Check for Regular File (S_IFREG = 0x8000)
    else if ((mode & 0xF000) == 0x8000) {
            std::string parent = local_base.substr(0, local_base.find_last_of("/\\"));
            createDirectory(parent);

            // Resume download / fault tolerance part:
            // Check if local file exists and is complete
            std::ifstream local_file(local_base, std::ios::binary | std::ios::ate);
            if (local_file.is_open()) {
                uint64_t local_size = local_file.tellg();
                local_file.close();

                if (local_size == size) {
                    std::cout << "Skipping: " << remote_path << " (already complete)" << std::endl;
                    return true;
                }
            }

            std::cout << "Downloading: " << remote_path << " (" << size << " bytes)" << std::endl;

            bool pull_success = adb.receiveFile(remote_path, local_base, size);

            if (!pull_success && has_root) {
                std::cout << "Sync pull failed, trying Root Shell Pull for: " << remote_path << std::endl;
                pull_success = adb.pullFileShell(remote_path, local_base);
            }

            if (pull_success) {
                std::cout << "Success: " << remote_path << std::endl;
                return true;
            } else {
                std::cerr << "Failed: " << remote_path << std::endl;
                return false;
            }
    } else {
        // Other file types (symlinks, etc.)
        std::cout << "Skipping non-regular file: " << remote_path << std::endl;
        return true;
    }
}

bool AndroidDirectoryExtractor::initialize(bool auto_root) {
    if (!adb.connect()) return false;

    auto devices = adb.getDevices();
    adb.disconnect();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (devices.empty()) {
        std::cerr << "No device found" << std::endl;
        return false;
    }

    std::cout << "Found " << devices.size() << " device(s)" << std::endl;
    std::cout << "  Using device: " << devices[0] << std::endl;

    if (!adb.connect()) return false;
    if (!adb.selectDevice(devices[0])) return false;

    // Check and attempt to acquire root privileges
    if(auto_root){
        std::cout << "\nChecking root privilege status..." << std::endl;
            has_root = adb.checkRootAccess();

            if (has_root) {
                std::cout << "[OK] Already have root privileges" << std::endl;
            } else {
                std::cout << "No root privileges currently, attempting to acquire..." << std::endl;
                has_root = adb.acquireRoot();

                // Re-select device (need to reconnect after adb root)
                if (has_root) {
                    adb.selectDevice(devices[0]);
                }
            }
    }

    if (has_root) {
        use_root_for_extraction = true;
        std::cout << "Root access available - partition extraction enabled" << std::endl;
    } else {
        std::cout << "Root access not available - partition extraction limited" << std::endl;
    }

    if (!adb.syncConnect()) {
            std::cerr << "Failed to establish sync connection" << std::endl;
            return false;
    }

    return true;
}

bool AndroidDirectoryExtractor::extractDirectory(const std::string& device_path) {
    std::cout << "\nStarting extraction: " << device_path << std::endl;

    // Permission check removed as it requires Shell protocol which would break Sync connection.
    // Sync protocol will simply fail to STAT if permission is denied.

    // Prepare local path
    std::string clean_path = device_path;
    if (clean_path[0] == '/') clean_path = clean_path.substr(1);
    std::string local_path = output_dir + "/" + clean_path;

    return extractFileRecursive(device_path, local_path);
}

void AndroidDirectoryExtractor::extractMultiple(const std::vector<std::string>& paths) {
    int success = 0, failed = 0;

    for (const auto& path : paths) {
        if (extractDirectory(path)) success++;
        else failed++;
    }

    std::cout << "\n=== Completed ===" << std::endl;
    std::cout << "Successful: " << success << " | Failed: " << failed << std::endl;
}

// 分区提取功能实现
bool AndroidDirectoryExtractor::testPartitionReadAccess(const std::string& device_path) {
    std::string cmd = "test -r " + device_path + " && echo 'readable' || echo 'not_readable'";
    std::string result = adb.executeShellAsRoot(cmd);
    return result.find("readable") != std::string::npos;
}

bool AndroidDirectoryExtractor::extractPartitionUsingDD(const std::string& partition_name, const std::string& block_device, const std::string& output_path) {
    std::cout << "Trying dd method..." << std::endl;

    // 使用dd命令读取分区到临时位置
    std::string dd_cmd = "dd if=" + block_device + " of=/data/local/tmp/" + partition_name + ".img bs=4096";
    std::string result = adb.executeShellAsRoot(dd_cmd);

    if (result.empty() || result.find("No such file") != std::string::npos) {
        return false;
    }

    // 使用adb pull下载文件
    return adb.pullFileShell("/data/local/tmp/" + partition_name + ".img", output_path);
}

bool AndroidDirectoryExtractor::extractPartitionUsingShell(const std::string& block_device, const std::string& output_path) {
    std::cout << "Trying shell cat method..." << std::endl;

    std::string cmd = "cat '" + block_device + "'";
    std::vector<char> output;

    if (!adb.executeRaw(cmd, output)) {
        return false;
    }

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to create output file: " << output_path << std::endl;
        return false;
    }

    outfile.write(output.data(), output.size());
    outfile.close();

    return true;
}

bool AndroidDirectoryExtractor::extractPartition(const std::string& partition_name, const std::string& output_filename) {
    if (!has_root) {
        std::cerr << "Error: Root access required for partition extraction" << std::endl;
        return false;
    }

    std::string filename = output_filename.empty() ? (partition_name + ".img") : output_filename;
    std::string output_path = output_dir + "/partitions/" + filename;

    std::cout << "Extracting partition: " << partition_name << std::endl;

    // 创建分区输出目录
    createDirectory(output_dir + "/partitions");

    // 查找分区路径
    std::string device_path = "/dev/block/by-name/" + partition_name;

    // 获取实际块设备路径
    std::string ls_cmd = "ls -la " + device_path;
    std::string result = adb.executeShellAsRoot(ls_cmd);

    std::string block_device = device_path; // 默认使用device_path

    // 解析ls输出来获取实际块设备
    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("->") != std::string::npos) {
            size_t arrow_pos = line.find("->");
            if (arrow_pos != std::string::npos) {
                block_device = line.substr(arrow_pos + 2);
                // 移除前后空白
                block_device.erase(0, block_device.find_first_not_of(" \t"));
                block_device.erase(block_device.find_last_not_of(" \t") + 1);
                break;
            }
        }
    }

    std::cout << "Device path: " << device_path << std::endl;
    std::cout << "Block device: " << block_device << std::endl;
    std::cout << "Output: " << output_path << std::endl;

    // 测试可读性
    if (!testPartitionReadAccess(block_device)) {
        std::cerr << "Error: Partition is not readable" << std::endl;
        return false;
    }

    // 尝试不同的提取方法
    if (extractPartitionUsingDD(partition_name, block_device, output_path)) {
        std::cout << "Successfully extracted using dd command" << std::endl;
        return true;
    } else if (extractPartitionUsingShell(block_device, output_path)) {
        std::cout << "Successfully extracted using shell cat" << std::endl;
        return true;
    } else {
        std::cerr << "Failed to extract partition using all available methods" << std::endl;
        return false;
    }
}

bool AndroidDirectoryExtractor::extractMultiplePartitions(const std::vector<std::string>& partition_names) {
    bool all_success = true;

    for (const auto& name : partition_names) {
        std::cout << "\n--- Extracting " << name << " ---" << std::endl;
        if (!extractPartition(name)) {
            all_success = false;
            std::cerr << "Failed to extract: " << name << std::endl;
        }
    }

    return all_success;
}

void AndroidDirectoryExtractor::listAvailablePartitions() {
    std::cout << "\nScanning for available partitions..." << std::endl;

    // 尝试读取 /dev/block/by-name/
    std::string cmd = "ls -la /dev/block/by-name/";
    std::string result = adb.executeShellAsRoot(cmd);

    if (result.empty() || result.find("No such file") != std::string::npos) {
        std::cout << "No /dev/block/by-name/ directory found" << std::endl;
        return;
    }

    std::istringstream stream(result);
    std::string line;
    std::vector<PartitionInfo> partitions;

    std::cout << "\n=== Available Partitions ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Name"
              << std::setw(30) << "Device Path"
              << std::setw(25) << "Block Device"
              << std::setw(15) << "Size"
              << std::setw(10) << "Readable" << std::endl;
    std::cout << std::string(100, '-') << std::endl;

    while (std::getline(stream, line)) {
        if (line.empty() || line.find("total") == 0 || line.find("drwxr-xr-x") == 0) continue;

        // 解析ls输出: lrwxrwxrwx 1 root root 15 2023-01-01 12:00 vbmeta -> /dev/block/sde12
        std::istringstream iss(line);
        std::string permissions, links, user, group, size, date, time, name, arrow, target;

        if (iss >> permissions >> links >> user >> group >> size >> date >> time >> name >> arrow >> target) {
            PartitionInfo info;
            info.name = name;
            info.device_path = "/dev/block/by-name/" + name;
            info.block_device = target;
            info.requires_root = true;

            // 尝试获取分区大小
            std::string size_cmd = "stat -c '%s' " + target + " 2>/dev/null";
            std::string size_result = adb.executeShellAsRoot(size_cmd);
            if (!size_result.empty() && size_result.find("stat:") == std::string::npos) {
                try {
                    info.size = std::stoull(size_result);
                } catch (...) {
                    info.size = 0;
                }
            }

            // 测试可读性
            info.is_readable = testPartitionReadAccess(info.block_device);

            std::string size_str;
            if (info.size > 0) {
                if (info.size >= 1024*1024*1024) {
                    size_str = std::to_string(info.size / 1024.0 / 1024.0 / 1024.0) + " GB";
                } else if (info.size >= 1024*1024) {
                    size_str = std::to_string(info.size / 1024.0 / 1024.0) + " MB";
                } else {
                    size_str = std::to_string(info.size / 1024.0) + " KB";
                }
            } else {
                size_str = "Unknown";
            }

            std::cout << std::left << std::setw(20) << info.name
                      << std::setw(30) << info.device_path
                      << std::setw(25) << info.block_device
                      << std::setw(15) << size_str
                      << std::setw(10) << (info.is_readable ? "Yes" : "No") << std::endl;
        }
    }
    std::cout << std::endl;
}

std::vector<PartitionInfo> AndroidDirectoryExtractor::getPartitionList() {
    std::vector<PartitionInfo> partitions;

    // 尝试读取 /dev/block/by-name/
    std::string cmd = "ls -la /dev/block/by-name/";
    std::string result = adb.executeShellAsRoot(cmd);

    if (result.empty() || result.find("No such file") != std::string::npos) {
        return partitions;
    }

    std::istringstream stream(result);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line.find("total") == 0 || line.find("drwxr-xr-x") == 0) continue;

        std::istringstream iss(line);
        std::string permissions, links, user, group, size, date, time, name, arrow, target;

        if (iss >> permissions >> links >> user >> group >> size >> date >> time >> name >> arrow >> target) {
            PartitionInfo info;
            info.name = name;
            info.device_path = "/dev/block/by-name/" + name;
            info.block_device = target;
            info.requires_root = true;

            // 尝试获取分区大小
            std::string size_cmd = "stat -c '%s' " + target + " 2>/dev/null";
            std::string size_result = adb.executeShellAsRoot(size_cmd);
            if (!size_result.empty() && size_result.find("stat:") == std::string::npos) {
                try {
                    info.size = std::stoull(size_result);
                } catch (...) {
                    info.size = 0;
                }
            }

            // 测试可读性
            info.is_readable = testPartitionReadAccess(info.block_device);

            partitions.push_back(info);
        }
    }

    return partitions;
}