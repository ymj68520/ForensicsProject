#include "adbExtractor.h"
#include <cstdint>
#include <ctime>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <fstream> 
#include <cerrno>   // For errno
#include <cstring>  // For strerror

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
    // 检查是否为目录（使用root权限）
    std::string check;
    if(has_root && use_root_for_extraction){
        check = adb.executeShellAsRoot("[ -d " + remote_path + " ] && echo DIR || echo FILE");
    }
    else{
        check = adb.executeShell("[ -d " + remote_path + " ] && echo DIR || echo FILE");
    }
    
    uint32_t mode = 0, size = 0, time = 0; // Declared here
    if (!adb.statFile(remote_path, mode, size, time)) {
        // stat failed. This might be permission denied or file not found.
        std::cerr << "Failed to stat: " << remote_path << std::endl;
        return false;
    }
    
    // Check for Directory (S_IFDIR = 0x4000)
    if ((mode & 0xF000) == 0x4000) {
        createDirectory(local_base);
        std::vector<ADBClient::SyncEntry> files = adb.listDirectory(remote_path);
        
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
                if (local_size == size) { // 'size' is remote file size
                    std::cout << "Skipping: " << remote_path << " (already complete)" << std::endl;
                    return true; // Already downloaded
                }
                // If local_size < size, it will re-download the whole file.
                // If local_size > size, something is wrong, proceed with download.
            }
            // Close to allow receiveFile to open for writing. If not opened, it's fine.
            local_file.close(); 
            
            return adb.receiveFile(remote_path, local_base, size); // Pass remote file size
    }
    
    return true;
}

bool AndroidDirectoryExtractor::initialize(bool auto_root = true) {
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
    
    // 检查并尝试获取root权限
    if(auto_root){
        std::cout << "\n检查root权限状态..." << std::endl;
            has_root = adb.checkRootAccess();
            
            if (has_root) {
                std::cout << "✓ 已具有root权限" << std::endl;
            } else {
                std::cout << "当前无root权限，尝试获取..." << std::endl;
                has_root = adb.acquireRoot();
                
                // 重新选择设备（adb root后需要重连）
                if (has_root) {
                    adb.selectDevice(devices[0]);
                }
            }
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

    // Recursive extraction
    bool success = extractFileRecursive(device_path, local_path);
    
    if (success) {
        std::cout << "Extraction successful: " << local_path << std::endl;
    } else {
        std::cerr << "Extraction failed (Check permissions)" << std::endl;
    }

    return success;
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
