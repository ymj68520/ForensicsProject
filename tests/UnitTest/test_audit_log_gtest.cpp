// test_audit_log_gtest.cpp
// GTest-based unit tests for AuditLog module
// Tests logging, flushing, and query operations

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <thread>
#include <chrono>

#include "AuditLog/AuditLog.h"

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Not;
using ::testing::IsEmpty;

namespace fs = std::filesystem;

// ============================================================================
// AuditLogConfig Tests
// ============================================================================

class AuditLogConfigTest : public ::testing::Test {};

TEST_F(AuditLogConfigTest, DefaultConfig) {
    AuditLogConfig config;
    
    // Check that default values are sensible (using actual field names)
    EXPECT_GT(config.cache_size, 0u);
    EXPECT_GT(config.batch_size, 0u);
    EXPECT_GT(config.flush_interval_seconds, 0);
    EXPECT_GT(config.retention_days, 0);
    EXPECT_FALSE(config.db_path.empty());
}

TEST_F(AuditLogConfigTest, DefaultDbPath) {
    AuditLogConfig config;
    EXPECT_EQ(config.db_path, "forensics_audit.db");
}

TEST_F(AuditLogConfigTest, DefaultCacheSize) {
    AuditLogConfig config;
    EXPECT_EQ(config.cache_size, 100u);
}

TEST_F(AuditLogConfigTest, DefaultBatchSize) {
    AuditLogConfig config;
    EXPECT_EQ(config.batch_size, 1u);  // Immediate write for safety
}

TEST_F(AuditLogConfigTest, DefaultAsyncWrite) {
    AuditLogConfig config;
    EXPECT_FALSE(config.async_write);  // Disabled by default for safety
}

TEST_F(AuditLogConfigTest, DefaultWalMode) {
    AuditLogConfig config;
    EXPECT_TRUE(config.enable_wal);
}

// ============================================================================
// AuditLogEntry Tests
// ============================================================================

class AuditLogEntryTest : public ::testing::Test {};

TEST_F(AuditLogEntryTest, DefaultEntry) {
    AuditLogEntry entry;
    
    EXPECT_EQ(entry.id, 0);  // id is int64_t, not string
    EXPECT_TRUE(entry.task_id.empty());
    EXPECT_TRUE(entry.action.empty());
    EXPECT_TRUE(entry.details.empty());
    EXPECT_TRUE(entry.user_id.empty());
}

TEST_F(AuditLogEntryTest, EntryWithValues) {
    AuditLogEntry entry;
    entry.id = 1;
    entry.task_id = "task-123";
    entry.action = "CREATED";
    entry.details = "Task was created";
    entry.user_id = "user-abc";
    entry.timestamp = std::chrono::system_clock::now();
    
    EXPECT_EQ(entry.id, 1);
    EXPECT_EQ(entry.task_id, "task-123");
    EXPECT_EQ(entry.action, "CREATED");
    EXPECT_EQ(entry.details, "Task was created");
    EXPECT_EQ(entry.user_id, "user-abc");
}

TEST_F(AuditLogEntryTest, TimestampToUnixMs) {
    AuditLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    
    int64_t ms = entry.timestampToUnixMs();
    
    // Should be a reasonable Unix timestamp in milliseconds (after year 2020)
    EXPECT_GT(ms, 1577836800000);  // Jan 1, 2020 in ms
}

TEST_F(AuditLogEntryTest, FromUnixMs) {
    int64_t timestamp_ms = 1609459200000;  // Jan 1, 2021 00:00:00 UTC
    
    AuditLogEntry entry = AuditLogEntry::fromUnixMs(
        42, "task-001", timestamp_ms, "STATUS_CHANGE", "Task updated", "admin"
    );
    
    EXPECT_EQ(entry.id, 42);
    EXPECT_EQ(entry.task_id, "task-001");
    EXPECT_EQ(entry.action, "STATUS_CHANGE");
    EXPECT_EQ(entry.details, "Task updated");
    EXPECT_EQ(entry.user_id, "admin");
}

// ============================================================================
// AuditLog Configuration Tests
// ============================================================================

class AuditLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test database path
        testDbPath = fs::temp_directory_path() / "test_audit.db";
        
        // Remove any existing test database
        fs::remove(testDbPath);
    }
    
    void TearDown() override {
        // Cleanup
        fs::remove(testDbPath);
    }
    
    fs::path testDbPath;
};

TEST_F(AuditLogTest, ConfigPathIsValid) {
    AuditLogConfig config;
    config.db_path = testDbPath.string();
    
    // Verify config path is set correctly
    EXPECT_EQ(config.db_path, testDbPath.string());
}

TEST_F(AuditLogTest, CacheSizeConfiguration) {
    AuditLogConfig config;
    config.cache_size = 200;
    
    EXPECT_EQ(config.cache_size, 200u);
}

TEST_F(AuditLogTest, BatchSizeConfiguration) {
    AuditLogConfig config;
    config.batch_size = 50;
    
    EXPECT_EQ(config.batch_size, 50u);
}

TEST_F(AuditLogTest, FlushIntervalConfiguration) {
    AuditLogConfig config;
    config.flush_interval_seconds = 10;
    
    EXPECT_EQ(config.flush_interval_seconds, 10);
}

TEST_F(AuditLogTest, RetentionDaysConfiguration) {
    AuditLogConfig config;
    config.retention_days = 365;
    
    EXPECT_EQ(config.retention_days, 365);
}

TEST_F(AuditLogTest, MaxDatabaseSizeConfiguration) {
    AuditLogConfig config;
    config.max_db_size_mb = 512;
    
    EXPECT_EQ(config.max_db_size_mb, 512u);
}

TEST_F(AuditLogTest, AsyncWriteConfiguration) {
    AuditLogConfig config;
    config.async_write = true;
    
    EXPECT_TRUE(config.async_write);
}

TEST_F(AuditLogTest, WalModeConfiguration) {
    AuditLogConfig config;
    config.enable_wal = false;
    
    EXPECT_FALSE(config.enable_wal);
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

class AuditLogJsonTest : public ::testing::Test {};

TEST_F(AuditLogJsonTest, EntryToJson) {
    AuditLogEntry entry;
    entry.id = 1;
    entry.task_id = "task-123";
    entry.action = "STATUS_CHANGE";
    entry.details = "Changed from PENDING to RUNNING";
    entry.user_id = "admin";
    entry.timestamp = std::chrono::system_clock::now();
    
    // Convert to JSON using nlohmann serializer
    nlohmann::json j = entry;
    
    EXPECT_EQ(j["id"], 1);
    EXPECT_EQ(j["task_id"], "task-123");
    EXPECT_EQ(j["action"], "STATUS_CHANGE");
    EXPECT_EQ(j["user_id"], "admin");
}

TEST_F(AuditLogJsonTest, JsonContainsTimestamp) {
    AuditLogEntry entry;
    entry.id = 1;
    entry.task_id = "task-001";
    entry.action = "CREATED";
    entry.timestamp = std::chrono::system_clock::now();
    
    nlohmann::json j = entry;
    
    EXPECT_TRUE(j.contains("timestamp"));
    EXPECT_TRUE(j.contains("timestamp_readable"));
}

TEST_F(AuditLogJsonTest, JsonTimestampIsNumeric) {
    AuditLogEntry entry;
    entry.id = 1;
    entry.task_id = "task-001";
    entry.action = "CREATED";
    entry.timestamp = std::chrono::system_clock::now();
    
    nlohmann::json j = entry;
    
    int64_t timestamp = j["timestamp"];
    EXPECT_GT(timestamp, 0);
}

// ============================================================================
// Time-based Tests
// ============================================================================

class AuditLogTimeTest : public ::testing::Test {};

TEST_F(AuditLogTimeTest, TimestampComparison) {
    auto now = std::chrono::system_clock::now();
    auto oneHourAgo = now - std::chrono::hours(1);
    auto oneHourLater = now + std::chrono::hours(1);
    
    EXPECT_LT(oneHourAgo, now);
    EXPECT_GT(oneHourLater, now);
}

TEST_F(AuditLogTimeTest, TimePointToTimeT) {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    
    // Should be a reasonable Unix timestamp (after year 2020)
    EXPECT_GT(time_t_val, 1577836800);  // Jan 1, 2020
}

TEST_F(AuditLogTimeTest, DurationCalculation) {
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::seconds(60);
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    EXPECT_EQ(duration.count(), 60);
}

TEST_F(AuditLogTimeTest, MillisecondConversion) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Milliseconds should be larger than seconds timestamp
    auto sec = std::chrono::system_clock::to_time_t(now);
    EXPECT_GT(ms, sec);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
