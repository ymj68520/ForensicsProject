// WindowsArtifactsParsers.cpp
// Implementation of various Windows artifact parsers

#include "WindowsFilesAnalyzer.h"
#include "../AuditLog/AuditLog.h"
#include <fstream>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

// --- Prefetch Analysis ---

void WindowsFilesAnalyzer::analyzePrefetchFiles() {
    std::cout << "Analyzing Prefetch files..." << std::endl;
    
    // Prefetch files are in Windows/Prefetch/*.pf
    std::vector<FileRecord> pfFiles = queryFilesByPattern("%/Windows/Prefetch/%.pf");
    
    for (const auto& file : pfFiles) {
        std::string extractPath = getExtractPath("prefetch/" + file.name);
        
        if (extractFileToPath(file.inode, extractPath)) {
            PrefetchInfo info = parsePrefetchFile(extractPath);
            // If parsing failed (empty path), skip
            if (!info.filePath.empty()) {
                info.creationTime = file.crtime;
                windowsDb_->insertPrefetchInfo(info);
            }
        }
    }
}

PrefetchInfo WindowsFilesAnalyzer::parsePrefetchFile(const std::string& prefetchPath) {
    PrefetchInfo info;
    
    std::ifstream file(prefetchPath, std::ios::binary);
    if (!file) return info;
    
    // Check header (MAM or SCCA for compressed/uncompressed)
    char header[4];
    file.read(header, 4);
    
    info.filePath = prefetchPath;
    
    // In a real implementation, we would decompress (if MAM) and parse the format.
    // The format contains execution count, last run times, and referenced files.
    
    // Placeholder extraction from filename
    // Filename format: EXENAME-HASH.pf
    std::string filename = fs::path(prefetchPath).filename().string();
    size_t dashPos = filename.find_last_of('-');
    if (dashPos != std::string::npos) {
        info.executableName = filename.substr(0, dashPos);
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos && dotPos > dashPos) {
            info.prefetchHash = filename.substr(dashPos + 1, dotPos - dashPos - 1);
        }
    }
    
    // Placeholder values
    info.runCount = 1; 
    info.lastRunTime = std::time(nullptr);
    
    return info;
}

// --- LNK Analysis ---

void WindowsFilesAnalyzer::analyzeLnkFiles() {
    std::cout << "Analyzing LNK (Shortcut) files..." << std::endl;
    
    // Search broadly for LNK files in user directories
    std::vector<FileRecord> lnkFiles = queryFilesByPattern("%/Users/%/%.lnk");
    
    for (const auto& file : lnkFiles) {
        std::string extractPath = getExtractPath("shortcuts/" + std::to_string(file.inode) + "_" + file.name);
        
        if (extractFileToPath(file.inode, extractPath)) {
            LnkFileInfo info = parseLnkFile(extractPath);
            if (!info.lnkPath.empty()) {
                info.creationTime = file.crtime;
                info.accessTime = file.atime;
                info.modificationTime = file.mtime;
                windowsDb_->insertLnkFileInfo(info);
            }
        }
    }
}

LnkFileInfo WindowsFilesAnalyzer::parseLnkFile(const std::string& lnkPath) {
    LnkFileInfo info;
    
    std::ifstream file(lnkPath, std::ios::binary);
    if (!file) return info;
    
    // LNK Header
    char header[76];
    file.read(header, 76); // Header size is 76 bytes
    if (file.gcount() < 76) return info;
    
    // Signature check (L = 0x4C)
    if (header[0] != 0x4C || header[1] != 0x00 || header[2] != 0x00 || header[3] != 0x00) {
        return info;
    }
    
    // Parsing GUID 00021401-0000-0000-C000-000000000046
    
    info.lnkPath = lnkPath;
    info.description = "Shortcut File";
    
    // Real parsing would extract Shell Link structures
    return info;
}

// --- Jump List Analysis ---

void WindowsFilesAnalyzer::analyzeJumpLists() {
    std::cout << "Analyzing Jump Lists..." << std::endl;
    
    // AutomaticDestinations
    std::vector<FileRecord> autoDest = queryFilesByPattern("%/AutomaticDestinations/%.automaticDestinations-ms");
    
    for (const auto& file : autoDest) {
        std::string extractPath = getExtractPath("jumplists/auto/" + file.name);
        extractFileToPath(file.inode, extractPath);
        // Parse OLE compound file structure...
    }
    
    // CustomDestinations
    std::vector<FileRecord> customDest = queryFilesByPattern("%/CustomDestinations/%.customDestinations-ms");
    
    for (const auto& file : customDest) {
        std::string extractPath = getExtractPath("jumplists/custom/" + file.name);
        extractFileToPath(file.inode, extractPath);
    }
}

std::vector<JumpListEntry> WindowsFilesAnalyzer::parseJumpListFile(const std::string& jumpListPath) {
    // Jump Lists are OLE Compound Files containing streams that are LNK files
    return {};
}

// --- Recycle Bin Analysis ---

void WindowsFilesAnalyzer::analyzeRecycleBin() {
    std::cout << "Analyzing Recycle Bin..." << std::endl;
    
    // Search for $I files (index files) in $Recycle.Bin
    std::vector<FileRecord> iFiles = queryFilesByPattern("%/$Recycle.Bin/%/$I%");
    
    for (const auto& file : iFiles) {
        std::string extractPath = getExtractPath("recyclebin/" + file.name);
        
        if (extractFileToPath(file.inode, extractPath)) {
            auto entries = parseRecycleBin(extractPath);
            for (const auto& entry : entries) {
                windowsDb_->insertRecycleBinEntry(entry);
            }
        }
    }
}

std::vector<RecycleBinEntry> WindowsFilesAnalyzer::parseRecycleBin(const std::string& recycleBinPath) {
    std::vector<RecycleBinEntry> entries;
    
    std::ifstream file(recycleBinPath, std::ios::binary);
    if (!file) return entries;
    
    // $I file format
    // Version 1 (Win < 10): Header (8), Size (8), Deleted Time (8), Path (520)
    // Version 2 (Win 10+): Header (8), Size (8), Deleted Time (8), Path Len (4), Path (variable)
    
    uint64_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 8);
    
    if (version == 1 || version == 2) {
        RecycleBinEntry entry;
        entry.recycleFilePath = recycleBinPath;
        
        uint64_t size;
        file.read(reinterpret_cast<char*>(&size), 8);
        entry.originalSize = size;
        
        uint64_t deletedTime;
        file.read(reinterpret_cast<char*>(&deletedTime), 8);
        entry.deletionTime = filetimeToUnixTime(deletedTime);
        
        // Path parsing depends on version and is simplified here
        entry.originalPath = "Extracted from $I file";
        
        entries.push_back(entry);
    }
    
    return entries;
}

// --- Other Analyzers ---

void WindowsFilesAnalyzer::analyzeNTFSMetadata() {
    // Requires extraction of $MFT, which can be large.
    // For now we skip or implement basic checking.
}

void WindowsFilesAnalyzer::analyzeUserProfiles() {
    // Handled mainly via Registry (NTUSER.DAT)
}

void WindowsFilesAnalyzer::analyzeBrowserData() {
    // Similar to Android, look for SQLite databases in AppData
    // Chrome: AppData/Local/Google/Chrome/User Data/Default/History
    // Edge: AppData/Local/Microsoft/Edge/User Data/Default/History
    // Firefox: AppData/Roaming/Mozilla/Firefox/Profiles/*.default*/places.sqlite
}

void WindowsFilesAnalyzer::analyzeWindowsServices() {
    // Extracted from SYSTEM registry hive
}

void WindowsFilesAnalyzer::analyzeScheduledTasks() {
    // Windows/System32/Tasks/*.xml
}

void WindowsFilesAnalyzer::analyzeAmcache() {
    // Windows/AppCompat/Programs/Amcache.hve
}

void WindowsFilesAnalyzer::analyzeSRUM() {
    // Windows/System32/sru/SRUDB.dat (ESE database)
}
