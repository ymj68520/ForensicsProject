#pragma once

#include <string>
#include <vector>
#include <set>

namespace forensics {

/**
 * @brief Helper class to extract text content from files
 * 
 * Supports basic text files and performs string extraction on binary files.
 * Future extension: Integration with poppler for PDF, etc.
 */
class TextExtractor {
public:
    /**
     * @brief Extract text from a file
     * 
     * @param info File information (path, name) to determine extraction strategy
     * @return Extracted content string
     */
    static std::string extract(const std::string& path);

    /**
     * @brief Check if file is likely to contain text based on extension
     */
    static bool isTextFile(const std::string& extension);
    
private:
    static std::string extractFromTextFile(const std::string& path);
    static std::string extractStrings(const std::string& path, size_t minLength = 4);
    
    static const std::set<std::string> textExtensions;
};

}
