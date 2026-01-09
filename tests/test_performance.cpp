#include <gtest/gtest.h>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <fstream>
#include "../HTTPServer/HTTPserver.h"

using namespace forensics;
using json = nlohmann::json;
using namespace std::chrono;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create large test database
        test_db_path_ = std::filesystem::temp_directory_path() / "performance_test.db";
        create_large_test_database();
    }

    void TearDown() override {
        // Clean up test database
        std::filesystem::remove(test_db_path_);
    }

    void create_large_test_database() {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(test_db_path_.c_str(), &db), SQLITE_OK);

        // Enable WAL mode for better performance
        sqlite3_exec(db, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA cache_size=10000", nullptr, nullptr, nullptr);

        // Create tables with indexes
        const char* create_tables_sql = R"(
            CREATE TABLE files (
                id INTEGER PRIMARY KEY,
                parent_id INTEGER,
                name TEXT NOT NULL,
                path TEXT NOT NULL,
                size INTEGER,
                created_time INTEGER,
                modified_time INTEGER,
                accessed_time INTEGER,
                file_type TEXT,
                is_deleted BOOLEAN DEFAULT 0,
                inode INTEGER,
                md5_hash TEXT,
                sha1_hash TEXT,
                permissions TEXT,
                uid INTEGER,
                gid INTEGER
            );

            CREATE TABLE events (
                id INTEGER PRIMARY KEY,
                file_id INTEGER,
                event_type TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                file_path TEXT,
                file_name TEXT,
                file_size INTEGER,
                user_id INTEGER,
                process_id INTEGER
            );

            CREATE TABLE images (
                id INTEGER PRIMARY KEY,
                original_file_id INTEGER,
                file_path TEXT NOT NULL,
                file_size INTEGER,
                file_extension TEXT,
                mime_type TEXT,
                width INTEGER,
                height INTEGER,
                created_time INTEGER,
                modified_time INTEGER,
                metadata TEXT
            );

            CREATE TABLE documents (
                id INTEGER PRIMARY KEY,
                original_file_id INTEGER,
                file_path TEXT NOT NULL,
                file_size INTEGER,
                file_extension TEXT,
                mime_type TEXT,
                title TEXT,
                author TEXT,
                created_time INTEGER,
                modified_time INTEGER,
                word_count INTEGER
            );

            CREATE INDEX idx_files_path ON files(path);
            CREATE INDEX idx_files_type ON files(file_type);
            CREATE INDEX idx_files_created ON files(created_time);
            CREATE INDEX idx_files_modified ON files(modified_time);
            CREATE INDEX idx_files_md5 ON files(md5_hash);
            CREATE INDEX idx_events_timestamp ON events(timestamp);
            CREATE INDEX idx_events_type ON events(event_type);
            CREATE INDEX idx_events_file_id ON events(file_id);
            CREATE INDEX idx_images_extension ON images(file_extension);
            CREATE INDEX idx_images_mime ON images(mime_type);
            CREATE INDEX idx_documents_extension ON documents(file_extension);

            CREATE VIEW file_summary AS
            SELECT
                'images' as category,
                COUNT(*) as file_count,
                SUM(file_size) as total_size
            FROM images
            UNION ALL
            SELECT
                'documents' as category,
                COUNT(*) as file_count,
                SUM(file_size) as total_size
            FROM documents
            UNION ALL
            SELECT
                'events' as category,
                COUNT(*) as file_count,
                0 as total_size
            FROM events
        )";

        ASSERT_EQ(sqlite3_exec(db, create_tables_sql, nullptr, nullptr, nullptr), SQLITE_OK);

        // Prepare statements for batch insertion
        sqlite3_stmt* files_stmt;
        sqlite3_stmt* events_stmt;
        sqlite3_stmt* images_stmt;
        sqlite3_stmt* documents_stmt;

        const char* files_sql = R"(
            INSERT INTO files (id, parent_id, name, path, size, created_time, modified_time,
                              accessed_time, file_type, is_deleted, inode, md5_hash, sha1_hash,
                              permissions, uid, gid)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        const char* events_sql = R"(
            INSERT INTO events (file_id, event_type, timestamp, file_path, file_name,
                               file_size, user_id, process_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";

        const char* images_sql = R"(
            INSERT INTO images (original_file_id, file_path, file_size, file_extension,
                               mime_type, width, height, created_time, modified_time, metadata)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        const char* documents_sql = R"(
            INSERT INTO documents (original_file_id, file_path, file_size, file_extension,
                                  mime_type, title, author, created_time, modified_time, word_count)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        ASSERT_EQ(sqlite3_prepare_v2(db, files_sql, -1, &files_stmt, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_prepare_v2(db, events_sql, -1, &events_stmt, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_prepare_v2(db, images_sql, -1, &images_stmt, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_prepare_v2(db, documents_sql, -1, &documents_stmt, nullptr), SQLITE_OK);

        // Generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> size_dist(100, 10000000); // 100B to 10MB
        std::uniform_int_distribution<> time_dist(1600000000, 1700000000); // Timestamp range
        std::uniform_int_distribution<> uid_dist(1000, 2000);
        std::uniform_int_distribution<> gid_dist(1000, 2000);
        std::uniform_int_distribution<> width_dist(100, 4000);
        std::uniform_int_distribution<> height_dist(100, 3000);
        std::uniform_int_distribution<> word_count_dist(100, 50000);

        std::vector<std::string> file_extensions = {
            "jpg", "jpeg", "png", "gif", "bmp", "tiff", "webp",
            "pdf", "doc", "docx", "txt", "rtf", "odt", "xls", "xlsx", "ppt", "pptx",
            "mp4", "avi", "mov", "wmv", "flv", "mkv", "mp3", "wav", "flac",
            "zip", "rar", "7z", "tar", "gz", "exe", "dll", "so", "app"
        };

        std::vector<std::string> event_types = {"create", "modify", "access", "delete", "move"};

        // Begin transaction
        sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

        // Insert large number of records
        const int num_files = 10000;
        const int num_events = 50000;
        const int num_images = 3000;
        const int num_documents = 2000;

        std::cout << "Creating performance test database with " << num_files << " files, "
                  << num_events << " events, " << num_images << " images, and "
                  << num_documents << " documents..." << std::endl;

        auto start_time = high_resolution_clock::now();

        // Insert files
        for (int i = 1; i <= num_files; ++i) {
            sqlite3_bind_int(files_stmt, 1, i);
            sqlite3_bind_int(files_stmt, 2, i > 1 ? (i / 100) + 1 : 0); // parent_id
            sqlite3_bind_text(files_stmt, 3, ("file_" + std::to_string(i) + "." + file_extensions[i % file_extensions.size()]).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(files_stmt, 4, ("/path/to/directory/file_" + std::to_string(i) + "." + file_extensions[i % file_extensions.size()]).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(files_stmt, 5, size_dist(gen));
            sqlite3_bind_int64(files_stmt, 6, time_dist(gen));
            sqlite3_bind_int64(files_stmt, 7, time_dist(gen));
            sqlite3_bind_int64(files_stmt, 8, time_dist(gen));
            sqlite3_bind_text(files_stmt, 9, file_extensions[i % file_extensions.size()].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(files_stmt, 10, i % 20 == 0 ? 1 : 0); // 5% deleted files
            sqlite3_bind_int64(files_stmt, 11, i + 100000);
            sqlite3_bind_text(files_stmt, 12, ("md5_" + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(files_stmt, 13, ("sha1_" + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(files_stmt, 14, "644", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(files_stmt, 15, uid_dist(gen));
            sqlite3_bind_int(files_stmt, 16, gid_dist(gen));

            sqlite3_step(files_stmt);
            sqlite3_reset(files_stmt);
        }

        // Insert events
        for (int i = 1; i <= num_events; ++i) {
            sqlite3_bind_int(events_stmt, 1, (i % num_files) + 1); // file_id
            sqlite3_bind_text(events_stmt, 2, event_types[i % event_types.size()].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(events_stmt, 3, time_dist(gen));
            sqlite3_bind_text(events_stmt, 4, ("/path/to/directory/file_" + std::to_string((i % num_files) + 1)).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(events_stmt, 5, ("file_" + std::to_string((i % num_files) + 1)).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(events_stmt, 6, size_dist(gen));
            sqlite3_bind_int(events_stmt, 7, uid_dist(gen));
            sqlite3_bind_int(events_stmt, 8, (i % 5000) + 1); // process_id

            sqlite3_step(events_stmt);
            sqlite3_reset(events_stmt);
        }

        // Insert images (subset of files)
        for (int i = 1; i <= num_images; ++i) {
            int file_id = (i * 3) + 1; // Every 3rd file
            if (file_id <= num_files) {
                sqlite3_bind_int(images_stmt, 1, file_id);
                sqlite3_bind_text(images_stmt, 2, ("/path/to/directory/file_" + std::to_string(file_id) + ".jpg").c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(images_stmt, 3, size_dist(gen));
                sqlite3_bind_text(images_stmt, 4, "jpg", -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(images_stmt, 5, "image/jpeg", -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(images_stmt, 6, width_dist(gen));
                sqlite3_bind_int(images_stmt, 7, height_dist(gen));
                sqlite3_bind_int64(images_stmt, 8, time_dist(gen));
                sqlite3_bind_int64(images_stmt, 9, time_dist(gen));
                sqlite3_bind_text(images_stmt, 10, ("metadata_" + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(images_stmt);
                sqlite3_reset(images_stmt);
            }
        }

        // Insert documents (subset of files)
        for (int i = 1; i <= num_documents; ++i) {
            int file_id = (i * 5) + 1; // Every 5th file
            if (file_id <= num_files) {
                sqlite3_bind_int(documents_stmt, 1, file_id);
                sqlite3_bind_text(documents_stmt, 2, ("/path/to/directory/file_" + std::to_string(file_id) + ".pdf").c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(documents_stmt, 3, size_dist(gen));
                sqlite3_bind_text(documents_stmt, 4, "pdf", -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(documents_stmt, 5, "application/pdf", -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(documents_stmt, 6, ("Document " + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(documents_stmt, 7, ("Author " + std::to_string(i)).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(documents_stmt, 8, time_dist(gen));
                sqlite3_bind_int64(documents_stmt, 9, time_dist(gen));
                sqlite3_bind_int(documents_stmt, 10, word_count_dist(gen));

                sqlite3_step(documents_stmt);
                sqlite3_reset(documents_stmt);
            }
        }

        // Commit transaction
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);

        // Cleanup
        sqlite3_finalize(files_stmt);
        sqlite3_finalize(events_stmt);
        sqlite3_finalize(images_stmt);
        sqlite3_finalize(documents_stmt);

        sqlite3_close(db);

        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end_time - start_time);
        std::cout << "Test database created in " << duration.count() << " ms" << std::endl;

        // Get database size
        auto db_size = std::filesystem::file_size(test_db_path_);
        std::cout << "Database size: " << db_size / (1024.0 * 1024.0) << " MB" << std::endl;
    }

    std::filesystem::path test_db_path_;
};

// Test schema extraction performance
TEST_F(PerformanceTest, SchemaExtractionPerformance) {
    const int num_iterations = 10;
    std::vector<double> times;

    for (int i = 0; i < num_iterations; ++i) {
        auto start = high_resolution_clock::now();
        auto schema = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(end - start).count();
        times.push_back(duration / 1000.0); // Convert to milliseconds

        EXPECT_TRUE(schema.contains("tables"));
        EXPECT_TRUE(schema.contains("views"));
    }

    double avg_time = 0;
    for (double time : times) {
        avg_time += time;
    }
    avg_time /= num_iterations;

    std::cout << "Average schema extraction time: " << avg_time << " ms" << std::endl;
    EXPECT_LT(avg_time, 100.0); // Should be under 100ms on average
}

// Test database info extraction performance
TEST_F(PerformanceTest, DatabaseInfoPerformance) {
    const int num_iterations = 20;
    std::vector<double> times;

    for (int i = 0; i < num_iterations; ++i) {
        auto start = high_resolution_clock::now();
        auto info = SQLiteHelperEnhanced::get_database_info(test_db_path_.string());
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(end - start).count();
        times.push_back(duration / 1000.0); // Convert to milliseconds

        EXPECT_TRUE(info.contains("table_count"));
        EXPECT_TRUE(info.contains("page_count"));
    }

    double avg_time = 0;
    for (double time : times) {
        avg_time += time;
    }
    avg_time /= num_iterations;

    std::cout << "Average database info extraction time: " << avg_time << " ms" << std::endl;
    EXPECT_LT(avg_time, 50.0); // Should be under 50ms on average
}

// Test export performance with different sizes
TEST_F(PerformanceTest, ExportPerformance) {
    struct ExportTestCase {
        std::string table;
        int limit;
        std::string format;
        double max_expected_time_ms;
    };

    std::vector<ExportTestCase> test_cases = {
        {"files", 100, "json", 50.0},
        {"files", 1000, "json", 200.0},
        {"files", 5000, "json", 500.0},
        {"events", 1000, "json", 100.0},
        {"events", 5000, "json", 300.0},
        {"images", 1000, "json", 150.0},
        {"documents", 1000, "json", 100.0},
        {"files", 1000, "csv", 250.0},
        {"files", 1000, "xml", 400.0}
    };

    for (const auto& test_case : test_cases) {
        auto start = high_resolution_clock::now();
        auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(),
                                                       test_case.table,
                                                       test_case.format,
                                                       test_case.limit);
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<milliseconds>(end - start).count();

        EXPECT_FALSE(result.contains("error"));
        EXPECT_GE(result["total_exported"], 0);

        std::cout << "Export " << test_case.table << " (" << test_case.format
                  << ", limit=" << test_case.limit << "): " << duration
                  << " ms (max expected: " << test_case.max_expected_time_ms << " ms)" << std::endl;

        // Performance assertion (may need adjustment based on system)
        EXPECT_LT(duration, test_case.max_expected_time_ms);
    }
}

// Test caching performance
TEST_F(PerformanceTest, CachingPerformance) {
    // Clear cache first
    SQLiteHelperEnhanced::clear_cache();

    // First call (no cache)
    auto start1 = high_resolution_clock::now();
    auto result1 = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
    auto end1 = high_resolution_clock::now();
    auto time1 = duration_cast<microseconds>(end1 - start1).count() / 1000.0;

    // Second call (should use cache)
    auto start2 = high_resolution_clock::now();
    auto result2 = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
    auto end2 = high_resolution_clock::now();
    auto time2 = duration_cast<microseconds>(end2 - start2).count() / 1000.0;

    std::cout << "First call (no cache): " << time1 << " ms" << std::endl;
    std::cout << "Second call (cached): " << time2 << " ms" << std::endl;
    std::cout << "Cache speedup: " << (time1 / time2) << "x" << std::endl;

    // Results should be identical
    EXPECT_EQ(result1.dump(), result2.dump());

    // Cached call should be faster (though may not always be true due to system timing)
    // In many cases, cache should provide at least 2x speedup
    if (time1 > 10.0) { // Only check if first call took significant time
        EXPECT_LT(time2, time1 * 0.8); // Cache should be at least 20% faster
    }
}

// Test concurrent query performance
TEST_F(PerformanceTest, ConcurrentQueryPerformance) {
    const int num_threads = 8;
    const int queries_per_thread = 10;

    std::vector<std::thread> threads;
    std::vector<std::vector<double>> thread_times(num_threads);

    auto start_total = high_resolution_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, queries_per_thread, &thread_times]() {
            std::vector<double>& times = thread_times[t];
            times.reserve(queries_per_thread);

            for (int q = 0; q < queries_per_thread; ++q) {
                auto start = high_resolution_clock::now();
                auto info = SQLiteHelperEnhanced::get_database_info(test_db_path_.string());
                auto end = high_resolution_clock::now();

                auto duration = duration_cast<microseconds>(end - start).count() / 1000.0;
                times.push_back(duration);

                EXPECT_TRUE(info.contains("table_count"));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end_total = high_resolution_clock::now();
    auto total_time = duration_cast<milliseconds>(end_total - start_total).count();

    // Calculate statistics
    double total_queries = num_threads * queries_per_thread;
    double avg_time_per_query = 0;
    double max_time = 0;
    double min_time = std::numeric_limits<double>::max();

    for (const auto& times : thread_times) {
        for (double time : times) {
            avg_time_per_query += time;
            max_time = std::max(max_time, time);
            min_time = std::min(min_time, time);
        }
    }
    avg_time_per_query /= total_queries;

    std::cout << "Concurrent query performance:" << std::endl;
    std::cout << "  Total queries: " << total_queries << std::endl;
    std::cout << "  Total time: " << total_time << " ms" << std::endl;
    std::cout << "  Average time per query: " << avg_time_per_query << " ms" << std::endl;
    std::cout << "  Min time: " << min_time << " ms" << std::endl;
    std::cout << "  Max time: " << max_time << " ms" << std::endl;
    std::cout << "  Queries per second: " << (total_queries * 1000.0 / total_time) << std::endl;

    // Performance assertions
    EXPECT_LT(avg_time_per_query, 100.0); // Average should be under 100ms
    EXPECT_LT(total_time, 5000.0); // Total should be under 5 seconds
    EXPECT_GT(total_queries * 1000.0 / total_time, 10.0); // At least 10 queries per second
}

// Test memory usage during large operations
TEST_F(PerformanceTest, MemoryUsageTest) {
    // This is a basic test - in practice you'd want more sophisticated memory monitoring
    const int large_export_limit = 10000;

    auto start = high_resolution_clock::now();
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(),
                                                   "events",
                                                   "json",
                                                   large_export_limit);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start).count();

    EXPECT_FALSE(result.contains("error"));
    EXPECT_GE(result["total_exported"], 0);

    std::cout << "Large export (" << large_export_limit << " records): "
              << duration << " ms" << std::endl;

    // Should handle large exports within reasonable time
    EXPECT_LT(duration, 2000.0); // Under 2 seconds
}

// Test query complexity limits
TEST_F(PerformanceTest, QueryComplexityTest) {
    // Test with complex queries that might timeout
    const int timeout_seconds = 5;

    auto start = high_resolution_clock::now();

    // Try to export a very large number of records
    auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(),
                                                   "events",
                                                   "json",
                                                   100000,  // Very large limit
                                                   0);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<seconds>(end - start).count();

    std::cout << "Complex query duration: " << duration << " seconds" << std::endl;

    // Should not hang indefinitely (timeout protection should work)
    EXPECT_LT(duration, timeout_seconds + 2); // Allow some buffer
}

// Performance regression test
TEST_F(PerformanceTest, PerformanceRegressionTest) {
    // Baseline performance values (these should be updated based on actual system performance)
    const double max_avg_schema_time = 100.0; // ms
    const double max_avg_info_time = 50.0;    // ms
    const double max_avg_export_1000 = 200.0; // ms

    // Test schema extraction
    double schema_time_sum = 0;
    const int schema_iterations = 10;
    for (int i = 0; i < schema_iterations; ++i) {
        auto start = high_resolution_clock::now();
        auto schema = SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
        auto end = high_resolution_clock::now();
        schema_time_sum += duration_cast<microseconds>(end - start).count() / 1000.0;
    }
    double avg_schema_time = schema_time_sum / schema_iterations;

    // Test database info extraction
    double info_time_sum = 0;
    const int info_iterations = 20;
    for (int i = 0; i < info_iterations; ++i) {
        auto start = high_resolution_clock::now();
        auto info = SQLiteHelperEnhanced::get_database_info(test_db_path_.string());
        auto end = high_resolution_clock::now();
        info_time_sum += duration_cast<microseconds>(end - start).count() / 1000.0;
    }
    double avg_info_time = info_time_sum / info_iterations;

    // Test export performance
    double export_time_sum = 0;
    const int export_iterations = 5;
    for (int i = 0; i < export_iterations; ++i) {
        auto start = high_resolution_clock::now();
        auto result = SQLiteHelperEnhanced::export_data(test_db_path_.string(),
                                                       "files",
                                                       "json",
                                                       1000);
        auto end = high_resolution_clock::now();
        export_time_sum += duration_cast<microseconds>(end - start).count() / 1000.0;
    }
    double avg_export_time = export_time_sum / export_iterations;

    std::cout << "Performance regression test results:" << std::endl;
    std::cout << "  Avg schema time: " << avg_schema_time << " ms (max: " << max_avg_schema_time << " ms)" << std::endl;
    std::cout << "  Avg info time: " << avg_info_time << " ms (max: " << max_avg_info_time << " ms)" << std::endl;
    std::cout << "  Avg export time (1000 records): " << avg_export_time << " ms (max: " << max_avg_export_1000 << " ms)" << std::endl;

    // Regression assertions
    EXPECT_LT(avg_schema_time, max_avg_schema_time) << "Schema extraction performance regression detected";
    EXPECT_LT(avg_info_time, max_avg_info_time) << "Database info extraction performance regression detected";
    EXPECT_LT(avg_export_time, max_avg_export_1000) << "Export performance regression detected";
}

// Stress test
TEST_F(PerformanceTest, StressTest) {
    const int stress_iterations = 100;
    const int operations_per_iteration = 10;
    std::vector<double> operation_times;

    for (int iter = 0; iter < stress_iterations; ++iter) {
        auto iter_start = high_resolution_clock::now();

        for (int op = 0; op < operations_per_iteration; ++op) {
            auto op_start = high_resolution_clock::now();

            // Mix different operations
            int op_type = (iter * operations_per_iteration + op) % 4;
            switch (op_type) {
                case 0:
                    SQLiteHelperEnhanced::get_database_info(test_db_path_.string());
                    break;
                case 1:
                    SQLiteHelperEnhanced::get_database_schema(test_db_path_.string());
                    break;
                case 2:
                    SQLiteHelperEnhanced::export_data(test_db_path_.string(), "files", "json", 100);
                    break;
                case 3:
                    SQLiteHelperEnhanced::export_data(test_db_path_.string(), "events", "json", 200);
                    break;
            }

            auto op_end = high_resolution_clock::now();
            operation_times.push_back(duration_cast<microseconds>(op_end - op_start).count() / 1000.0);
        }

        auto iter_end = high_resolution_clock::now();
        auto iter_duration = duration_cast<milliseconds>(iter_end - iter_start).count();

        // Allow some rest time between iterations
        std::this_thread::sleep_for(milliseconds(10));
    }

    // Calculate statistics
    double avg_time = 0, max_time = 0, min_time = std::numeric_limits<double>::max();
    for (double time : operation_times) {
        avg_time += time;
        max_time = std::max(max_time, time);
        min_time = std::min(min_time, time);
    }
    avg_time /= operation_times.size();

    std::cout << "Stress test results:" << std::endl;
    std::cout << "  Total operations: " << operation_times.size() << std::endl;
    std::cout << "  Average operation time: " << avg_time << " ms" << std::endl;
    std::cout << "  Min operation time: " << min_time << " ms" << std::endl;
    std::cout << "  Max operation time: " << max_time << " ms" << std::endl;

    // Stress test assertions
    EXPECT_LT(avg_time, 500.0); // Average should be under 500ms even under stress
    EXPECT_LT(max_time, 2000.0); // No operation should take more than 2 seconds
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}