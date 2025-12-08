#include "adbExtractor.h"
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::cout << "=== Android Directory Extractor (using ADB protocol) ===" << std::endl;

    AndroidDirectoryExtractor extractor("./extracted_android_data");

    if (!extractor.initialize()) {
        std::cerr << "Initialization failed" << std::endl;
        return 1;
    }

    std::vector<std::string> paths_to_extract;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            paths_to_extract.push_back(argv[i]);
        }
    } else {
        // Default behavior if no arguments provided
        std::cout << "No paths provided. Defaulting to /system/build.prop." << std::endl;
        paths_to_extract.push_back("/system/build.prop");
    }

    std::cout << "\nAttempting to extract: ";
    for (const auto& path : paths_to_extract) {
        std::cout << path << " ";
    }
    std::cout << std::endl;

    extractor.extractMultiple(paths_to_extract);

    return 0;
}