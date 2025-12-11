#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <string>

#include "adbClient.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir_cross(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define mkdir_cross(dir) mkdir(dir, 0755)
#endif

struct PartitionInfo {
    std::string name;           // 分区名称 (如 "vbmeta", "boot")
    std::string device_path;     // 设备路径 (如 "/dev/block/by-name/vbmeta")
    std::string block_device;    // 实际块设备路径 (如 "/dev/block/sde12")
    uint64_t size;              // 分区大小 (字节)
    std::string type;           // 分区类型 (如 "emmc", "ufs")
    bool is_readable;           // 是否可读
    bool requires_root;         // 是否需要root权限

    PartitionInfo() : size(0), is_readable(false), requires_root(true) {}
};

class AndroidDirectoryExtractor{
    private:
    ADBClient adb;
    std::string output_dir;
    bool has_root;
    bool use_root_for_extraction;

    void createDirectory(const std::string &path);
    bool extractFileRecursive(const std::string& remote_path, const std::string& local_base);

    // 分区提取相关方法
    bool testPartitionReadAccess(const std::string& device_path);
    bool extractPartitionUsingDD(const std::string& partition_name, const std::string& block_device, const std::string& output_path);
    bool extractPartitionUsingShell(const std::string& block_device, const std::string& output_path);

    public:
    AndroidDirectoryExtractor(const std::string& output = "./extracted_data")
        : output_dir(output), has_root(false), use_root_for_extraction(false) {
        createDirectory(output_dir);
    }

    bool initialize(bool auto_root = true);
    bool extractDirectory(const std::string& device_path);
    void extractMultiple(const std::vector<std::string>& paths);

    // 分区提取功能
    bool extractPartition(const std::string& partition_name, const std::string& output_filename = "");
    bool extractMultiplePartitions(const std::vector<std::string>& partition_names);
    void listAvailablePartitions();
    std::vector<PartitionInfo> getPartitionList();
    bool hasRootAccess() const { return has_root; }
};