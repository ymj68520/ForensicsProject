// WindowsEventLogParser.cpp
// Implementation of Event Log (EVTX) parsing logic

#include "WindowsFilesAnalyzer.h"
#include "../AuditLog/AuditLog.h"
#include <fstream>
#include <cstring>
#include <ctime>

void WindowsFilesAnalyzer::analyzeEventLogs() {
    std::cout << "Analyzing Event Logs..." << std::endl;
    
    // Search for EVTX files in System32/winevt/Logs
    std::vector<FileRecord> logFiles = queryFilesByPattern("%/System32/winevt/Logs/%.evtx");
    
    int processedCount = 0;
    for (const auto& logFile : logFiles) {
        // We mainly care about Security, System, and Application logs, but will process all if found
        // Checking filename can help prioritize
        std::string filename = logFile.name;
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        
        // Skip small files (empty logs)
        if (logFile.size < 4096) continue;
        
        std::string extractPath = getExtractPath("eventlogs/" + filename);
        
        if (extractFileToPath(logFile.inode, extractPath)) {
            auto entries = parseEventLog(extractPath);
            windowsDb_->insertEventLogEntries(entries);
            processedCount++;
        }
    }
    
    std::cout << "  Processed " << processedCount << " event log files." << std::endl;
}

std::vector<EventLogEntry> WindowsFilesAnalyzer::parseEventLog(const std::string& logPath) {
    std::vector<EventLogEntry> entries;
    
    // EVTX file format parsing is complex (chunks, compression, etc.)
    // Here we implement basic validation and a placeholder entry
    
    std::ifstream file(logPath, std::ios::binary);
    if (!file) return entries;
    
    // Check header ("ElfFile\0")
    char header[8];
    file.read(header, 8);
    if (strncmp(header, "ElfFile", 7) != 0) {
        return entries;
    }
    
    // Placeholder: In a real implementation we would iterate through chunks and records
    // validating the format headers.
    
    // Create a dummy summary entry for the file presence
    EventLogEntry entry;
    entry.recordId = 0;
    entry.logSource = "Analyzer";
    entry.eventId = 0;
    entry.level = "Info";
    entry.timestamp = std::time(nullptr);
    entry.source = "WindowsFilesAnalyzer";
    entry.message = "Event Log file found and extracted: " + logPath;
    entry.computerName = "UNKNOWN";
    entry.userSid = "S-1-0-0";
    entry.channel = "Unknown";
    
    entries.push_back(entry);
    
    return entries;
}
