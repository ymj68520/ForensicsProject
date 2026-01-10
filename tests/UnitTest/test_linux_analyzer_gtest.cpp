// test_linux_analyzer_gtest.cpp
// Comprehensive GTest-based unit tests for LinuxFilesAnalyzer module
// Tests: parsers, query builder, error handling, and data types

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>

// Include the module headers
#include "LinuxFilesAnalyzer/Common/LinuxDataTypes.h"
#include "LinuxFilesAnalyzer/Common/LinuxAnalyzerErrors.h"
#include "LinuxFilesAnalyzer/Database/LinuxQueryBuilder.h"
#include "LinuxFilesAnalyzer/Parsers/LinuxLogParser.h"
#include "LinuxFilesAnalyzer/Parsers/LinuxUserParser.h"
#include "LinuxFilesAnalyzer/Parsers/LinuxHistoryParser.h"

using namespace LinuxAnalysis;
using ::testing::HasSubstr;
using ::testing::Eq;

// ============================================================================
// Data Type Default Initialization Tests
// ============================================================================

class LinuxDataTypesTest : public ::testing::Test {};

TEST_F(LinuxDataTypesTest, LinuxLogEntry_DefaultInit) {
    LinuxLogEntry entry;
    EXPECT_EQ(entry.unixTimestamp, 0);
    EXPECT_EQ(entry.pid, -1);
    EXPECT_TRUE(entry.logFile.empty());
    EXPECT_TRUE(entry.message.empty());
}

TEST_F(LinuxDataTypesTest, LinuxUserInfo_DefaultInit) {
    LinuxUserInfo user;
    EXPECT_EQ(user.uid, -1);
    EXPECT_EQ(user.gid, -1);
    EXPECT_EQ(user.lastPasswordChange, 0);
    EXPECT_EQ(user.passwordMaxAge, 0);
    EXPECT_FALSE(user.isLocked);
    EXPECT_FALSE(user.isSystemAccount);
}

TEST_F(LinuxDataTypesTest, LinuxLoginRecord_DefaultInit) {
    LinuxLoginRecord record;
    EXPECT_EQ(record.loginTime, 0);
    EXPECT_EQ(record.logoutTime, 0);
    EXPECT_TRUE(record.isSuccess);
    EXPECT_EQ(record.pid, 0);
}

TEST_F(LinuxDataTypesTest, NetworkConnection_DefaultInit) {
    NetworkConnection conn;
    EXPECT_EQ(conn.localPort, 0);
    EXPECT_EQ(conn.remotePort, 0);
    EXPECT_EQ(conn.uid, -1);
    EXPECT_EQ(conn.pid, -1);
}

TEST_F(LinuxDataTypesTest, FirewallRule_DefaultInit) {
    FirewallRule rule;
    EXPECT_EQ(rule.sourcePort, -1);
    EXPECT_EQ(rule.destinationPort, -1);
}

TEST_F(LinuxDataTypesTest, ShellHistoryEntry_DefaultInit) {
    ShellHistoryEntry entry;
    EXPECT_TRUE(entry.username.empty());
    EXPECT_TRUE(entry.command.empty());
    EXPECT_EQ(entry.timestamp, 0);
    EXPECT_EQ(entry.lineNumber, 0);
}

TEST_F(LinuxDataTypesTest, CronJobEntry_DefaultInit) {
    CronJobEntry entry;
    EXPECT_TRUE(entry.username.empty());
    EXPECT_TRUE(entry.command.empty());
    EXPECT_TRUE(entry.cronType.empty());
}

TEST_F(LinuxDataTypesTest, SSHKeyInfo_DefaultInit) {
    SSHKeyInfo key;
    EXPECT_TRUE(key.username.empty());
    EXPECT_TRUE(key.keyType.empty());
    EXPECT_TRUE(key.publicKey.empty());
}

TEST_F(LinuxDataTypesTest, PackageInfo_DefaultInit) {
    PackageInfo pkg;
    EXPECT_TRUE(pkg.name.empty());
    EXPECT_TRUE(pkg.version.empty());
    EXPECT_EQ(pkg.installTime, 0);
}

// ============================================================================
// QueryBuilder Tests
// ============================================================================

class QueryBuilderTest : public ::testing::Test {
protected:
    QueryBuilder qb;
};

TEST_F(QueryBuilderTest, EmptyBuilder) {
    EXPECT_TRUE(qb.empty());
    EXPECT_EQ(qb.buildWhereClause(), "");
    EXPECT_EQ(qb.getParameterCount(), 0u);
}

TEST_F(QueryBuilderTest, SimpleEqualsCondition) {
    qb.where(Columns::USERNAME, ConditionType::EQUALS, std::string("admin"));
    
    EXPECT_FALSE(qb.empty());
    EXPECT_EQ(qb.buildWhereClause(), "username = ?");
    EXPECT_EQ(qb.getParameterCount(), 1u);
}

TEST_F(QueryBuilderTest, IntegerEqualsCondition) {
    qb.where(Columns::UID, ConditionType::EQUALS, 1000);
    
    EXPECT_FALSE(qb.empty());
    EXPECT_EQ(qb.buildWhereClause(), "uid = ?");
    EXPECT_EQ(qb.getParameterCount(), 1u);
}

TEST_F(QueryBuilderTest, AndCondition) {
    qb.where(Columns::USERNAME, ConditionType::EQUALS, std::string("admin"))
      .andWhere(Columns::IS_LOCKED, ConditionType::EQUALS, false);
    
    EXPECT_EQ(qb.buildWhereClause(), "username = ? AND is_locked = ?");
    EXPECT_EQ(qb.getParameterCount(), 2u);
}

TEST_F(QueryBuilderTest, OrCondition) {
    qb.where(Columns::LEVEL, ConditionType::EQUALS, std::string("ERROR"))
      .orWhere(Columns::LEVEL, ConditionType::EQUALS, std::string("WARNING"));
    
    EXPECT_EQ(qb.buildWhereClause(), "level = ? OR level = ?");
    EXPECT_EQ(qb.getParameterCount(), 2u);
}

TEST_F(QueryBuilderTest, LikeCondition) {
    qb.where(Columns::MESSAGE, ConditionType::LIKE, std::string("%error%"));
    
    EXPECT_EQ(qb.buildWhereClause(), "message LIKE ?");
}

TEST_F(QueryBuilderTest, NotLikeCondition) {
    qb.where(Columns::MESSAGE, ConditionType::NOT_LIKE, std::string("%debug%"));
    
    EXPECT_EQ(qb.buildWhereClause(), "message NOT LIKE ?");
}

TEST_F(QueryBuilderTest, NullCondition) {
    qb.whereNull(Columns::HOSTNAME, true);
    
    EXPECT_EQ(qb.buildWhereClause(), "hostname IS NULL");
    EXPECT_EQ(qb.getParameterCount(), 0u);  // NULL checks have no parameters
}

TEST_F(QueryBuilderTest, NotNullCondition) {
    qb.whereNull(Columns::HOSTNAME, false);
    
    EXPECT_EQ(qb.buildWhereClause(), "hostname IS NOT NULL");
    EXPECT_EQ(qb.getParameterCount(), 0u);
}

TEST_F(QueryBuilderTest, LessThanCondition) {
    qb.where(Columns::UID, ConditionType::LESS_THAN, 1000);
    
    EXPECT_EQ(qb.buildWhereClause(), "uid < ?");
}

TEST_F(QueryBuilderTest, GreaterThanCondition) {
    qb.where(Columns::UID, ConditionType::GREATER_THAN, 1000);
    
    EXPECT_EQ(qb.buildWhereClause(), "uid > ?");
}

TEST_F(QueryBuilderTest, LessEqualCondition) {
    qb.where(Columns::UID, ConditionType::LESS_EQUAL, 1000);
    
    EXPECT_EQ(qb.buildWhereClause(), "uid <= ?");
}

TEST_F(QueryBuilderTest, GreaterEqualCondition) {
    qb.where(Columns::UID, ConditionType::GREATER_EQUAL, 1000);
    
    EXPECT_EQ(qb.buildWhereClause(), "uid >= ?");
}

TEST_F(QueryBuilderTest, OrderByAsc) {
    qb.orderBy(Columns::TIMESTAMP, SortOrder::ASC);
    
    EXPECT_EQ(qb.buildOrderByClause(), " ORDER BY timestamp ASC");
}

TEST_F(QueryBuilderTest, OrderByDesc) {
    qb.orderBy(Columns::TIMESTAMP, SortOrder::DESC);
    
    EXPECT_EQ(qb.buildOrderByClause(), " ORDER BY timestamp DESC");
}

TEST_F(QueryBuilderTest, Limit) {
    qb.limit(100);
    
    EXPECT_EQ(qb.buildLimitClause(), " LIMIT 100");
}

TEST_F(QueryBuilderTest, LimitOffset) {
    qb.limit(50).offset(100);
    
    EXPECT_EQ(qb.buildLimitClause(), " LIMIT 50 OFFSET 100");
}

TEST_F(QueryBuilderTest, FullClause) {
    qb.where(Columns::UID, ConditionType::GREATER_THAN, 1000)
      .orderBy(Columns::USERNAME, SortOrder::ASC)
      .limit(10);
    
    std::string expected = " WHERE uid > ? ORDER BY username ASC LIMIT 10";
    EXPECT_EQ(qb.buildFullClause(), expected);
}

TEST_F(QueryBuilderTest, ComplexQuery) {
    qb.where(Columns::LEVEL, ConditionType::EQUALS, std::string("ERROR"))
      .andWhere(Columns::UNIX_TIMESTAMP, ConditionType::GREATER_THAN, static_cast<int64_t>(1609459200))
      .orderBy(Columns::TIMESTAMP, SortOrder::DESC)
      .limit(100)
      .offset(0);
    
    std::string whereClause = qb.buildWhereClause();
    EXPECT_THAT(whereClause, HasSubstr("level = ?"));
    EXPECT_THAT(whereClause, HasSubstr("unix_timestamp > ?"));
    EXPECT_EQ(qb.getParameterCount(), 2u);
}

TEST_F(QueryBuilderTest, InvalidColumnThrows) {
    EXPECT_THROW(
        qb.where("invalid; DROP TABLE", ConditionType::EQUALS, std::string("test")),
        std::invalid_argument
    );
}

TEST_F(QueryBuilderTest, SqlInjectionPrevention) {
    // SQL injection attempts should throw
    EXPECT_THROW(
        qb.where("username; DELETE FROM users", ConditionType::EQUALS, std::string("x")),
        std::invalid_argument
    );
    
    EXPECT_THROW(
        qb.where("username' OR '1'='1", ConditionType::EQUALS, std::string("x")),
        std::invalid_argument
    );
}

TEST_F(QueryBuilderTest, Clear) {
    qb.where(Columns::USERNAME, ConditionType::EQUALS, std::string("test"))
      .limit(10);
    
    qb.clear();
    EXPECT_TRUE(qb.empty());
    EXPECT_EQ(qb.buildFullClause(), "");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class LinuxAnalyzerErrorTest : public ::testing::Test {};

TEST_F(LinuxAnalyzerErrorTest, Success) {
    LinuxAnalyzerError err;
    EXPECT_FALSE(err.isError());
    EXPECT_TRUE(err.isSuccess());
    EXPECT_EQ(err.code(), ErrorCode::SUCCESS);
}

TEST_F(LinuxAnalyzerErrorTest, WithCode) {
    LinuxAnalyzerError err(ErrorCode::DATABASE_OPEN_FAILED);
    EXPECT_TRUE(err.isError());
    EXPECT_FALSE(err.isSuccess());
    EXPECT_EQ(err.code(), ErrorCode::DATABASE_OPEN_FAILED);
    EXPECT_FALSE(err.message().empty());
}

TEST_F(LinuxAnalyzerErrorTest, WithDetails) {
    LinuxAnalyzerError err(ErrorCode::FILE_NOT_FOUND, "/path/to/file");
    EXPECT_EQ(err.details(), "/path/to/file");
    EXPECT_THAT(err.toString(), HasSubstr("/path/to/file"));
}

TEST_F(LinuxAnalyzerErrorTest, ToStringFormat) {
    LinuxAnalyzerError err(ErrorCode::PARSE_INVALID_FORMAT, "bad data");
    std::string str = err.toString();
    EXPECT_THAT(str, HasSubstr("300"));  // Error code
    EXPECT_THAT(str, HasSubstr("bad data"));  // Details
}

TEST_F(LinuxAnalyzerErrorTest, AllErrorCodesHaveMessages) {
    std::vector<ErrorCode> codes = {
        ErrorCode::SUCCESS,
        ErrorCode::DATABASE_OPEN_FAILED,
        ErrorCode::FILE_NOT_FOUND,
        ErrorCode::PARSE_INVALID_FORMAT,
        ErrorCode::VALIDATION_INVALID_COLUMN,
        ErrorCode::SYSTEM_MEMORY_ERROR,
        ErrorCode::UNKNOWN_ERROR
    };
    
    for (auto code : codes) {
        std::string msg = getErrorMessage(code);
        EXPECT_FALSE(msg.empty()) << "Error code " << static_cast<int>(code) << " has empty message";
    }
}

// ============================================================================
// Result Type Tests
// ============================================================================

class ResultTest : public ::testing::Test {};

TEST_F(ResultTest, SuccessValue) {
    Result<int> result = makeSuccess(42);
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(result.hasError());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(ResultTest, SuccessString) {
    Result<std::string> result = makeSuccess(std::string("hello"));
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), "hello");
}

TEST_F(ResultTest, ErrorResult) {
    Result<int> result = makeError<int>(ErrorCode::PARSE_INVALID_FORMAT);
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::PARSE_INVALID_FORMAT);
}

TEST_F(ResultTest, ErrorWithDetails) {
    Result<int> result = makeError<int>(ErrorCode::FILE_NOT_FOUND, "/missing/file");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().details(), "/missing/file");
}

TEST_F(ResultTest, ValueOr_HasValue) {
    Result<std::string> result = makeSuccess(std::string("success"));
    EXPECT_EQ(result.valueOr("default"), "success");
}

TEST_F(ResultTest, ValueOr_HasError) {
    Result<std::string> result = makeError<std::string>(ErrorCode::UNKNOWN_ERROR);
    EXPECT_EQ(result.valueOr("default"), "default");
}

TEST_F(ResultTest, ValueThrowsOnError) {
    Result<int> result = makeError<int>(ErrorCode::UNKNOWN_ERROR);
    EXPECT_THROW(result.value(), std::runtime_error);
}

TEST_F(ResultTest, ErrorThrowsOnValue) {
    Result<int> result = makeSuccess(42);
    EXPECT_THROW(result.error(), std::runtime_error);
}

TEST_F(ResultTest, VoidResult_Success) {
    Result<void> result = makeSuccess();
    EXPECT_TRUE(result.hasValue());
    EXPECT_TRUE(result.isSuccess());
    EXPECT_FALSE(result.hasError());
}

TEST_F(ResultTest, VoidResult_Error) {
    Result<void> result = makeError(ErrorCode::SYSTEM_MEMORY_ERROR);
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
}

TEST_F(ResultTest, BoolConversion) {
    Result<int> success = makeSuccess(42);
    Result<int> error = makeError<int>(ErrorCode::UNKNOWN_ERROR);
    
    EXPECT_TRUE(static_cast<bool>(success));
    EXPECT_FALSE(static_cast<bool>(error));
}

// ============================================================================
// LinuxLogParser Tests
// ============================================================================

class LinuxLogParserTest : public ::testing::Test {};

TEST_F(LinuxLogParserTest, ParseSyslogLine_Standard) {
    std::string line = "Jan  5 10:23:45 server sshd[1234]: Accepted publickey for user";
    LinuxLogEntry entry = LinuxLogParser::parseSyslogLine(line, "auth.log");
    
    EXPECT_EQ(entry.logFile, "auth.log");
    EXPECT_EQ(entry.timestamp, "Jan  5 10:23:45");
    EXPECT_EQ(entry.hostname, "server");
    EXPECT_EQ(entry.process, "sshd");
    EXPECT_EQ(entry.pid, 1234);
    EXPECT_THAT(entry.message, HasSubstr("Accepted publickey"));
}

TEST_F(LinuxLogParserTest, ParseSyslogLine_NoPid) {
    std::string line = "Jan  5 10:23:45 server kernel: Linux version 5.4.0";
    LinuxLogEntry entry = LinuxLogParser::parseSyslogLine(line, "syslog");
    
    EXPECT_EQ(entry.process, "kernel");
    EXPECT_EQ(entry.pid, -1);
}

TEST_F(LinuxLogParserTest, ParseSyslogLine_WithDoubleDigitDay) {
    std::string line = "Jan 15 10:23:45 server sshd[1234]: Connection closed";
    LinuxLogEntry entry = LinuxLogParser::parseSyslogLine(line, "auth.log");
    
    EXPECT_EQ(entry.timestamp, "Jan 15 10:23:45");
}

TEST_F(LinuxLogParserTest, InferLogLevel_Error) {
    EXPECT_EQ(LinuxLogParser::inferLogLevel("Connection failed"), "ERROR");
    EXPECT_EQ(LinuxLogParser::inferLogLevel("error: something went wrong"), "ERROR");
    EXPECT_EQ(LinuxLogParser::inferLogLevel("critical: system crash"), "ERROR");
}

TEST_F(LinuxLogParserTest, InferLogLevel_Warning) {
    EXPECT_EQ(LinuxLogParser::inferLogLevel("Warning: disk space low"), "WARNING");
    EXPECT_EQ(LinuxLogParser::inferLogLevel("warn: something is off"), "WARNING");
}

TEST_F(LinuxLogParserTest, InferLogLevel_Info) {
    EXPECT_EQ(LinuxLogParser::inferLogLevel("User logged in"), "INFO");
    EXPECT_EQ(LinuxLogParser::inferLogLevel("Started service"), "INFO");
}

TEST_F(LinuxLogParserTest, InferLogLevel_Debug) {
    EXPECT_EQ(LinuxLogParser::inferLogLevel("Debug message"), "DEBUG");
    EXPECT_EQ(LinuxLogParser::inferLogLevel("debugging enabled"), "DEBUG");
}

TEST_F(LinuxLogParserTest, InferFacility) {
    EXPECT_EQ(LinuxLogParser::inferFacility("auth.log"), "auth");
    EXPECT_EQ(LinuxLogParser::inferFacility("kern.log"), "kern");
    EXPECT_EQ(LinuxLogParser::inferFacility("syslog"), "syslog");
    EXPECT_EQ(LinuxLogParser::inferFacility("daemon.log"), "daemon");
    EXPECT_EQ(LinuxLogParser::inferFacility("messages"), "user");  // Default fallback
}

// ============================================================================
// LinuxUserParser Tests
// ============================================================================

class LinuxUserParserTest : public ::testing::Test {};

TEST_F(LinuxUserParserTest, ParsePasswdLine_Standard) {
    std::string line = "john:x:1001:1001:John Doe:/home/john:/bin/bash";
    LinuxUserInfo user = LinuxUserParser::parsePasswdLine(line);
    
    EXPECT_EQ(user.username, "john");
    EXPECT_EQ(user.uid, 1001);
    EXPECT_EQ(user.gid, 1001);
    EXPECT_EQ(user.fullName, "John Doe");
    EXPECT_EQ(user.homeDirectory, "/home/john");
    EXPECT_EQ(user.shell, "/bin/bash");
    EXPECT_FALSE(user.isSystemAccount);
}

TEST_F(LinuxUserParserTest, ParsePasswdLine_Root) {
    std::string line = "root:x:0:0:root:/root:/bin/bash";
    LinuxUserInfo user = LinuxUserParser::parsePasswdLine(line);
    
    EXPECT_EQ(user.username, "root");
    EXPECT_EQ(user.uid, 0);
    EXPECT_TRUE(user.isSystemAccount);
}

TEST_F(LinuxUserParserTest, ParsePasswdLine_EmptyGecos) {
    std::string line = "daemon:x:1:1::/usr/sbin:/usr/sbin/nologin";
    LinuxUserInfo user = LinuxUserParser::parsePasswdLine(line);
    
    EXPECT_TRUE(user.fullName.empty());
}

TEST_F(LinuxUserParserTest, ParsePasswdFile_Multiple) {
    std::string content = 
        "root:x:0:0:root:/root:/bin/bash\n"
        "john:x:1001:1001:John:/home/john:/bin/bash\n"
        "# Comment line\n"
        "jane:x:1002:1002:Jane:/home/jane:/bin/zsh\n";
    
    std::vector<LinuxUserInfo> users = LinuxUserParser::parsePasswdFile(content);
    EXPECT_EQ(users.size(), 3u);
    EXPECT_EQ(users[0].username, "root");
    EXPECT_TRUE(users[0].isSystemAccount);
    EXPECT_EQ(users[1].username, "john");
    EXPECT_FALSE(users[1].isSystemAccount);
    EXPECT_EQ(users[2].username, "jane");
}

TEST_F(LinuxUserParserTest, ParseGroupLine_Standard) {
    std::string line = "developers:x:1001:john,jane,bob";
    LinuxGroupInfo group = LinuxUserParser::parseGroupLine(line);
    
    EXPECT_EQ(group.groupName, "developers");
    EXPECT_EQ(group.gid, 1001);
    EXPECT_EQ(group.members.size(), 3u);
    EXPECT_EQ(group.members[0], "john");
    EXPECT_EQ(group.members[1], "jane");
    EXPECT_EQ(group.members[2], "bob");
}

TEST_F(LinuxUserParserTest, ParseGroupLine_NoMembers) {
    std::string line = "wheel:x:10:";
    LinuxGroupInfo group = LinuxUserParser::parseGroupLine(line);
    
    EXPECT_EQ(group.groupName, "wheel");
    EXPECT_EQ(group.gid, 10);
    EXPECT_TRUE(group.members.empty());
}

TEST_F(LinuxUserParserTest, IsSystemAccount) {
    EXPECT_TRUE(LinuxUserParser::isSystemAccount(0));      // root
    EXPECT_TRUE(LinuxUserParser::isSystemAccount(1));      // daemon
    EXPECT_TRUE(LinuxUserParser::isSystemAccount(999));    // system
    EXPECT_FALSE(LinuxUserParser::isSystemAccount(1000));  // regular user
    EXPECT_FALSE(LinuxUserParser::isSystemAccount(65534)); // nobody
    EXPECT_FALSE(LinuxUserParser::isSystemAccount(10000)); // regular user
}

TEST_F(LinuxUserParserTest, IsAccountLocked) {
    EXPECT_TRUE(LinuxUserParser::isAccountLocked("!$6$hash"));
    EXPECT_TRUE(LinuxUserParser::isAccountLocked("*"));
    EXPECT_TRUE(LinuxUserParser::isAccountLocked("!!"));
    EXPECT_TRUE(LinuxUserParser::isAccountLocked("!"));
    EXPECT_FALSE(LinuxUserParser::isAccountLocked("$6$validhash"));
    EXPECT_FALSE(LinuxUserParser::isAccountLocked(""));
}

// ============================================================================
// LinuxHistoryParser Tests
// ============================================================================

class LinuxHistoryParserTest : public ::testing::Test {};

TEST_F(LinuxHistoryParserTest, ParseBashHistory_Simple) {
    std::string content = 
        "ls -la\n"
        "cd /home\n"
        "echo test\n";
    
    auto entries = LinuxHistoryParser::parseBashHistory(content, "testuser", ".bash_history");
    
    EXPECT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].command, "ls -la");
    EXPECT_EQ(entries[0].username, "testuser");
    EXPECT_EQ(entries[0].shellType, "bash");
}

TEST_F(LinuxHistoryParserTest, ParseBashHistory_WithTimestamps) {
    std::string content = 
        "ls -la\n"
        "cd /home\n"
        "#1234567890\n"
        "echo test\n";
    
    auto entries = LinuxHistoryParser::parseBashHistory(content, "testuser", ".bash_history");
    
    EXPECT_EQ(entries.size(), 3u);  // 3 commands, timestamp line is not a command
    EXPECT_EQ(entries[2].timestamp, 1234567890);  // Command after timestamp line
}

TEST_F(LinuxHistoryParserTest, ParseZshHistory_Extended) {
    std::string content = 
        ": 1234567890:0;ls -la\n"
        ": 1234567891:0;cd /home\n";
    
    auto entries = LinuxHistoryParser::parseZshHistory(content, "testuser", ".zsh_history");
    
    EXPECT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].command, "ls -la");
    EXPECT_EQ(entries[0].timestamp, 1234567890);
    EXPECT_EQ(entries[0].shellType, "zsh");
}

TEST_F(LinuxHistoryParserTest, ParseZshHistory_SimpleFormat) {
    std::string content = 
        "ls -la\n"
        "cd /home\n";
    
    auto entries = LinuxHistoryParser::parseZshHistory(content, "testuser", ".zsh_history");
    
    EXPECT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].command, "ls -la");
    EXPECT_EQ(entries[0].shellType, "zsh");
}

TEST_F(LinuxHistoryParserTest, DetectShellType) {
    EXPECT_EQ(LinuxHistoryParser::detectShellType(".bash_history"), "bash");
    EXPECT_EQ(LinuxHistoryParser::detectShellType(".zsh_history"), "zsh");
    EXPECT_EQ(LinuxHistoryParser::detectShellType("/home/user/.local/share/fish/fish_history"), "fish");
    EXPECT_EQ(LinuxHistoryParser::detectShellType("unknown_file"), "bash");  // Default
}

TEST_F(LinuxHistoryParserTest, ParseBashHistory_EmptyLines) {
    std::string content = 
        "ls -la\n"
        "\n"
        "cd /home\n"
        "\n\n"
        "echo test\n";
    
    auto entries = LinuxHistoryParser::parseBashHistory(content, "testuser", ".bash_history");
    
    // Empty lines should be skipped
    EXPECT_EQ(entries.size(), 3u);
}

TEST_F(LinuxHistoryParserTest, LineNumbers) {
    std::string content = 
        "ls -la\n"
        "cd /home\n"
        "echo test\n";
    
    auto entries = LinuxHistoryParser::parseBashHistory(content, "testuser", ".bash_history");
    
    EXPECT_EQ(entries[0].lineNumber, 1);
    EXPECT_EQ(entries[1].lineNumber, 2);
    EXPECT_EQ(entries[2].lineNumber, 3);
}

// ============================================================================
// Integration Tests
// ============================================================================

class LinuxAnalyzerIntegrationTest : public ::testing::Test {};

TEST_F(LinuxAnalyzerIntegrationTest, QueryBuilderWithMultipleConditions) {
    QueryBuilder qb;
    qb.where(Columns::USERNAME, ConditionType::EQUALS, std::string("admin"))
      .andWhere(Columns::IS_SYSTEM_ACCOUNT, ConditionType::EQUALS, false)
      .andWhere(Columns::UID, ConditionType::GREATER_THAN, 1000)
      .orderBy(Columns::USERNAME, SortOrder::ASC)
      .limit(50);
    
    std::string fullClause = qb.buildFullClause();
    EXPECT_THAT(fullClause, HasSubstr("WHERE"));
    EXPECT_THAT(fullClause, HasSubstr("username = ?"));
    EXPECT_THAT(fullClause, HasSubstr("is_system_account = ?"));
    EXPECT_THAT(fullClause, HasSubstr("uid > ?"));
    EXPECT_THAT(fullClause, HasSubstr("ORDER BY"));
    EXPECT_THAT(fullClause, HasSubstr("LIMIT 50"));
}

TEST_F(LinuxAnalyzerIntegrationTest, ErrorPropagation) {
    // Create an error and propagate it through Result
    LinuxAnalyzerError originalError(ErrorCode::DATABASE_QUERY_FAILED, "query timeout");
    Result<std::vector<LinuxLogEntry>> result = makeError<std::vector<LinuxLogEntry>>(originalError);
    
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::DATABASE_QUERY_FAILED);
    EXPECT_EQ(result.error().details(), "query timeout");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
