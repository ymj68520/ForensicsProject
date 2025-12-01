#include "EventExtractor.h"
#include <iostream>
#include <vector>
#include <algorithm>

EventExtractor::EventExtractor(const std::string& sourceDbPath,
	const std::string& eventDbPath)
	: sourceDbPath_(sourceDbPath), eventDbPath_(eventDbPath),
	sourceDb_(nullptr), eventDb_(nullptr) {
}

EventExtractor::~EventExtractor() {
	closeDatabases();
}

bool EventExtractor::extractEvents() {
	if (!openDatabases()) {
		return false;
	}

	if (!createEventTables()) {
		return false;
	}

	if (!extractFileSystemEvents()) {
		return false;
	}

	std::cout << "Events extracted successfully" << std::endl;

	return true;
}

bool EventExtractor::openDatabases() {
	int rc = sqlite3_open(sourceDbPath_.c_str(), &sourceDb_);
	if (rc != SQLITE_OK) {
		std::cerr << "Cannot open source database: " << sqlite3_errmsg(sourceDb_) << std::endl;
		return false;
	}

	rc = sqlite3_open(eventDbPath_.c_str(), &eventDb_);
	if (rc != SQLITE_OK) {
		std::cerr << "Cannot open event database: " << sqlite3_errmsg(eventDb_) << std::endl;
		return false;
	}

	return true;
}

bool EventExtractor::createEventTables() {
	// Main events table
	std::string createEventsTable = R"(
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            event_type TEXT NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            description TEXT,
            file_size INTEGER,
            file_type TEXT
        );
    )";

	char* errMsg = nullptr;
	int rc = sqlite3_exec(eventDb_, createEventsTable.c_str(), nullptr, nullptr, &errMsg);

	if (rc != SQLITE_OK) {
		std::cerr << "Failed to create events table: " << errMsg << std::endl;
		sqlite3_free(errMsg);
		return false;
	}

	// Create summary tables for different event types
	std::string createCreationEventsTable = R"(
        CREATE TABLE IF NOT EXISTS creation_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            file_size INTEGER,
            file_type TEXT
        );
    )";

	std::string createModificationEventsTable = R"(
        CREATE TABLE IF NOT EXISTS modification_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            file_size INTEGER,
            file_type TEXT
        );
    )";

	std::string createAccessEventsTable = R"(
        CREATE TABLE IF NOT EXISTS access_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            file_size INTEGER,
            file_type TEXT
        );
    )";

	std::string createChangeEventsTable = R"(
        CREATE TABLE IF NOT EXISTS change_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            file_size INTEGER,
            file_type TEXT,
            description TEXT
        );
    )";

	std::string createDeletionEventsTable = R"(
        CREATE TABLE IF NOT EXISTS deletion_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            inode INTEGER,
            file_size INTEGER,
            file_type TEXT
        );
    )";

	sqlite3_exec(eventDb_, createCreationEventsTable.c_str(), nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, createModificationEventsTable.c_str(), nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, createAccessEventsTable.c_str(), nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, createChangeEventsTable.c_str(), nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, createDeletionEventsTable.c_str(), nullptr, nullptr, nullptr);

	// Create indices for performance
	sqlite3_exec(eventDb_, "CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);",
		nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, "CREATE INDEX IF NOT EXISTS idx_events_type ON events(event_type);",
		nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, "CREATE INDEX IF NOT EXISTS idx_events_path ON events(file_path);",
		nullptr, nullptr, nullptr);
	sqlite3_exec(eventDb_, "CREATE INDEX IF NOT EXISTS idx_events_inode ON events(inode);",
		nullptr, nullptr, nullptr);

	// Create timeline view
	std::string createTimelineView = R"(
        CREATE VIEW IF NOT EXISTS timeline AS
        SELECT
            datetime(timestamp, 'unixepoch') as event_time,
            event_type,
            file_path,
            inode,
            file_size,
            file_type,
            description
        FROM events
        ORDER BY timestamp DESC;
    )";

	sqlite3_exec(eventDb_, createTimelineView.c_str(), nullptr, nullptr, nullptr);

	// Create statistics view
	std::string createStatsView = R"(
        CREATE VIEW IF NOT EXISTS event_statistics AS
        SELECT
            event_type,
            COUNT(*) as event_count,
            MIN(timestamp) as first_event,
            MAX(timestamp) as last_event,
            datetime(MIN(timestamp), 'unixepoch') as first_event_time,
            datetime(MAX(timestamp), 'unixepoch') as last_event_time
        FROM events
        GROUP BY event_type;
    )";

	sqlite3_exec(eventDb_, createStatsView.c_str(), nullptr, nullptr, nullptr);

	// Create hourly activity view
	std::string createHourlyView = R"(
        CREATE VIEW IF NOT EXISTS hourly_activity AS
        SELECT
            strftime('%Y-%m-%d %H:00:00', datetime(timestamp, 'unixepoch')) as hour,
            event_type,
            COUNT(*) as event_count
        FROM events
        GROUP BY hour, event_type
        ORDER BY hour DESC;
    )";

	sqlite3_exec(eventDb_, createHourlyView.c_str(), nullptr, nullptr, nullptr);

	return true;
}

bool EventExtractor::extractFileSystemEvents() {
	const char* query = R"(
        SELECT inode, path, atime, mtime, ctime, crtime, type, size, is_deleted
        FROM files
        WHERE type = 'REG';
    )";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(sourceDb_, query, -1, &stmt, nullptr);

	if (rc != SQLITE_OK) {
		std::cerr << "Failed to prepare query: " << sqlite3_errmsg(sourceDb_) << std::endl;
		return false;
	}

	int eventCount = 0;
	int creationCount = 0;
	int modificationCount = 0;
	int accessCount = 0;
	int changeCount = 0;
	int deletionCount = 0;

	// Begin transaction for better performance
	sqlite3_exec(eventDb_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int64_t inode = sqlite3_column_int64(stmt, 0);
		std::string path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
		int64_t atime = sqlite3_column_int64(stmt, 2);
		int64_t mtime = sqlite3_column_int64(stmt, 3);
		int64_t ctime = sqlite3_column_int64(stmt, 4);
		int64_t crtime = sqlite3_column_int64(stmt, 5);
		std::string type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
		int64_t size = sqlite3_column_int64(stmt, 7);
		int isDeleted = sqlite3_column_int(stmt, 8);

		// Create event for file creation (birth time)
		if (crtime > 0) {
			TimelineEvent event;
			event.timestamp = crtime;
			event.eventType = "CREATED";
			event.filePath = path;
			event.inode = inode;
			event.description = "File created";
			insertEvent(event);

			// Insert into creation_events table
			std::string sql = R"(
                INSERT INTO creation_events (timestamp, file_path, inode, file_size, file_type)
                VALUES (?, ?, ?, ?, ?);
            )";
			sqlite3_stmt* insertStmt;
			sqlite3_prepare_v2(eventDb_, sql.c_str(), -1, &insertStmt, nullptr);
			sqlite3_bind_int64(insertStmt, 1, crtime);
			sqlite3_bind_text(insertStmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(insertStmt, 3, inode);
			sqlite3_bind_int64(insertStmt, 4, size);
			sqlite3_bind_text(insertStmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(insertStmt);
			sqlite3_finalize(insertStmt);

			eventCount++;
			creationCount++;
		}

		// Create event for file modification
		if (mtime > 0 && mtime != crtime) {
			TimelineEvent event;
			event.timestamp = mtime;
			event.eventType = "MODIFIED";
			event.filePath = path;
			event.inode = inode;
			event.description = "File content modified";
			insertEvent(event);

			// Insert into modification_events table
			std::string sql = R"(
                INSERT INTO modification_events (timestamp, file_path, inode, file_size, file_type)
                VALUES (?, ?, ?, ?, ?);
            )";
			sqlite3_stmt* insertStmt;
			sqlite3_prepare_v2(eventDb_, sql.c_str(), -1, &insertStmt, nullptr);
			sqlite3_bind_int64(insertStmt, 1, mtime);
			sqlite3_bind_text(insertStmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(insertStmt, 3, inode);
			sqlite3_bind_int64(insertStmt, 4, size);
			sqlite3_bind_text(insertStmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(insertStmt);
			sqlite3_finalize(insertStmt);

			eventCount++;
			modificationCount++;
		}

		// Create event for file access
		if (atime > 0 && atime != mtime && atime != crtime) {
			TimelineEvent event;
			event.timestamp = atime;
			event.eventType = "ACCESSED";
			event.filePath = path;
			event.inode = inode;
			event.description = "File accessed/read";
			insertEvent(event);

			// Insert into access_events table
			std::string sql = R"(
                INSERT INTO access_events (timestamp, file_path, inode, file_size, file_type)
                VALUES (?, ?, ?, ?, ?);
            )";
			sqlite3_stmt* insertStmt;
			sqlite3_prepare_v2(eventDb_, sql.c_str(), -1, &insertStmt, nullptr);
			sqlite3_bind_int64(insertStmt, 1, atime);
			sqlite3_bind_text(insertStmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(insertStmt, 3, inode);
			sqlite3_bind_int64(insertStmt, 4, size);
			sqlite3_bind_text(insertStmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(insertStmt);
			sqlite3_finalize(insertStmt);

			eventCount++;
			accessCount++;
		}

		// Create event for metadata change
		if (ctime > 0 && ctime != mtime && ctime != crtime) {
			TimelineEvent event;
			event.timestamp = ctime;
			event.eventType = "CHANGED";
			event.filePath = path;
			event.inode = inode;
			event.description = "File metadata changed (permissions, ownership, etc.)";
			insertEvent(event);

			// Insert into change_events table
			std::string sql = R"(
                INSERT INTO change_events (timestamp, file_path, inode, file_size, file_type, description)
                VALUES (?, ?, ?, ?, ?, ?);
            )";
			sqlite3_stmt* insertStmt;
			sqlite3_prepare_v2(eventDb_, sql.c_str(), -1, &insertStmt, nullptr);
			sqlite3_bind_int64(insertStmt, 1, ctime);
			sqlite3_bind_text(insertStmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(insertStmt, 3, inode);
			sqlite3_bind_int64(insertStmt, 4, size);
			sqlite3_bind_text(insertStmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(insertStmt, 6, "Metadata changed", -1, SQLITE_TRANSIENT);
			sqlite3_step(insertStmt);
			sqlite3_finalize(insertStmt);

			eventCount++;
			changeCount++;
		}

		// Create event for deleted files
		if (isDeleted) {
			// Use the most recent timestamp as deletion time
			int64_t deletionTime = std::max({ atime, mtime, ctime, crtime });

			TimelineEvent event;
			event.timestamp = deletionTime;
			event.eventType = "DELETED";
			event.filePath = path;
			event.inode = inode;
			event.description = "File deleted (unallocated)";
			insertEvent(event);

			// Insert into deletion_events table
			std::string sql = R"(
                INSERT INTO deletion_events (timestamp, file_path, inode, file_size, file_type)
                VALUES (?, ?, ?, ?, ?);
            )";
			sqlite3_stmt* insertStmt;
			sqlite3_prepare_v2(eventDb_, sql.c_str(), -1, &insertStmt, nullptr);
			sqlite3_bind_int64(insertStmt, 1, deletionTime);
			sqlite3_bind_text(insertStmt, 2, path.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(insertStmt, 3, inode);
			sqlite3_bind_int64(insertStmt, 4, size);
			sqlite3_bind_text(insertStmt, 5, type.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(insertStmt);
			sqlite3_finalize(insertStmt);

			eventCount++;
			deletionCount++;
		}
	}

	sqlite3_finalize(stmt);

	// Commit transaction
	sqlite3_exec(eventDb_, "COMMIT;", nullptr, nullptr, nullptr);

	std::cout << "  Total events: " << eventCount << std::endl;
	std::cout << "    - Creation events: " << creationCount << std::endl;
	std::cout << "    - Modification events: " << modificationCount << std::endl;
	std::cout << "    - Access events: " << accessCount << std::endl;
	std::cout << "    - Change events: " << changeCount << std::endl;
	std::cout << "    - Deletion events: " << deletionCount << std::endl;

	return true;
}

bool EventExtractor::insertEvent(const TimelineEvent& event) {
	const char* sql = R"(
        INSERT INTO events (timestamp, event_type, file_path, inode, description, file_size, file_type)
        VALUES (?, ?, ?, ?, ?, 0, '');
    )";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(eventDb_, sql, -1, &stmt, nullptr);

	if (rc != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int64(stmt, 1, event.timestamp);
	sqlite3_bind_text(stmt, 2, event.eventType.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, event.filePath.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 4, event.inode);
	sqlite3_bind_text(stmt, 5, event.description.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE;
}

void EventExtractor::closeDatabases() {
	if (sourceDb_) {
		sqlite3_close(sourceDb_);
		sourceDb_ = nullptr;
	}

	if (eventDb_) {
		sqlite3_close(eventDb_);
		eventDb_ = nullptr;
	}
}