# pragma once

#include <sys/stat.h>
#include <sys/types.h>

#include "adbClient.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir_cross(dir) _mkdir(dir)
#else
#define mkdir_cross(dir) mkdir(dir, 0755)
#endif

class AndroidDirectoryExtractor{
    private:
    ADBClient adb;
    std::string output_dir;
    bool has_root;
    bool use_root_for_extraction;

    void createDirectory(const std::string &path);
    bool extractFileRecursive(const std::string& remote_path, const std::string& local_base);
    public:
    AndroidDirectoryExtractor(const std::string& output = "./extracted_data") 
        : output_dir(output) {
        createDirectory(output_dir);
    }

    bool initialize(bool auto_root = true);
    bool extractDirectory(const std::string& device_path);
    void extractMultiple(const std::vector<std::string>& paths);
};