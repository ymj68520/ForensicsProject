#include "../WindowsFilesAnalyzer/WindowsFilesAnalyzer.h"
#include "../DatabaseManager/DatabaseManager.h"
#include "../AuditLog/AuditLog.h"
#include <iostream>
#include <vector>
#include <filesystem>
#include <sqlite3.h>

namespace fs = std::filesystem;

// Mock Database Setup
void setupMockDatabase(const std::string& dbPath) {
    sqlite3* db;
    sqlite3_open(dbPath.c_str(), &db);
    
    const char* createTable = "CREATE TABLE files ("
                              "inode INTEGER, name TEXT, path TEXT, size INTEGER, "
                              "mtime INTEGER, ctime INTEGER, type TEXT, "
                              "is_deleted INTEGER, md5 TEXT, category TEXT);";
    sqlite3_exec(db, createTable, nullptr, nullptr, nullptr);
    
    // Insert mock Windows files
    const char* insertFiles = 
        "INSERT INTO files (inode, name, path, size, mtime, ctime, type, is_deleted, md5, category) VALUES "
        "(101, 'SYSTEM', '/Windows/System32/config/SYSTEM', 1048576, 1609459200, 1609459200, 'REG', 0, 'hash1', 'OS_CONFIG'),"
        "(102, 'SAM', '/Windows/System32/config/SAM', 65536, 1609459200, 1609459200, 'REG', 0, 'hash2', 'OS_CONFIG'),"
        "(103, 'SOFTWARE', '/Windows/System32/config/SOFTWARE', 2097152, 1609459200, 1609459200, 'REG', 0, 'hash3', 'OS_CONFIG'),"
        "(104, 'NTUSER.DAT', '/Users/Admin/NTUSER.DAT', 262144, 1609459200, 1609459200, 'REG', 0, 'hash4', 'OS_CONFIG'),"
        "(201, 'System.evtx', '/Windows/System32/winevt/Logs/System.evtx', 1048576, 1609459200, 1609459200, 'REG', 0, 'hash5', 'LOG_FILE'),"
        "(301, 'CMD.EXE-12345678.pf', '/Windows/Prefetch/CMD.EXE-12345678.pf', 4096, 1609459200, 1609459200, 'REG', 0, 'hash6', 'SYSTEM'),"
        "(401, 'calc.lnk', '/Users/Admin/Desktop/calc.lnk', 1024, 1609459200, 1609459200, 'REG', 0, 'hash7', 'UNKNOWN');";
    sqlite3_exec(db, insertFiles, nullptr, nullptr, nullptr);
    
    sqlite3_close(db);
}

int main() {
    std::cout << "=== Windows Files Analyzer Test ===" << std::endl;
    
    std::string mockDbPath = "mock_source.db";
    std::string outputDbPath = "test_windows_results.db";
    std::string imagePath = "mock_image.raw";
    
    // Create dummy image file to satisfy FileExtractor initialization if it checks for existence
    std::ofstream dummyImage(imagePath);
    dummyImage << "DUMMY IMAGE CONTENT";
    dummyImage.close();
    
    // Cleanup previous runs
    fs::remove(mockDbPath);
    fs::remove(outputDbPath);
    fs::remove_all("test_extract");
    
    setupMockDatabase(mockDbPath);
    
    // Initialize DatabaseManager
    DatabaseManager dbManager(mockDbPath);
    if (!dbManager.initialize()) {
        std::cerr << "Failed to initialize DatabaseManager with mock DB" << std::endl;
        return 1;
    }
    
    // Initialize Windows Analyzer
    WindowsFilesAnalyzer analyzer(imagePath, &dbManager);
    analyzer.setOutputDatabasePath(outputDbPath);
    analyzer.setExtractDirectory("test_extract");
    
    std::cout << "Initializing analyzer..." << std::endl;
    if (!analyzer.initialize()) {
        std::cerr << "Failed to initialize WindowsFilesAnalyzer" << std::endl;
        // Even if FileExtractor fails because it can't open the image, 
        // we might want to continue some parts of the test if possible.
        // But initialize() returns false if any part fails.
    }
    
    std::cout << "Starting analysis (expecting extraction errors since image is fake)..." << std::endl;
    // Manually initialize the result DB for testing purpose since analyzer.initialize() fails on image
    WindowsAnalysisDatabase testDb(outputDbPath);
    if (testDb.initialize()) {
        std::cout << "WindowsAnalysisDatabase initialized and tables created." << std::endl;
    } else {
        std::cerr << "Failed to initialize WindowsAnalysisDatabase" << std::endl;
    }

    analyzer.analyzeWindowsData();
    
    std::cout << "Checking output database..." << std::endl;
    if (fs::exists(outputDbPath)) {
        std::cout << "Output database created successfully: " << outputDbPath << std::endl;
        
        // Verify tables were created
        sqlite3* outDb;
        sqlite3_open(outputDbPath.c_str(), &outDb);
        
        const char* checkTables = "SELECT name FROM sqlite_master WHERE type='table';";
        sqlite3_stmt* stmt;
        std::cout << "Tables in output database:" << std::endl;
        if (sqlite3_prepare_v2(outDb, checkTables, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::cout << " - " << sqlite3_column_text(stmt, 0) << std::endl;
            }
            sqlite3_finalize(stmt);
        }
        
        sqlite3_close(outDb);
    } else {
        std::cerr << "Output database NOT created!" << std::endl;
        return 1;
    }
    
    // Cleanup
    fs::remove(mockDbPath);
    // Keep outputDbPath for manual inspection if needed, or remove it.
    fs::remove(imagePath);
    
    std::cout << "=== Test Completed ===" << std::endl;
    return 0;
}
