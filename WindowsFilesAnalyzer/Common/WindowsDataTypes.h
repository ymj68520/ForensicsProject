// WindowsDataTypes.h
// Data structures for Windows file analysis

#pragma once
#ifndef WINDOWS_DATA_TYPES_H
#define WINDOWS_DATA_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

// Registry value structure
struct RegistryValue {
    std::string hivePath;           // Full path to the hive file
    std::string hiveType;           // SAM, SYSTEM, SOFTWARE, SECURITY, NTUSER
    std::string keyPath;            // Registry key path
    std::string valueName;          // Value name
    std::string valueType;          // REG_SZ, REG_DWORD, REG_BINARY, etc.
    std::string valueData;          // Value data (as string)
    int64_t lastModified;           // Last modification timestamp
    std::string forensicImportance; // HIGH, MEDIUM, LOW
};

// Event log entry structure (EVTX format)
struct EventLogEntry {
    int64_t recordId;               // Record ID
    std::string logSource;          // Security, System, Application, etc.
    int eventId;                    // Event ID
    std::string level;              // Information, Warning, Error, Critical
    int64_t timestamp;              // Event timestamp
    std::string source;             // Event source
    std::string message;            // Event message
    std::string computerName;       // Computer name
    std::string userSid;            // User SID
    std::string channel;            // Log channel
};

// Prefetch file information
struct PrefetchInfo {
    std::string filePath;           // Path to prefetch file
    std::string executableName;     // Executable name
    std::string executablePath;     // Full path to executable
    std::string prefetchHash;       // Prefetch hash
    int runCount;                   // Number of times executed
    int64_t lastRunTime;            // Last execution time
    int64_t creationTime;           // Prefetch file creation time
    std::vector<std::string> referencedFiles; // Files loaded by the executable
    std::vector<std::string> referencedDirectories; // Directories accessed
};

// LNK (shortcut) file information
struct LnkFileInfo {
    std::string lnkPath;            // Path to LNK file
    std::string targetPath;         // Target file path
    std::string workingDirectory;   // Working directory
    std::string arguments;          // Command line arguments
    std::string iconLocation;       // Icon location
    int64_t creationTime;           // Creation time
    int64_t modificationTime;       // Modification time
    int64_t accessTime;             // Access time
    int64_t targetSize;             // Target file size
    std::string driveType;          // Drive type (fixed, removable, network)
    std::string volumeSerial;       // Volume serial number
    std::string netBiosName;        // NetBIOS name (for network shares)
    std::string relativePath;       // Relative path
    std::string description;        // Description/comment
};

// Jump List entry
struct JumpListEntry {
    std::string appId;              // Application ID
    std::string entryPath;          // Target path
    std::string entryName;          // Entry name
    int64_t accessTime;             // Last access time
    int64_t creationTime;           // Creation time
    int accessCount;                // Access count
    bool isPinned;                  // Is pinned to taskbar
};

// Windows user account info (from SAM)
struct WindowsUserInfo {
    int rid;                        // Relative ID
    std::string username;           // Username
    std::string fullName;           // Full name
    std::string comment;            // Account comment
    int64_t lastLogin;              // Last login time
    int64_t passwordLastSet;        // Password last set time
    int64_t accountExpires;         // Account expiration time
    int64_t passwordExpires;        // Password expiration time
    std::string accountFlags;       // Account flags (enabled, disabled, etc.)
    bool isAdmin;                   // Is administrator
    std::string homeDirectory;      // Home directory path
    std::string profilePath;        // User profile path
};

// USB device history
struct USBDeviceInfo {
    std::string vendorId;           // Vendor ID
    std::string productId;          // Product ID
    std::string serialNumber;       // Serial number
    std::string deviceDescription;  // Device description
    std::string friendlyName;       // Friendly name
    std::string deviceClass;        // Device class (storage, etc.)
    int64_t firstConnected;         // First connection time
    int64_t lastConnected;          // Last connection time
    std::string lastDriveLetter;    // Last assigned drive letter
};

// Recycle bin entry
struct RecycleBinEntry {
    std::string recycleFilePath;    // Path within $Recycle.Bin
    std::string originalPath;       // Original file path
    std::string fileName;           // Original file name
    int64_t deletionTime;           // Deletion timestamp
    int64_t originalSize;           // Original file size
    std::string userSid;            // User SID who deleted
};

// Browser artifact (Edge, Chrome, Firefox)
struct BrowserArtifact {
    std::string browserName;        // Browser name
    std::string artifactType;       // history, bookmark, download, cookie, cache
    std::string url;                // URL
    std::string title;              // Page title
    int64_t timestamp;              // Timestamp
    int visitCount;                 // Visit count
    std::string localPath;          // Local file path (for downloads/cache)
    int64_t fileSize;               // File size
};

// NTFS $MFT entry metadata
struct MftEntryInfo {
    int64_t entryNumber;            // MFT entry number
    std::string fileName;           // File name
    std::string filePath;           // Full path
    int64_t parentEntry;            // Parent MFT entry
    int64_t logicalSize;            // Logical file size
    int64_t physicalSize;           // Physical size on disk
    int64_t creationTime;           // Creation time (SI)
    int64_t modificationTime;       // Modification time (SI)
    int64_t accessTime;             // Access time (SI)
    int64_t mftModificationTime;    // MFT modification time (SI)
    int64_t fnCreationTime;         // Creation time (FN)
    int64_t fnModificationTime;     // Modification time (FN)
    bool isDirectory;               // Is directory
    bool isDeleted;                 // Is deleted
    bool hasAds;                    // Has alternate data streams
    std::string permissions;        // File permissions
};

// Windows service information
struct WindowsServiceInfo {
    std::string serviceName;        // Service name
    std::string displayName;        // Display name
    std::string imagePath;          // Executable path
    std::string startType;          // Auto, Manual, Disabled
    std::string serviceType;        // Service type
    std::string accountName;        // Service account
    std::string description;        // Description
    bool isRunning;                 // Current state
};

// Scheduled task information
struct ScheduledTaskInfo {
    std::string taskName;           // Task name
    std::string taskPath;           // Task path
    std::string author;             // Author
    std::string description;        // Description
    std::string actionType;         // Action type
    std::string actionPath;         // Executable/script path
    std::string arguments;          // Arguments
    std::string triggerType;        // Trigger type
    int64_t lastRunTime;            // Last run time
    int64_t nextRunTime;            // Next scheduled run time
    std::string status;             // Enabled, Disabled
    std::string runAs;              // Run as user
};

// Amcache entry (program execution history)
struct AmcacheEntry {
    std::string filePath;           // File path
    std::string fileHash;           // SHA-1 hash
    std::string fileName;           // File name
    std::string companyName;        // Company name
    std::string productName;        // Product name
    std::string productVersion;     // Product version
    std::string fileDescription;    // File description
    int64_t fileSize;               // File size
    int64_t linkTime;               // PE link time
    int64_t lastModified;           // Last modified time
    std::string language;           // Language
};

// SRUM (System Resource Usage Monitor) entry
struct SrumEntry {
    std::string appName;            // Application name
    std::string userName;           // User name
    int64_t timestamp;              // Timestamp
    int64_t bytesReceived;          // Network bytes received
    int64_t bytesSent;              // Network bytes sent
    int64_t foregroundDuration;     // Foreground time (ms)
    int64_t backgroundDuration;     // Background time (ms)
    int64_t cpuTimeMs;              // CPU time in milliseconds
};

#endif // WINDOWS_DATA_TYPES_H
