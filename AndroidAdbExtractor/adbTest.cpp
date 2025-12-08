#include "adbExtractor.h"
#include <iostream>
#include <vector>

int adbTest(){
    std::cout << "=== Android Directory Extractor (using ADB protocol) ===" << std::endl;

    AndroidDirectoryExtractor extractor("./extracted_android_data");

    if (!extractor.initialize()) {
        std::cerr << "Initialization failed" << std::endl;
        return 1;
    }

    // std::vector<std::string> dirs = {"/data", "/system"};
    // extractor.extractMultiple(dirs);

    // Test with a specific file that should exist on an unrooted device
    std::vector<std::string> test_files = {"/sdcard/test_file.txt"}; 
    std::cout << "\nAttempting to extract: " << test_files[0] << std::endl;
    extractor.extractMultiple(test_files);

    return 0;
}