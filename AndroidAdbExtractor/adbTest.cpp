#include "adbExtractor.h"
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// Initialize Windows console encoding for proper Unicode/Chinese character display
void initializeConsoleEncoding() {
#ifdef _WIN32
    // Set console to UTF-8 encoding
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Also try to enable virtual terminal processing for ANSI escape codes
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    // Set stderr to UTF-8 as well
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hErr, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hErr, dwMode);
        }
    }
#endif
}

int main(int argc, char** argv) {
    // Initialize console encoding for Windows
    initializeConsoleEncoding();
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