#include "adbExtractor.h"

void AndroidDirectoryExtractor::createDirectory(const std::string &path){
    size_t pos = 0;
    while((pos=path.find_first_of("/\\", pos)) != std::string::npos){
        std::string dir = path.substr(0, pos++);
        if(!dir.empty()){
            mkdir_cross(dir.c_str());

        }
        mkdir_cross(path.c_str());
    }
}

bool AndroidDirectoryExtractor::extractFileRecursive(const std::string& remote_path, const std::string& local_base) {
        // 检查是否为目录
        std::string check = adb.executeShell("[ -d " + remote_path + " ] && echo DIR || echo FILE");
        
        if (check.find("DIR") != std::string::npos) {
            // 是目录，递归提取
            std::vector<std::string> files = adb.listDirectory(remote_path);
            
            for (const auto& file : files) {
                if (file == "." || file == "..") continue;
                
                std::string remote_full = remote_path + "/" + file;
                std::string local_full = local_base + "/" + file;
                
                extractFileRecursive(remote_full, local_full);
            }
            return true;
        } else {
            // 是文件，直接下载
            createDirectory(local_base.substr(0, local_base.find_last_of("/\\")));
            return adb.receiveFile(remote_path, local_base);
        }
    }

    bool AndroidDirectoryExtractor::initialize() {
        if (!adb.connect()) return false;

        auto devices = adb.getDevices();
        if (devices.empty()) {
            std::cerr << "✗ 未找到设备" << std::endl;
            return false;
        }

        std::cout << "✓ 找到 " << devices.size() << " 个设备" << std::endl;
        std::cout << "  使用设备: " << devices[0] << std::endl;

        return adb.selectDevice(devices[0]);
    }

    bool AndroidDirectoryExtractor::extractDirectory(const std::string& device_path) {
        std::cout << "\n开始提取: " << device_path << std::endl;

        // 检查权限
        std::string check = adb.executeShell("ls -la " + device_path + " 2>&1");
        if (check.find("Permission denied") != std::string::npos) {
            std::cerr << "✗ 权限不足，需要root权限" << std::endl;
            return false;
        }

        // 准备本地路径
        std::string clean_path = device_path;
        if (clean_path[0] == '/') clean_path = clean_path.substr(1);
        std::string local_path = output_dir + "/" + clean_path;

        // 建立sync连接
        if (!adb.syncConnect()) {
            std::cerr << "✗ 无法建立同步连接" << std::endl;
            return false;
        }

        // 递归提取
        bool success = extractFileRecursive(device_path, local_path);
        
        if (success) {
            std::cout << "✓ 提取成功: " << local_path << std::endl;
        } else {
            std::cerr << "✗ 提取失败" << std::endl;
        }

        return success;
    }

    void AndroidDirectoryExtractor::extractMultiple(const std::vector<std::string>& paths) {
        int success = 0, failed = 0;
        
        for (const auto& path : paths) {
            if (extractDirectory(path)) success++;
            else failed++;
        }

        std::cout << "\n=== 完成 ===" << std::endl;
        std::cout << "成功: " << success << " | 失败: " << failed << std::endl;
    }

