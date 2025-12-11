#include "adbExtractor.h"
#include <iostream>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// 安全的分区列表 - 这些通常是安全的读取
std::vector<std::string> SAFE_PARTITIONS = {
    "vbmeta",    // 验证启动分区
    "boot",      // 启动镜像 (通常包含kernel和ramdisk)
    "recovery",  // 恢复分区
    "system",    // 系统分区 (通常只读)
    "vendor",    // 厂商分区
    "product",   // 产品分区
    "cache",     // 缓存分区
    "userdata",  // 用户数据分区 (需要谨慎)
    "misc",      // 杂项分区
    "persist"    // 持久化分区
};

// 危险的分区列表 - 这些可能影响设备正常工作
std::vector<std::string> DANGEROUS_PARTITIONS = {
    "efuse",     // 熔丝设置
    "param",     // 参数分区
    "frp",       // 工厂重置保护
    "config",    // 配置分区
    "sec",       // 安全分区
    "keymaster", // 密钥管理
    "tee",       // 可信执行环境
    "proinfo"    // 生产信息
};

bool isSafePartition(const std::string& partition_name) {
    // 检查是否在危险列表中
    for (const auto& dangerous : DANGEROUS_PARTITIONS) {
        if (partition_name.find(dangerous) != std::string::npos) {
            return false;
        }
    }

    // 检查是否在安全列表中
    for (const auto& safe : SAFE_PARTITIONS) {
        if (partition_name.find(safe) != std::string::npos) {
            return true;
        }
    }

    // 默认情况下，不认识的分区被认为是中等风险的
    std::cout << "Warning: Unknown partition '" << partition_name
              << "'. Treat as potentially unsafe." << std::endl;
    return false;
}

void showUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] [paths...]" << std::endl;
    std::cout << std::endl;
    std::cout << "File Extraction Mode:" << std::endl;
    std::cout << "  " << program_name << " /system/build.prop /data/data/com.example" << std::endl;
    std::cout << std::endl;
    std::cout << "Partition Extraction Mode:" << std::endl;
    std::cout << "  " << program_name << " --partition vbmeta" << std::endl;
    std::cout << "  " << program_name << " --partition vbmeta boot recovery" << std::endl;
    std::cout << "  " << program_name << " --list-partitions" << std::endl;
    std::cout << "  " << program_name << " --extract-safe" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --partition vbmeta      # Extract vbmeta partition" << std::endl;
    std::cout << "  " << program_name << " --extract-safe         # Extract all safe partitions" << std::endl;
    std::cout << "  " << program_name << " --list-partitions       # List available partitions" << std::endl;
    std::cout << "  " << program_name << " /system/build.prop      # Extract files (original behavior)" << std::endl;
}

void handlePartitionExtraction(AndroidDirectoryExtractor& extractor,
                             const std::vector<std::string>& partitions) {
    std::cout << "\n=== Partition Extraction Mode ===" << std::endl;

    if (!extractor.hasRootAccess()) {
        std::cout << "Warning: No root access. Partition access may be limited." << std::endl;
    }

    // 筛选安全分区
    std::vector<std::string> safe_partitions;
    std::vector<std::string> unsafe_partitions;

    for (const auto& partition : partitions) {
        if (isSafePartition(partition)) {
            safe_partitions.push_back(partition);
            std::cout << "✓ Safe: " << partition << std::endl;
        } else {
            unsafe_partitions.push_back(partition);
            std::cout << "✗ Unsafe: " << partition << " (skipped)" << std::endl;
        }
    }

    if (safe_partitions.empty()) {
        std::cout << "\nNo safe partitions to extract." << std::endl;
        return;
    }

    std::cout << "\nProceeding with extraction of " << safe_partitions.size() << " safe partitions..." << std::endl;

    bool success = extractor.extractMultiplePartitions(safe_partitions);

    if (success) {
        std::cout << "\n=== Extraction Complete ===" << std::endl;
        std::cout << "Partition images saved to: ./extracted_android_data/partitions/" << std::endl;
    } else {
        std::cout << "\nSome partitions failed to extract." << std::endl;
    }

    // 提示被跳过的危险分区
    if (!unsafe_partitions.empty()) {
        std::cout << "\nNote: The following potentially unsafe partitions were skipped:" << std::endl;
        for (const auto& unsafe : unsafe_partitions) {
            std::cout << "  - " << unsafe << std::endl;
        }
        std::cout << "These partitions may affect device operation if accessed." << std::endl;
    }
}

void listAvailablePartitions(AndroidDirectoryExtractor& extractor) {
    std::cout << "\n=== Available Partitions ===" << std::endl;
    extractor.listAvailablePartitions();

    auto partitions = extractor.getPartitionList();
    if (partitions.empty()) {
        std::cout << "No partitions found or insufficient permissions." << std::endl;
        return;
    }

    std::cout << "\nSafety Analysis:" << std::endl;
    for (const auto& partition : partitions) {
        if (isSafePartition(partition.name)) {
            std::cout << "✓ " << partition.name << " (Safe - "
                      << (partition.size / 1024.0 / 1024.0) << " MB)" << std::endl;
        } else {
            std::cout << "✗ " << partition.name << " (Potentially unsafe - "
                      << (partition.size / 1024.0 / 1024.0) << " MB)" << std::endl;
        }
    }
}

void extractAllSafePartitions(AndroidDirectoryExtractor& extractor) {
    std::cout << "\n=== Extract All Safe Partitions ===" << std::endl;

    auto all_partitions = extractor.getPartitionList();
    std::vector<std::string> safe_partitions;

    for (const auto& partition : all_partitions) {
        if (partition.is_readable && isSafePartition(partition.name)) {
            safe_partitions.push_back(partition.name);
        }
    }

    if (safe_partitions.empty()) {
        std::cout << "No readable safe partitions found." << std::endl;
        return;
    }

    std::cout << "Found " << safe_partitions.size() << " readable safe partitions:" << std::endl;
    for (const auto& name : safe_partitions) {
        std::cout << "  - " << name << std::endl;
    }

    std::cout << "\nProceeding with extraction..." << std::endl;
    handlePartitionExtraction(extractor, safe_partitions);
}

int main(int argc, char** argv) {
    // Initialize console encoding for Windows
    initializeConsoleEncoding();
    std::cout << "=== Android Extractor (Files & Partitions) ===" << std::endl;

    AndroidDirectoryExtractor extractor("./extracted_android_data");

    if (!extractor.initialize()) {
        std::cerr << "Initialization failed. Please check:" << std::endl;
        std::cerr << "1. ADB server is running (adb start-server)" << std::endl;
        std::cerr << "2. Device is connected and USB debugging is enabled" << std::endl;
        std::cerr << "3. Computer is authorized on the device" << std::endl;
        return 1;
    }

    // 解析命令行参数
    if (argc <= 1) {
        showUsage(argv[0]);
        std::cout << "\nDefaulting to file extraction mode with /system/build.prop" << std::endl;
        std::vector<std::string> default_paths = {"/system/build.prop"};
        extractor.extractMultiple(default_paths);
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // 检查是否是分区提取模式
    if (args[0] == "--partition") {
        if (args.size() <= 1) {
            std::cerr << "Error: --partition requires partition names" << std::endl;
            showUsage(argv[0]);
            return 1;
        }

        std::vector<std::string> partitions(args.begin() + 1, args.end());
        handlePartitionExtraction(extractor, partitions);
    }
    else if (args[0] == "--list-partitions") {
        listAvailablePartitions(extractor);
    }
    else if (args[0] == "--extract-safe") {
        extractAllSafePartitions(extractor);
    }
    else if (args[0] == "--help" || args[0] == "-h") {
        showUsage(argv[0]);
    }
    else {
        // 文件提取模式（原有功能）
        std::cout << "\n=== File Extraction Mode ===" << std::endl;
        std::cout << "Extracting files: ";
        for (const auto& path : args) {
            std::cout << path << " ";
        }
        std::cout << std::endl;

        extractor.extractMultiple(args);

        std::cout << "\nTip: Use --partition to extract Android partitions" << std::endl;
        std::cout << "Example: " << argv[0] << " --partition vbmeta boot" << std::endl;
    }

    std::cout << "\nProgram completed. Check './extracted_android_data/' for results." << std::endl;
    return 0;
}