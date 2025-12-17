#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "../HTTPServer/HTTPServerEnhanced.h"

using namespace forensics;
using json = nlohmann::json;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Ge;
using ::testing::Le;

class SQLiteHelperEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test database
        test_db_path_ = std::filesystem::temp_directory_path() / "test_enhanced.db";

        // Initialize database with test data
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(test_db_path_.c_str(), &db), SQLITE_OK);

        // Create test tables
        const char* create_tables_sql[] = {
            "CREATE TABLE files ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL,"
            "path TEXT NOT NULL,"
            "size INTEGER,"
            "created_time INTEGER,"
            "modified_time INTEGER,"
            "file_type TEXT,"
            "is_deleted BOOLEAN DEFAULT 0,"
            "md5_hash TEXT"
            ")",

            "CREATE TABLE events ("
            "id INTEGER PRIMARY KEY,"
            "file_id INTEGER,"
            "event_type TEXT NOT NULL,"
            "timestamp INTEGER NOT NULL,"
            "file_path TEXT,"
            "user_id INTEGER"
            ")",

            "CREATE TABLE images ("
            "id INTEGER PRIMARY KEY,"
            "original_file_id INTEGER,"
            "file_path TEXT NOT NULL,"
            "file_size INTEGER,"
            "file_extension TEXT,"
            "mime_type TEXT,"
            "created_time INTEGER,"
            "modified_time INTEGER"
            ")",

            "CREATE INDEX idx_files_path ON files(path)",
            "CREATE INDEX idx_events_timestamp ON events(timestamp)",
            "CREATE INDEX idx_images_extension ON images(file_extension)",

            "CREATE VIEW file_summary AS"
            "SELECT 'images' as category, COUNT(*) as file_count, SUM(file_size) as total_size FROM images"
            "UNION ALL"
            "SELECT 'events' as category, COUNT(*) as file_count, 0 as total_size FROM events"
        };

        for (const char* sql : create_tables_sql) {
            ASSERT_EQ(sqlite3_exec(db, sql, nullptr, nullptr, nullptr), SQLITE_OK);
        }

        // Insert test data
        const char* insert_data_sql[] = {
            "INSERT INTO files (id, name, path, size, created_time, modified_time, file_type, md5_hash) VALUES"
            "(1, 'test.jpg', '/path/to/test.jpg', 1024, 1609459200, 1609459200, 'jpg', 'd41d8cd98f00b204e9800998ecf8427e'),"
            "(2, 'document.pdf', '/path/to/document.pdf', 2048, 1609462800, 1609462800, 'pdf', '098f6bcd4621d373cade4e832627b4f6'),"
            "(3, 'deleted_file.txt', '/path/to/deleted_file.txt', 512, 1609466400, 1609466400, 'txt', '5d41402abc4b2a76b9719d911017c592')",

            "INSERT INTO events (id, file_id, event_type, timestamp, file_path, user_id) VALUES"
            "(1, 1, 'create', 1609459200, '/path/to/test.jpg', 1000),"
            "(2, 2, 'create', 1609462800, '/path/to/document.pdf', 1001),"
            "(3, 3, 'delete', 1609470000, '/path/to/deleted_file.txt', 1002),"
            "(4, 1, 'modify', 1609473600, '/path/to/test.jpg', 1000)",

            "INSERT INTO images (id, original_file_id, file_path, file_size, file_extension, mime_type, created_time, modified_time) VALUES"
            "(1, 1, '/path/to/test.jpg', 1024, 'jpg', 'image/jpeg', 1609459200, 1609459200),"
            "(2, 4, '/path/to/photo.png', 2048, 'png', 'image/png', 1609462800, 1609462800),"
            "(3, 5, '/path/to/screenshot.gif', 512, 'gif', 'image/gif', 1609466400, 1609466400)"
        };

        for (const char* sql : insert_data_sql) {
            ASSERT_EQ(sqlite3_exec(db, sql, nullptr, nullptr, nullptr), SQLITE_OK);
        }

        sqlite3_close(db);
    }

    void TearDown() override {
        // Clean up test database
        std::filesystem::remove(test_db_path_);
    }

    std::filesystem::path test_db_path_;
};

// Test ApiResponse creation and serialization
TEST_F(SQLiteHelperEnhancedTest, ApiResponseCreation) {
    // Test success response
    auto success_resp = ApiResponse::success("Test success", json{{"key", "value"}});
    EXPECT_TRUE(success_resp.success);
    EXPECT_EQ(success_resp.message, "Test success");
    EXPECT_TRUE(success_resp.data.contains("key"));
    EXPECT_EQ(success_resp.data["key"], "value");
    EXPECT_FALSE(success_resp.timestamp.empty());

    auto success_json = success_resp.to_json();
    EXPECT_TRUE(success_json["success"]);
    EXPECT_EQ(success_json["message"], "Test success");
    EXPECT_EQ(success_json["data"]["key"], "value");

    // Test error response
    auto error_resp = ApiResponse::error("Test error", "TEST_ERROR");
    EXPECT_FALSE(error_resp.success);
    EXPECT_EQ(error_resp.message, "Test error");
    EXPECT_EQ(error_resp.error_code, "TEST_ERROR");

    auto error_json = error_resp.to_json();
    EXPECT_FALSE(error_json["success"]);
    EXPECT_EQ(error_json["message"], "Test error");
    EXPECT_EQ(error_json["error_code"], "TEST_ERROR");
}

// Test database schema extraction
TEST_F(SQLiteHelperEnhancedTest, GetDatabaseSchema) {
    auto schema = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());

    EXPECT_TRUE(schema.contains("tables"));
    EXPECT_TRUE(schema.contains("views"));

    auto tables = schema["tables"];
    EXPECT_TRUE(tables.is_array());
    EXPECT_GE(tables.size(), 3); // Should have at least files, events, images

    // Check files table structure
    bool found_files_table = false;
    for (const auto& table : tables) {
        if (table["name"] == "files") {
            found_files_table = true;
            EXPECT_EQ(table["type"], "table");
            EXPECT_TRUE(table.contains("columns"));

            auto columns = table["columns"];
            EXPECT_TRUE(columns.is_array());

            // Check for essential columns
            bool found_id = false, found_name = false, found_path = false;
            for (const auto& column : columns) {
                if (column["name"] == "id") {
                    found_id = true;
                    EXPECT_TRUE(column["primary_key"]);
                }
                if (column["name"] == "name") {
                    found_name = true;
                    EXPECT_EQ(column["data_type"], "TEXT");
                }
                if (column["name"] == "path") {
                    found_path = true;
                    EXPECT_EQ(column["data_type"], "TEXT");
                }
            }
            EXPECT_TRUE(found_id);
            EXPECT_TRUE(found_name);
            EXPECT_TRUE(found_path);
        }
    }
    EXPECT_TRUE(found_files_table);

    // Check views
    auto views = schema["views"];
    EXPECT_TRUE(views.is_array());
    EXPECT_GE(views.size(), 1); // Should have at least file_summary view
}

// Test database schema with table pattern filter
TEST_F(SQLiteHelperEnhancedTest, GetDatabaseSchemaWithPattern) {
    auto schema = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string(), "files%");

    EXPECT_TRUE(schema.contains("tables"));
    auto tables = schema["tables"];

    // Should only return tables matching the pattern
    for (const auto& table : tables) {
        std::string table_name = table["name"];
        EXPECT_TRUE(table_name.find("files") == 0);
    }
}

// Test database information extraction
TEST_F(SQLiteHelperEnhancedTest, GetDatabaseInfo) {
    auto info = SQLiteHelperEnhanced::get_database_info(test_db_path_.string());

    EXPECT_TRUE(info.contains("page_count"));
    EXPECT_TRUE(info.contains("page_size"));
    EXPECT_TRUE(info.contains("table_count"));
    EXPECT_TRUE(info.contains("view_count"));
    EXPECT_TRUE(info.contains("index_count"));

    EXPECT_GE(info["page_count"], 0);
    EXPECT_GT(info["page_size"], 0);
    EXPECT_GE(info["table_count"], 0);
    EXPECT_GE(info["view_count"], 0);
    EXPECT_GE(info["index_count"], 0);

    if (info.contains("estimated_size_bytes")) {
        EXPECT_GE(info["estimated_size_bytes"], 0);
    }
}

// Test data export in JSON format
TEST_F(SQLiteHelperEnhancedTest, ExportDataJson) {
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 10, 0);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("data"));
    EXPECT_TRUE(result.contains("total_exported"));
    EXPECT_TRUE(result.contains("total_available"));
    EXPECT_TRUE(result.contains("limit"));
    EXPECT_TRUE(result.contains("offset"));

    auto data = result["data"];
    EXPECT_TRUE(data.is_array());
    EXPECT_GE(result["total_exported"], 0);
    EXPECT_GE(result["total_available"], 0);
    EXPECT_EQ(result["limit"], 10);
    EXPECT_EQ(result["offset"], 0);

    // Check structure of exported data
    if (data.size() > 0) {
        auto first_row = data[0];
        EXPECT_TRUE(first_row.contains("id"));
        EXPECT_TRUE(first_row.contains("name"));
        EXPECT_TRUE(first_row.contains("path"));
    }
}

// Test data export in CSV format
TEST_F(SQLiteHelperEnhancedTest, ExportDataCsv) {
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "csv", 5, 0);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("csv_content"));
    EXPECT_TRUE(result.contains("total_exported"));

    auto csv_content = result["csv_content"].get<std::string>();
    EXPECT_FALSE(csv_content.empty());

    // Check CSV header
    EXPECT_TRUE(csv_content.find("id,") == 0); // Should start with id column
    EXPECT_TRUE(csv_content.find("name,") != std::string::npos);
    EXPECT_TRUE(csv_content.find("path,") != std::string::npos);

    // Check CSV data rows
    auto lines = std::vector<std::string>{};
    std::stringstream ss(csv_content);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    EXPECT_GE(lines.size(), 1); // Header + at least one data row
}

// Test data export in XML format
TEST_F(SQLiteHelperEnhancedTest, ExportDataXml) {
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "xml", 5, 0);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("xml_content"));
    EXPECT_TRUE(result.contains("total_exported"));

    auto xml_content = result["xml_content"].get<std::string>();
    EXPECT_FALSE(xml_content.empty());

    // Check XML structure
    EXPECT_TRUE(xml_content.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") == 0);
    EXPECT_TRUE(xml_content.find("<data>") != std::string::npos);
    EXPECT_TRUE(xml_content.find("<table name=\"files\">") != std::string::npos);
    EXPECT_TRUE(xml_content.find("</table>") != std::string::npos);
    EXPECT_TRUE(xml_content.find("</data>") != std::string::npos);
}

// Test export with invalid table name (SQL injection protection)
TEST_F(SQLiteHelperEnhancedTest, ExportDataInvalidTableName) {
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files; DROP TABLE files;", "json");

    EXPECT_TRUE(result.contains("error"));
    EXPECT_EQ(result["success"], false);
    EXPECT_EQ(result["error_code"], "INVALID_TABLE");
}

// Test export with unsupported format
TEST_F(SQLiteHelperEnhancedTest, ExportDataUnsupportedFormat) {
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "unsupported");

    EXPECT_TRUE(result.contains("error"));
    EXPECT_EQ(result["success"], false);
    EXPECT_EQ(result["error_code"], "UNSUPPORTED_FORMAT");
}

// Test export pagination
TEST_F(SQLiteHelperEnhancedTest, ExportDataPagination) {
    // Export first page
    auto page1 = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 1, 0);
    auto page2 = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 1, 1);

    EXPECT_EQ(page1["limit"], 1);
    EXPECT_EQ(page1["offset"], 0);
    EXPECT_EQ(page2["limit"], 1);
    EXPECT_EQ(page2["offset"], 1);

    if (page1["total_exported"] > 0 && page2["total_exported"] > 0) {
        // Verify different records are returned
        auto data1 = page1["data"];
        auto data2 = page2["data"];
        EXPECT_NE(data1[0]["id"], data2[0]["id"]);
    }
}

// Test cache functionality
TEST_F(SQLiteHelperEnhancedTest, CacheFunctionality) {
    // Clear cache first
    SQLiteHelperEnhanced::clear_cache();

    // First call should compute result
    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Second call should use cache
    auto start2 = std::chrono::high_resolution_clock::now();
    auto result2 = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Results should be identical
    EXPECT_EQ(result1.dump(), result2.dump());

    // Second call should be faster (though this might not always be true due to system timing)
    // In a real test environment, you'd want to test with a more expensive query
}

// Test cache statistics
TEST_F(SQLiteHelperEnhancedTest, CacheStatistics) {
    SQLiteHelperEnhanced::clear_cache();

    // Perform some cached queries
    SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
    SQLiteHelperEnhanced::get_database_info(test_db_path_.string());

    auto stats = SQLiteHelperEnhanced::get_cache_stats();
    EXPECT_TRUE(stats.contains("cached_queries"));
    EXPECT_TRUE(stats.contains("expired_entries"));
    EXPECT_GE(stats["cached_queries"], 0);
    EXPECT_GE(stats["expired_entries"], 0);
}

// Test connection pool (basic functionality)
TEST_F(SQLiteHelperEnhancedTest, ConnectionPool) {
    // Create multiple concurrent requests to test connection pool
    std::vector<std::thread> threads;
    std::vector<json> results(5);

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &results, i]() {
            results[i] = SQLiteHelperEnhanced::get_database_info(test_db_path_.string());
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All results should be valid
    for (const auto& result : results) {
        EXPECT_TRUE(result.contains("page_count"));
        EXPECT_TRUE(result.contains("table_count"));
    }
}

// Test query timeout functionality (simulated)
TEST_F(SQLiteHelperEnhancedTest, QueryTimeout) {
    // This is a basic test - in practice you'd want a more complex query
    // that actually takes time to execute
    auto start = std::chrono::high_resolution_clock::now();
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 1000, 0);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Query should complete quickly for our small test database
    EXPECT_LT(duration.count(), 5);
}

// Test error handling for non-existent database
TEST_F(SQLiteHelperEnhancedTest, NonExistentDatabase) {
    std::string non_existent_path = "/path/to/non/existent/database.db";
    auto result = SQLiteHelperEnhanced::get_database_schema(non_existent_path);

    EXPECT_TRUE(result.contains("error"));
}

// Test task database listing
TEST_F(SQLiteHelperEnhancedTest, GetTaskDatabases) {
    auto databases = SQLiteHelperEnhanced::get_task_databases("test_task_id");

    EXPECT_TRUE(databases.contains("raw_db"));
    EXPECT_TRUE(databases.contains("events_db"));
    EXPECT_TRUE(databases.contains("files_db"));
    EXPECT_TRUE(databases.contains("android_db"));

    // All should be empty strings for a non-existent task
    EXPECT_EQ(databases["raw_db"], "");
    EXPECT_EQ(databases["events_db"], "");
    EXPECT_EQ(databases["files_db"], "");
    EXPECT_EQ(databases["android_db"], "");
}

// Performance test with larger dataset
TEST_F(SQLiteHelperEnhancedTest, PerformanceTest) {
    // Insert more test data for performance testing
    sqlite3* db;
    ASSERT_EQ(sqlite3_open(test_db_path_.c_str(), &db), SQLITE_OK);

    // Insert 1000 test records
    for (int i = 0; i < 1000; ++i) {
        std::string sql = "INSERT INTO files (id, name, path, size, file_type) VALUES "
                         "(" + std::to_string(100 + i) + ", 'file" + std::to_string(i) + ".txt', "
                         "'/path/to/file" + std::to_string(i) + ".txt', " + std::to_string(1024 + i) + ", 'txt')";
        ASSERT_EQ(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
    }

    sqlite3_close(db);

    // Test export performance
    auto start = std::chrono::high_resolution_clock::now();
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 1000, 0);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_GE(result["total_exported"], 1000);

    // Performance should be reasonable (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}