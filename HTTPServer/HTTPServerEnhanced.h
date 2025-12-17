#pragma once

#include <crow.h>
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <sqlite3.h>
#include "TaskManager.h"
#include "SQLiteHelper.h"
#include "Utils.h"

namespace forensics {
    using json = nlohmann::json;

    // Standard API response wrapper
    struct ApiResponse {
        bool success = true;
        std::string message;
        nlohmann::json data;
        std::string timestamp;
        nlohmann::json pagination;
        std::string error_code;

        nlohmann::json to_json() const {
            nlohmann::json response;
            response["success"] = success;
            response["message"] = message;
            response["data"] = data;
            response["timestamp"] = timestamp;
            if (!pagination.empty()) {
                response["pagination"] = pagination;
            }
            if (!error_code.empty()) {
                response["error_code"] = error_code;
            }
            return response;
        }

        static ApiResponse success(const std::string& msg = "", const nlohmann::json& data = nullptr) {
            ApiResponse response;
            response.success = true;
            response.message = msg;
            response.data = data ? data : nlohmann::json::object();
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            response.timestamp = std::ctime(&time_t);
            response.timestamp = response.timestamp.substr(0, response.timestamp.length() - 1); // Remove newline
            return response;
        }

        static ApiResponse error(const std::string& msg, const std::string& error_code = "") {
            ApiResponse response;
            response.success = false;
            response.message = msg;
            response.error_code = error_code;
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            response.timestamp = std::ctime(&time_t);
            response.timestamp = response.timestamp.substr(0, response.timestamp.length() - 1);
            return response;
        }
    };

    // Database connection pool
    class ConnectionPool {
    private:
        std::vector<std::unique_ptr<sqlite3*>> connections_;
        std::vector<std::mutex> mutexes_;
        std::string db_path_;
        size_t max_connections_;

    public:
        ConnectionPool(const std::string& db_path, size_t max_connections = 5)
            : db_path_(db_path), max_connections_(max_connections) {
            connections_.reserve(max_connections_);
            mutexes_.resize(max_connections_);

            for (size_t i = 0; i < max_connections_; ++i) {
                auto conn = std::make_unique<sqlite3*>();
                if (sqlite3_open(db_path.c_str(), conn.get()) == SQLITE_OK) {
                    connections_.push_back(std::move(conn));
                }
            }
        }

        ~ConnectionPool() {
            for (auto& conn : connections_) {
                if (conn && *conn) {
                    sqlite3_close(*conn);
                }
            }
        }

        struct ConnectionGuard {
            sqlite3* conn;
            std::mutex* mutex;
            ConnectionGuard(sqlite3* c, std::mutex* m) : conn(c), mutex(m) {}
            ~ConnectionGuard() {
                if (mutex) mutex->unlock();
            }
        };

        ConnectionGuard get_connection() {
            static size_t next_idx = 0;
            for (size_t i = 0; i < connections_.size(); ++i) {
                size_t idx = (next_idx + i) % connections_.size();
                if (connections_[idx] && mutexes_[idx].try_lock()) {
                    next_idx = (idx + 1) % connections_.size();
                    return ConnectionGuard(*connections_[idx], &mutexes_[idx]);
                }
            }
            // Fallback: wait for the first connection
            mutexes_[0].lock();
            return ConnectionGuard(*connections_[0], &mutexes_[0]);
        }
    };

    // Enhanced SQLite Helper with caching and performance optimizations
    class SQLiteHelperEnhanced {
    private:
        static std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> connection_pools_;
        static std::mutex pool_mutex_;

        // Cache for expensive queries
        struct CacheEntry {
            nlohmann::json data;
            std::chrono::steady_clock::time_point timestamp;
            int ttl_seconds;
        };
        static std::unordered_map<std::string, CacheEntry> query_cache_;
        static std::mutex cache_mutex_;

        static std::shared_ptr<ConnectionPool> get_connection_pool(const std::string& db_path) {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            auto it = connection_pools_.find(db_path);
            if (it == connection_pools_.end()) {
                auto pool = std::make_shared<ConnectionPool>(db_path);
                connection_pools_[db_path] = pool;
                return pool;
            }
            return it->second;
        }

        static nlohmann::json get_cached_result(const std::string& cache_key) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = query_cache_.find(cache_key);
            if (it != query_cache_.end()) {
                auto now = std::chrono::steady_clock::now();
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp).count();
                if (age < it->second.ttl_seconds) {
                    return it->second.data;
                } else {
                    query_cache_.erase(it);
                }
            }
            return nullptr;
        }

        static void cache_result(const std::string& cache_key, const nlohmann::json& data, int ttl_seconds = 300) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            CacheEntry entry;
            entry.data = data;
            entry.timestamp = std::chrono::steady_clock::now();
            entry.ttl_seconds = ttl_seconds;
            query_cache_[cache_key] = entry;
        }

        static nlohmann::json execute_query_with_timeout(const std::string& db_path, const std::string& sql, int timeout_seconds = 30) {
            auto pool = get_connection_pool(db_path);
            auto conn_guard = pool->get_connection();
            sqlite3* db = conn_guard.conn;

            // Set timeout
            sqlite3_busy_timeout(db, timeout_seconds * 1000);

            sqlite3_stmt* stmt;
            nlohmann::json result;

            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                std::vector<nlohmann::json> rows;
                int column_count = sqlite3_column_count(stmt);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    nlohmann::json row;
                    for (int i = 0; i < column_count; ++i) {
                        const char* column_name = sqlite3_column_name(stmt, i);
                        switch (sqlite3_column_type(stmt, i)) {
                            case SQLITE_INTEGER:
                                row[column_name] = sqlite3_column_int64(stmt, i);
                                break;
                            case SQLITE_FLOAT:
                                row[column_name] = sqlite3_column_double(stmt, i);
                                break;
                            case SQLITE_TEXT:
                                row[column_name] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
                                break;
                            case SQLITE_BLOB:
                                // Skip BLOB data for API responses
                                row[column_name] = "[BLOB]";
                                break;
                            case SQLITE_NULL:
                                row[column_name] = nullptr;
                                break;
                        }
                    }
                    rows.push_back(std::move(row));
                }

                if (rows.size() == 1) {
                    result = rows[0];
                } else {
                    result["rows"] = rows;
                    result["count"] = rows.size();
                }
            } else {
                result["error"] = "Query failed: " + std::string(sqlite3_errmsg(db));
            }

            sqlite3_finalize(stmt);
            return result;
        }

    public:
        static nlohmann::json get_database_schema(const std::string& db_path, const std::string& table_pattern = "%") {
            std::string cache_key = "schema:" + db_path + ":" + table_pattern;
            auto cached = get_cached_result(cache_key);
            if (!cached.empty()) {
                return cached;
            }

            nlohmann::json schema;

            // Get table information
            std::string tables_sql = "SELECT name, type FROM sqlite_master WHERE name LIKE ? AND type IN ('table', 'view')";
            auto pool = get_connection_pool(db_path);
            auto conn_guard = pool->get_connection();
            sqlite3* db = conn_guard.conn;

            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, tables_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, table_pattern.c_str(), -1, SQLITE_TRANSIENT);

                nlohmann::json tables = nlohmann::json::array();
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string table_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string table_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

                    nlohmann::json table_info;
                    table_info["name"] = table_name;
                    table_info["type"] = table_type;

                    // Get column information
                    std::string pragma_sql = "PRAGMA table_info(" + table_name + ")";
                    sqlite3_stmt* pragma_stmt;
                    if (sqlite3_prepare_v2(db, pragma_sql.c_str(), -1, &pragma_stmt, nullptr) == SQLITE_OK) {
                        nlohmann::json columns = nlohmann::json::array();
                        while (sqlite3_step(pragma_stmt) == SQLITE_ROW) {
                            nlohmann::json column;
                            column["cid"] = sqlite3_column_int(pragma_stmt, 0);
                            column["name"] = reinterpret_cast<const char*>(sqlite3_column_text(pragma_stmt, 1));
                            column["data_type"] = reinterpret_cast<const char*>(sqlite3_column_text(pragma_stmt, 2));
                            column["not_null"] = sqlite3_column_int(pragma_stmt, 3) == 1;
                            column["default_value"] = sqlite3_column_text(pragma_stmt, 4) ?
                                reinterpret_cast<const char*>(sqlite3_column_text(pragma_stmt, 4)) : nullptr;
                            column["primary_key"] = sqlite3_column_int(pragma_stmt, 5) == 1;
                            columns.push_back(column);
                        }
                        table_info["columns"] = columns;
                        sqlite3_finalize(pragma_stmt);
                    }

                    // Get indexes
                    std::string index_sql = "PRAGMA index_list(" + table_name + ")";
                    sqlite3_stmt* index_stmt;
                    if (sqlite3_prepare_v2(db, index_sql.c_str(), -1, &index_stmt, nullptr) == SQLITE_OK) {
                        nlohmann::json indexes = nlohmann::json::array();
                        while (sqlite3_step(index_stmt) == SQLITE_ROW) {
                            nlohmann::json index;
                            index["seq"] = sqlite3_column_int(index_stmt, 0);
                            index["name"] = reinterpret_cast<const char*>(sqlite3_column_text(index_stmt, 1));
                            index["unique"] = sqlite3_column_int(index_stmt, 2) == 1;
                            index["origin"] = reinterpret_cast<const char*>(sqlite3_column_text(index_stmt, 3));
                            index["partial"] = sqlite3_column_int(index_stmt, 4) == 1;
                            indexes.push_back(index);
                        }
                        table_info["indexes"] = indexes;
                        sqlite3_finalize(index_stmt);
                    }

                    tables.push_back(table_info);
                }
                schema["tables"] = tables;
            }

            sqlite3_finalize(stmt);

            // Get view definitions
            std::string views_sql = "SELECT name, sql FROM sqlite_master WHERE type = 'view' AND name LIKE ?";
            if (sqlite3_prepare_v2(db, views_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, table_pattern.c_str(), -1, SQLITE_TRANSIENT);

                nlohmann::json views = nlohmann::json::array();
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    nlohmann::json view;
                    view["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    view["sql"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    views.push_back(view);
                }
                schema["views"] = views;
            }

            sqlite3_finalize(stmt);

            cache_result(cache_key, schema, 3600); // Cache schema for 1 hour
            return schema;
        }

        static nlohmann::json get_database_info(const std::string& db_path) {
            std::string cache_key = "db_info:" + db_path;
            auto cached = get_cached_result(cache_key);
            if (!cached.empty()) {
                return cached;
            }

            nlohmann::json info;

            // Basic database info
            auto pool = get_connection_pool(db_path);
            auto conn_guard = pool->get_connection();
            sqlite3* db = conn_guard.conn;

            // Get page count, page size, etc.
            std::string pragma_sql = "PRAGMA page_count";
            auto result = execute_query_with_timeout(db_path, pragma_sql);
            if (result.contains("page_count")) {
                info["page_count"] = result["page_count"];
            }

            pragma_sql = "PRAGMA page_size";
            result = execute_query_with_timeout(db_path, pragma_sql);
            if (result.contains("page_size")) {
                info["page_size"] = result["page_size"];
            }

            // Get table counts
            std::string table_count_sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'";
            result = execute_query_with_timeout(db_path, table_count_sql);
            if (result.contains("COUNT(*)")) {
                info["table_count"] = result["COUNT(*)"];
            }

            // Get view counts
            std::string view_count_sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'view'";
            result = execute_query_with_timeout(db_path, view_count_sql);
            if (result.contains("COUNT(*)")) {
                info["view_count"] = result["COUNT(*)"];
            }

            // Get index counts
            std::string index_count_sql = "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index'";
            result = execute_query_with_timeout(db_path, index_count_sql);
            if (result.contains("COUNT(*)")) {
                info["index_count"] = result["COUNT(*)"];
            }

            // Calculate approximate database size
            if (info.contains("page_count") && info.contains("page_size")) {
                info["estimated_size_bytes"] = info["page_count"].get<long long>() * info["page_size"].get<long long>();
            }

            cache_result(cache_key, info, 300); // Cache for 5 minutes
            return info;
        }

        static nlohmann::json export_data(const std::string& db_path, const std::string& table_name,
                                         const std::string& format = "json", int limit = 1000, int offset = 0) {
            std::string cache_key = "export:" + db_path + ":" + table_name + ":" + format + ":" +
                                   std::to_string(limit) + ":" + std::to_string(offset);
            auto cached = get_cached_result(cache_key);
            if (!cached.empty() && format == "json") {
                return cached;
            }

            // Sanitize table name to prevent SQL injection
            if (table_name.find("DROP") != std::string::npos ||
                table_name.find("DELETE") != std::string::npos ||
                table_name.find(";") != std::string::npos) {
                return ApiResponse::error("Invalid table name", "INVALID_TABLE").to_json();
            }

            std::string sql = "SELECT * FROM " + table_name + " LIMIT ? OFFSET ?";
            auto pool = get_connection_pool(db_path);
            auto conn_guard = pool->get_connection();
            sqlite3* db = conn_guard.conn;

            sqlite3_stmt* stmt;
            nlohmann::json result;

            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, limit);
                sqlite3_bind_int(stmt, 2, offset);

                if (format == "json") {
                    nlohmann::json rows = nlohmann::json::array();
                    int column_count = sqlite3_column_count(stmt);
                    std::vector<std::string> column_names;

                    for (int i = 0; i < column_count; ++i) {
                        column_names.push_back(sqlite3_column_name(stmt, i));
                    }

                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        nlohmann::json row;
                        for (int i = 0; i < column_count; ++i) {
                            switch (sqlite3_column_type(stmt, i)) {
                                case SQLITE_INTEGER:
                                    row[column_names[i]] = sqlite3_column_int64(stmt, i);
                                    break;
                                case SQLITE_FLOAT:
                                    row[column_names[i]] = sqlite3_column_double(stmt, i);
                                    break;
                                case SQLITE_TEXT:
                                    row[column_names[i]] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
                                    break;
                                case SQLITE_BLOB:
                                    row[column_names[i]] = "[BLOB]";
                                    break;
                                case SQLITE_NULL:
                                    row[column_names[i]] = nullptr;
                                    break;
                            }
                        }
                        rows.push_back(std::move(row));
                    }

                    result["data"] = rows;
                    result["total_exported"] = rows.size();

                    // Get total count for pagination
                    std::string count_sql = "SELECT COUNT(*) FROM " + table_name;
                    sqlite3_stmt* count_stmt;
                    if (sqlite3_prepare_v2(db, count_sql.c_str(), -1, &count_stmt, nullptr) == SQLITE_OK) {
                        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
                            result["total_available"] = sqlite3_column_int64(count_stmt, 0);
                        }
                        sqlite3_finalize(count_stmt);
                    }

                    result["limit"] = limit;
                    result["offset"] = offset;

                    cache_result(cache_key, result, 60); // Cache exports for 1 minute
                } else if (format == "csv") {
                    // CSV format implementation
                    std::string csv_content;
                    int column_count = sqlite3_column_count(stmt);

                    // Header row
                    for (int i = 0; i < column_count; ++i) {
                        if (i > 0) csv_content += ",";
                        csv_content += sqlite3_column_name(stmt, i);
                    }
                    csv_content += "\n";

                    // Data rows
                    int row_count = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW && row_count < limit) {
                        for (int i = 0; i < column_count; ++i) {
                            if (i > 0) csv_content += ",";

                            if (sqlite3_column_type(stmt, i) == SQLITE_TEXT) {
                                csv_content += "\"";
                                std::string text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                                // Escape quotes in CSV
                                size_t pos = 0;
                                while ((pos = text.find("\"", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "\"\"");
                                    pos += 2;
                                }
                                csv_content += text;
                                csv_content += "\"";
                            } else if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
                                csv_content += "";
                            } else {
                                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                                if (text) csv_content += text;
                            }
                        }
                        csv_content += "\n";
                        row_count++;
                    }

                    result["csv_content"] = csv_content;
                    result["total_exported"] = row_count;
                } else if (format == "xml") {
                    // XML format implementation
                    std::string xml_content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                    xml_content += "<data>\n";
                    xml_content += "<table name=\"" + table_name + "\">\n";

                    int column_count = sqlite3_column_count(stmt);
                    std::vector<std::string> column_names;

                    for (int i = 0; i < column_count; ++i) {
                        column_names.push_back(sqlite3_column_name(stmt, i));
                    }

                    int row_count = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW && row_count < limit) {
                        xml_content += "  <row>\n";
                        for (int i = 0; i < column_count; ++i) {
                            xml_content += "    <" + column_names[i] + ">";
                            if (sqlite3_column_type(stmt, i) == SQLITE_TEXT) {
                                std::string text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                                // Escape XML special characters
                                size_t pos = 0;
                                while ((pos = text.find("&", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "&amp;");
                                    pos += 5;
                                }
                                pos = 0;
                                while ((pos = text.find("<", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "&lt;");
                                    pos += 4;
                                }
                                pos = 0;
                                while ((pos = text.find(">", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "&gt;");
                                    pos += 4;
                                }
                                pos = 0;
                                while ((pos = text.find("\"", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "&quot;");
                                    pos += 6;
                                }
                                pos = 0;
                                while ((pos = text.find("'", pos)) != std::string::npos) {
                                    text.replace(pos, 1, "&apos;");
                                    pos += 6;
                                }
                                xml_content += text;
                            } else if (sqlite3_column_type(stmt, i) != SQLITE_NULL) {
                                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                                if (text) xml_content += text;
                            }
                            xml_content += "</" + column_names[i] + ">\n";
                        }
                        xml_content += "  </row>\n";
                        row_count++;
                    }

                    xml_content += "</table>\n";
                    xml_content += "</data>\n";

                    result["xml_content"] = xml_content;
                    result["total_exported"] = row_count;
                } else {
                    result = ApiResponse::error("Unsupported export format: " + format, "UNSUPPORTED_FORMAT").to_json();
                }
            } else {
                result = ApiResponse::error("Failed to prepare export query: " + std::string(sqlite3_errmsg(db)), "QUERY_FAILED").to_json();
            }

            sqlite3_finalize(stmt);
            return result;
        }

        static void clear_cache() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            query_cache_.clear();
        }

        static nlohmann::json get_cache_stats() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            nlohmann::json stats;
            stats["cached_queries"] = query_cache_.size();

            auto now = std::chrono::steady_clock::now();
            int expired_count = 0;
            for (const auto& [key, entry] : query_cache_) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp).count();
                if (age >= entry.ttl_seconds) {
                    expired_count++;
                }
            }
            stats["expired_entries"] = expired_count;
            return stats;
        }

        // Method to list all databases for a task
        static nlohmann::json get_task_databases(const std::string& task_id) {
            nlohmann::json databases;
            databases["raw_db"] = "";
            databases["events_db"] = "";
            databases["files_db"] = "";
            databases["android_db"] = "";

            // Try to find database files based on task
            // This would need to be implemented based on how task storage is organized
            // For now, return placeholder structure

            return databases;
        }
    };

    // Static member definitions
    std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> SQLiteHelperEnhanced::connection_pools_;
    std::mutex SQLiteHelperEnhanced::pool_mutex_;
    std::unordered_map<std::string, SQLiteHelperEnhanced::CacheEntry> SQLiteHelperEnhanced::query_cache_;
    std::mutex SQLiteHelperEnhanced::cache_mutex_;

    // Enhanced HTTP Server with Phase 4 features
    class HTTPServerEnhanced {
    public:
        HTTPServerEnhanced(asio::io_context& ioc);
        ~HTTPServerEnhanced() = default;

        void run(int port = 8080);

    private:
        crow::App<> app_;
        TaskManager& task_manager_;
        asio::io_context& ioc_;

        // Original handlers
        crow::response handle_create_task(const crow::request& req);
        crow::response handle_get_task(const crow::request& req, const std::string& task_id);
        crow::response handle_get_task_results(const crow::request& req, const std::string& task_id);

        // Phase 4: System Information Endpoints
        crow::response handle_health_check(const crow::request& req);
        crow::response handle_system_info(const crow::request& req);
        crow::response handle_list_databases(const crow::request& req, const std::string& task_id);
        crow::response handle_database_schema(const crow::request& req, const std::string& type);

        // Phase 4: Utility Endpoints
        crow::response handle_api_docs(const crow::request& req);
        crow::response handle_database_schema_docs(const crow::request& req);
        crow::response handle_export_data(const crow::request& req, const std::string& task_id);

        // Helper methods
        crow::response create_standard_response(const ApiResponse& api_response);
        crow::response create_error_response(const std::string& message, const std::string& error_code = "", int status_code = 400);
        nlohmann::json get_system_capabilities();
        nlohmann::json get_endpoint_documentation();
        nlohmann::json get_database_schema_documentation();
    };
}