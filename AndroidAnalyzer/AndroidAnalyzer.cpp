#include "AndroidAnalyzer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

AndroidAnalyzer::AndroidAnalyzer(const std::string& imagePath, DatabaseManager* dbManager)
    : imagePath_(imagePath), dbManager_(dbManager) {
}

AndroidAnalyzer::~AndroidAnalyzer() {
}

bool AndroidAnalyzer::initialize() {
    fileExtractor_ = std::make_unique<FileExtractor>(imagePath_, dbManager_->getDbPath());
    if (!fileExtractor_->initialize()) {
        std::cerr << "Failed to initialize FileExtractor" << std::endl;
        return false;
    }
    return true;
}

void AndroidAnalyzer::analyzeAndroidData() {
    std::cout << "Starting Android data analysis..." << std::endl;

    // Define known Android app data paths
    std::vector<std::pair<std::string, std::string>> knownApps = {
        {"data/data/com.android.providers.telephony/databases/mmssms.db", "SMS"},
        {"data/data/com.android.providers.contacts/databases/contacts2.db", "Contacts"},
        {"data/data/com.android.providers.contacts/databases/calllog.db", "CallLog"},
        {"data/data/com.whatsapp/databases/msgstore.db", "WhatsApp"},
        {"data/data/com.whatsapp/databases/wa.db", "WhatsApp"}
    };

    for (const auto& app : knownApps) {
        std::string tempPath = "/tmp/" + std::filesystem::path(app.first).filename().string();
        if (extractAndParseDB(app.first, tempPath)) {
            AndroidAppData data;
            data.packageName = app.first.substr(0, app.first.find_last_of('/'));
            data.dbPath = app.first;
            data.dataType = app.second;

            if (app.second == "SMS") {
                parseSMS(tempPath);
            } else if (app.second == "Contacts") {
                parseContacts(tempPath);
            } else if (app.second == "CallLog") {
                parseCallLog(tempPath);
            } else if (app.second == "WhatsApp") {
                parseWhatsApp(tempPath);
            }

            // Clean up temp file
            std::filesystem::remove(tempPath);
        }
    }

    std::cout << "Android data analysis completed." << std::endl;
}

bool AndroidAnalyzer::extractAndParseDB(const std::string& dbPathInImage, const std::string& tempPath) {
    // Extract the database file
    if (!fileExtractor_->extractFileByPath(dbPathInImage, tempPath)) {
        std::cerr << "Failed to extract database: " << dbPathInImage << std::endl;
        return false;
    }

    return true;
}

void AndroidAnalyzer::parseSMS(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open SMS database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, address, body, date, type FROM sms;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "SMS";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["address"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["body"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseContacts(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open Contacts database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, display_name, data1 FROM raw_contacts JOIN data ON raw_contacts._id = data.raw_contact_id WHERE data.mimetype_id = (SELECT _id FROM mimetypes WHERE mimetype = 'vnd.android.cursor.item/phone_v2');";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "Contacts";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["display_name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["phone"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseCallLog(const std::string& dbPath) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open CallLog database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::string sql = "SELECT _id, number, date, duration, type FROM calls;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "CallLog";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["number"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["date"] = std::to_string(sqlite3_column_int64(stmt, 2));
            record["duration"] = std::to_string(sqlite3_column_int(stmt, 3));
            record["type"] = std::to_string(sqlite3_column_int(stmt, 4));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseWhatsApp(const std::string& dbPath) {
    // WhatsApp parsing - simplified example
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open WhatsApp database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    // Example: parse messages (this is a simplified version)
    std::string sql = "SELECT _id, key_remote_jid, data, timestamp FROM messages LIMIT 100;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        AndroidAppData data;
        data.dataType = "WhatsApp";
        data.dbPath = dbPath;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::map<std::string, std::string> record;
            record["id"] = std::to_string(sqlite3_column_int(stmt, 0));
            record["remote_jid"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record["data"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record["timestamp"] = std::to_string(sqlite3_column_int64(stmt, 3));
            data.records.push_back(record);
        }
        sqlite3_finalize(stmt);

        insertParsedData(data);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::parseGenericAppData(const std::string& packageName, const std::string& dbPath) {
    // Generic parsing for other apps - list tables and sample data
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        return;
    }

    // Get table names
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::cout << "Found table in " << packageName << ": " << tableName << std::endl;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
}

void AndroidAnalyzer::insertParsedData(const AndroidAppData& data) {
    std::cout << "Parsed " << data.records.size() << " records from " << data.dataType << std::endl;

    // Insert into database or output to file
    // For now, just print to console
    for (const auto& record : data.records) {
        std::cout << data.dataType << " Record: ";
        for (const auto& field : record) {
            std::cout << field.first << "=" << field.second << " ";
        }
        std::cout << std::endl;
    }
}