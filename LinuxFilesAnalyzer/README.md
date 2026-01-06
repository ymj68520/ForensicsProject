# LinuxFilesAnalyzer Module

**Location**: `LinuxFilesAnalyzer/`

## Purpose
This module specializes in the forensic analysis of Linux system artifacts. It extracts key files from forensic images (via `FileExtractor`) and parses them to uncover user activity, system configuration, and security-related information.

## Key Features

### Log Analysis
- Parses system logs (`/var/log/syslog`, `/var/log/messages`)
- Parses authentication logs (`/var/log/auth.log`, `/var/log/secure`)
- Parses kernel logs (`/var/log/kern.log`, `dmesg`)
- Parses application logs (`/var/log/dpkg.log`, apt history)

### User and Authentication Analysis
- Parses `/etc/passwd`, `/etc/shadow`, `/etc/group`
- Parses login records (`wtmp`, `btmp`, `lastlog`)
- Extracts SSH authorized keys and known hosts

### Shell History Analysis
- Parses Bash history (`~/.bash_history`)
- Parses Zsh history (`~/.zsh_history`)
- Parses Fish history (`~/.local/share/fish/fish_history`)

### System Configuration Analysis
- Parses cron jobs (`/etc/crontab`, user crontabs)
- Analyzes installed packages (`/var/lib/dpkg/status`)
- Detects browser profiles (Chrome, Firefox)

### Output
All analysis results are stored in a dedicated SQLite database (`*_linux.db`) with 15+ structured tables for each artifact type.

## Usage

The module is integrated into the main `ForensicAnalyzer` application.

```cpp
// Initialize
LinuxFilesAnalyzer analyzer(imagePath, &dbManager);
if (analyzer.initialize()) {
    // Run full analysis
    analyzer.analyzeLinuxData();
}
```

## Dependencies
- `DatabaseManager`: For file metadata and result storage
- `FileExtractor`: For retrieving file content from disk images
- `SQLite3`: For database operations
