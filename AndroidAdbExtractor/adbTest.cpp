#include "adbExtractor.h"

int adbTest(){
    std::cout << "=== Android目录提取工具 (使用ADB协议) ===" << std::endl;

    AndroidDirectoryExtractor extractor("./extracted_android_data");

    if (!extractor.initialize()) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }

    std::vector<std::string> dirs = {"/data", "/system"};
    extractor.extractMultiple(dirs);

    return 0;
}