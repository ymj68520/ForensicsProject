#include <iostream>
#include <string>
#include <filesystem>
#include <memory>
#include "ImageAnalyzer.h"
#include "EventExtractor.h"
#include "FileClassifier.h"

namespace fs = std::filesystem;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <image_path>\n";
    std::cout << "Supported formats: E01, DD (raw)\n";
    std::cout << "Supported systems: Windows, Linux, USB devices\n";
}

std::string getBaseName(const std::string& path) {
    fs::path p(path);
    return p.stem().string();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string imagePath = argv[1];

    // Check if image exists
    if (!fs::exists(imagePath)) {
        std::cerr << "Error: Image file not found: " << imagePath << std::endl;
        return 1;
    }

    std::cout << "=== Forensic Image Analyzer ===" << std::endl;
    std::cout << "Image: " << imagePath << std::endl;
    std::cout << "Using The Sleuth Kit 4.14.0" << std::endl;
    std::cout << std::endl;

    // Generate database names
    std::string baseName = getBaseName(imagePath);
    std::string rawDbPath = baseName + "_raw.db";
    std::string eventDbPath = baseName + "_events.db";
    std::string fileDbPath = baseName + "_files.db";

    try {
        // Step 1: Analyze image and create raw database
        std::cout << "[1/3] Analyzing image and extracting raw data..." << std::endl;
        auto analyzer = std::make_unique<ImageAnalyzer>(imagePath);

        if (!analyzer->analyze()) {
            std::cerr << "Error: Failed to analyze image" << std::endl;
            return 1;
        }

        if (!analyzer->extractToDatabase(rawDbPath)) {
            std::cerr << "Error: Failed to extract data to database" << std::endl;
            return 1;
        }
        std::cout << "✓ Raw database created: " << rawDbPath << std::endl;
        std::cout << std::endl;

        // Step 2: Extract filesystem events
        std::cout << "[2/3] Extracting filesystem events..." << std::endl;
        auto eventExtractor = std::make_unique<EventExtractor>(rawDbPath, eventDbPath);

        if (!eventExtractor->extractEvents()) {
            std::cerr << "Error: Failed to extract events" << std::endl;
            return 1;
        }
        std::cout << "✓ Event database created: " << eventDbPath << std::endl;
        std::cout << std::endl;

        // Step 3: Classify files by type
        std::cout << "[3/3] Classifying files by type..." << std::endl;
        auto fileClassifier = std::make_unique<FileClassifier>(rawDbPath, fileDbPath);

        if (!fileClassifier->classifyAndExtract()) {
            std::cerr << "Error: Failed to classify files" << std::endl;
            return 1;
        }
        std::cout << "✓ File database created: " << fileDbPath << std::endl;
        std::cout << std::endl;

        // Summary
        std::cout << "=== Analysis Complete ===" << std::endl;
        std::cout << "Generated databases:" << std::endl;
        std::cout << "  1. " << rawDbPath << " (Raw TSK data)" << std::endl;
        std::cout << "  2. " << eventDbPath << " (Filesystem events)" << std::endl;
        std::cout << "  3. " << fileDbPath << " (Classified files)" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
