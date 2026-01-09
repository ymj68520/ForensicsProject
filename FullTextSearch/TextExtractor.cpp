#include "TextExtractor.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace fs = std::filesystem;

namespace forensics {

const std::set<std::string> TextExtractor::textExtensions = {
    ".txt", ".log", ".csv", ".json", ".xml", ".html", ".htm", ".css", ".js", 
    ".c", ".cpp", ".h", ".hpp", ".py", ".java", ".md", ".ini", ".conf", ".cfg",
    ".sh", ".bat", ".ps1"
};

std::string TextExtractor::extract(const std::string& path) {
    if (!fs::exists(path)) {
        return "";
    }

    std::string ext = fs::path(path).extension().string();
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    if (isTextFile(ext)) {
        return extractFromTextFile(path);
    } else {
        // Fallback to strings extraction for binary or unknown files
        return extractStrings(path);
    }
}

bool TextExtractor::isTextFile(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    return textExtensions.find(ext) != textExtensions.end();
}

std::string TextExtractor::extractFromTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    // Read entire file into string
    // Use iterator to append
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

std::string TextExtractor::extractStrings(const std::string& path, size_t minLength) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    std::string result;
    std::string currentSequence;
    char c;

    // Basic 'strings' implementation:
    // Collect printable characters. If sequence len >= minLength, append to result.
    while (file.get(c)) {
        if (std::isprint(static_cast<unsigned char>(c)) || c == '\n' || c == '\t') {
            currentSequence += c;
        } else {
            if (currentSequence.length() >= minLength) {
                result += currentSequence + "\n";
            }
            currentSequence.clear();
        }
        
        // Safety limit to prevent massive memory usage for huge binary files
        if (result.length() > 10 * 1024 * 1024) { // 10MB limit
            result += "\n[Truncated - File too large]\n";
            break;
        }
    }

    if (currentSequence.length() >= minLength) {
        result += currentSequence + "\n";
    }

    return result;
}

}
