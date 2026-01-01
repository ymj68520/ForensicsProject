// WindowsRegistryParser.cpp
// Implementation of Registry parsing logic

#include "WindowsFilesAnalyzer.h"
#include "../AuditLog/AuditLog.h"
#include <fstream>
#include <cstring>

void WindowsFilesAnalyzer::analyzeRegistryHives() {
    std::cout << "Analyzing Registry Hives..." << std::endl;
    
    // 1. Analyze SYSTEM Hive
    // System hive is usually at Windows/System32/config/SYSTEM
    std::vector<FileRecord> systemHives = queryFilesByPattern("%/System32/config/SYSTEM");
    for (const auto& hive : systemHives) {
        std::string extractPath = getExtractPath("registry/" + std::to_string(hive.inode) + "_SYSTEM");
        std::cout << "  Found SYSTEM hive: " << hive.path << std::endl;
        
        if (extractFileToPath(hive.inode, extractPath)) {
            auto values = parseRegistryHive(extractPath, "SYSTEM");
            windowsDb_->insertRegistryValues(values);
            
            // Also parse specific artifacts from SYSTEM
            auto usbDevices = parseUSBDevicesFromRegistry(extractPath);
            for (const auto& usb : usbDevices) {
                windowsDb_->insertUSBDevice(usb);
            }
            
            auto services = parseServicesFromRegistry(extractPath);
            for (const auto& svc : services) {
                windowsDb_->insertWindowsService(svc);
            }
        }
    }
    
    // 2. Analyze SAM Hive
    // SAM hive is usually at Windows/System32/config/SAM
    std::vector<FileRecord> samHives = queryFilesByPattern("%/System32/config/SAM");
    for (const auto& hive : samHives) {
        std::string extractPath = getExtractPath("registry/" + std::to_string(hive.inode) + "_SAM");
        std::cout << "  Found SAM hive: " << hive.path << std::endl;
        
        if (extractFileToPath(hive.inode, extractPath)) {
            auto values = parseRegistryHive(extractPath, "SAM");
            windowsDb_->insertRegistryValues(values);
            
            auto users = parseUserAccountsFromSAM(extractPath);
            for (const auto& user : users) {
                windowsDb_->insertUserInfo(user);
            }
        }
    }
    
    // 3. Analyze SOFTWARE Hive
    std::vector<FileRecord> softwareHives = queryFilesByPattern("%/System32/config/SOFTWARE");
    for (const auto& hive : softwareHives) {
        std::string extractPath = getExtractPath("registry/" + std::to_string(hive.inode) + "_SOFTWARE");
        
        if (extractFileToPath(hive.inode, extractPath)) {
            auto values = parseRegistryHive(extractPath, "SOFTWARE");
            windowsDb_->insertRegistryValues(values);
        }
    }
    
    // 4. Analyze NTUSER.DAT (User Registry)
    // Located in Users/<Username>/NTUSER.DAT
    std::vector<FileRecord> userHives = queryFilesByPattern("%/Users/%/NTUSER.DAT");
    for (const auto& hive : userHives) {
        std::string extractPath = getExtractPath("registry/users/" + std::to_string(hive.inode) + "_NTUSER.DAT");
        std::cout << "  Found User hive: " << hive.path << std::endl;
        
        if (extractFileToPath(hive.inode, extractPath)) {
            auto values = parseRegistryHive(extractPath, "NTUSER");
            windowsDb_->insertRegistryValues(values);
        }
    }
}

std::vector<RegistryValue> WindowsFilesAnalyzer::parseRegistryHive(const std::string& hivePath, 
                                              const std::string& hiveType) {
    std::vector<RegistryValue> values;
    
    // In a real implementation, we would use a registry parser library (like libregf or hivex)
    // to traverse the registry structure.
    // simpler "strings" based approach or header validation could represent a placeholder.
    
    // For this implementation, we will perform a basic check and return an empty list
    // or simulate some data if needed.
    
    std::ifstream file(hivePath, std::ios::binary);
    if (!file) return values;
    
    // Read header (registry hive header starts with "regf")
    char header[4];
    file.read(header, 4);
    if (strncmp(header, "regf", 4) != 0) {
        return values; // Not a valid registry hive
    }
    
    // Placeholder: record that we found a valid hive
    RegistryValue val;
    val.hivePath = hivePath;
    val.hiveType = hiveType;
    val.keyPath = "ROOT";
    val.valueName = "(Default)";
    val.valueType = "REG_SZ";
    val.valueData = "Hive successfully identified";
    val.forensicImportance = "INFO";
    val.lastModified = 0;
    
    values.push_back(val);
    return values;
}

std::vector<WindowsUserInfo> WindowsFilesAnalyzer::parseUserAccountsFromSAM(const std::string& samPath) {
    std::vector<WindowsUserInfo> users;
    // Placeholder implementation
    // Parsing binary SAM structure is complex.
    return users;
}

std::vector<USBDeviceInfo> WindowsFilesAnalyzer::parseUSBDevicesFromRegistry(const std::string& systemPath) {
    std::vector<USBDeviceInfo> devices;
    // Placeholder implementation
    // Would parse ControlSet001/Enum/USBSTOR
    return devices;
}

std::vector<WindowsServiceInfo> WindowsFilesAnalyzer::parseServicesFromRegistry(const std::string& systemPath) {
    std::vector<WindowsServiceInfo> services;
    // Placeholder implementation
    // Would parse ControlSet001/Services
    return services;
}
