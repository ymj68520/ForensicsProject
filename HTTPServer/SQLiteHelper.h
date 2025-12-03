#pragma once
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class SQLiteHelper {
public:
    static nlohmann::json get_file_summary(const std::string& db_path) {
        sqlite3* db;
        nlohmann::json result;
        
        if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
            result["error"] = "Cannot open database";
            return result;
        }

        const char* sql = "SELECT * FROM file_summary"; // 查询项目中的视图
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            std::vector<nlohmann::json> rows;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                nlohmann::json row;
                row["category"] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
                row["file_count"] = sqlite3_column_int(stmt, 1);
                row["total_size"] = (long long)sqlite3_column_int64(stmt, 2);
                rows.push_back(std::move(row));
            }
            result["summary"] = std::move(rows);
        } else {
            result["error"] = "Query failed: " + std::string(sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }
};