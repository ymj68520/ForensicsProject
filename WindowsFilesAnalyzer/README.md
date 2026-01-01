# WindowsFilesAnalyzer Module

**Location**: `WindowsFilesAnalyzer/`

## Purpose
This module specializes in the forensic analysis of Windows system artifacts. It extracts key files from forensic images (via `FileExtractor`) and parses them to uncover user activity and system configuration.

## Key Features

### Registry Analysis
- Extracts SAM, SYSTEM, SOFTWARE, and NTUSER.DAT hives
- Parses user accounts and profile information
- Extracts USB device connection history
- Identifies system services and configuration

### Event Log Analysis
- Extracts Windows Event Logs (`.evtx`)
- Focuses on Security, System, and Application logs

### Artifact Analysis
- **Prefetch Files**: Tracks application execution history
- **LNK Files**: Analyzes shortcuts to identify accessed files
- **Jump Lists**: Extracts recent and frequent file access
- **Recycle Bin**: Recovers deleted file metadata ($I files)

### Output
All analysis results are stored in a dedicated SQLite database (`*_windows.db`) with structured tables for each artifact type.

## Usage

The module is integrated into the main `ForensicAnalyzer` application.

```cpp
// Initialize
WindowsFilesAnalyzer analyzer(imagePath, &dbManager);
if (analyzer.initialize()) {
    // Run full analysis
    analyzer.analyzeWindowsData();
}
```

## Dependencies
- `DatabaseManager`: For file metadata and result storage
- `FileExtractor`: For retrieving file content from disk images
- `SQLite3`: For database operations
