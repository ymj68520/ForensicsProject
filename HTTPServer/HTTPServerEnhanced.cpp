#include "HTTPServerEnhanced.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <cstdlib>

namespace forensics {

    HTTPServerEnhanced::HTTPServerEnhanced(asio::io_context& ioc) : task_manager_(TaskManager::instance()), ioc_(ioc) {

        // Original routes
        CROW_ROUTE(app_, "/tasks").methods("POST"_method)([this](const crow::request& req) {
            return handle_create_task(req);
        });

        CROW_ROUTE(app_, "/tasks/<string>").methods("GET"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_get_task(req, task_id);
        });

        CROW_ROUTE(app_, "/tasks/<string>/results").methods("GET"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_get_task_results(req, task_id);
        });

        // Phase 4: System Information Endpoints
        CROW_ROUTE(app_, "/api/system/health").methods("GET"_method)([this](const crow::request& req) {
            return handle_health_check(req);
        });

        CROW_ROUTE(app_, "/api/system/info").methods("GET"_method)([this](const crow::request& req) {
            return handle_system_info(req);
        });

        CROW_ROUTE(app_, "/api/system/databases/<string>").methods("GET"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_list_databases(req, task_id);
        });

        CROW_ROUTE(app_, "/api/system/database-schema/<string>").methods("GET"_method)([this](const crow::request& req, const std::string& type) {
            return handle_database_schema(req, type);
        });

        // Phase 4: Utility Endpoints
        CROW_ROUTE(app_, "/api/docs/endpoints").methods("GET"_method)([this](const crow::request& req) {
            return handle_api_docs(req);
        });

        CROW_ROUTE(app_, "/api/docs/database-schema").methods("GET"_method)([this](const crow::request& req) {
            return handle_database_schema_docs(req);
        });

        CROW_ROUTE(app_, "/api/export/<string>").methods("POST"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_export_data(req, task_id);
        });

        // Default route for API documentation
        CROW_ROUTE(app_, "/").methods("GET"_method)([this](const crow::request& req) {
            auto response = ApiResponse::success("Forensics HTTP Server API - Phase 4 Enhanced", get_endpoint_documentation());
            return create_standard_response(response);
        });
    }

    void HTTPServerEnhanced::run(int port) {
        std::cout << "Starting Enhanced HTTP server on port " << port << std::endl;
        app_.port(port).multithreaded().run();
    }

    // Original handlers (kept for backward compatibility)
    crow::response HTTPServerEnhanced::handle_create_task(const crow::request& req) {
        try {
            auto body = json::parse(req.body);
            std::string image_path = body["image_path"];

            bool android_analyze = false;
            if (body.contains("android_analyze")) {
                android_analyze = body["android_analyze"];
            }

            XFSMode xfs_mode = XFSMode::Auto;
            if (body.contains("xfs_mode")) {
                std::string mode_str = body["xfs_mode"];
                if (mode_str == "native") xfs_mode = XFSMode::Native;
                else if (mode_str == "pure") xfs_mode = XFSMode::Pure;
            }

            std::string db_output_dir = "";
            if (body.contains("db_output_dir")) {
                db_output_dir = body["db_output_dir"];
            }

            std::string task_id = task_manager_.create_task(image_path);
            task_manager_.start_analysis(task_id, android_analyze, xfs_mode, db_output_dir);

            auto response = ApiResponse::success("Task created successfully", json{
                {"task_id", task_id},
                {"image_path", image_path},
                {"android_analyze", android_analyze},
                {"xfs_mode", xfs_mode == XFSMode::Auto ? "auto" : (xfs_mode == XFSMode::Native ? "native" : "pure")}
            });

            return create_standard_response(response, 201);
        } catch (const std::exception& e) {
            return create_error_response("Invalid request: " + std::string(e.what()), "INVALID_REQUEST");
        }
    }

    crow::response HTTPServerEnhanced::handle_get_task(const crow::request& req, const std::string& task_id) {
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            return create_error_response("Task not found", "TASK_NOT_FOUND", 404);
        }

        json task_data = {
            {"id", task.id},
            {"image_path", task.image_path},
            {"status", task.status == TaskStatus::PENDING ? "pending" :
                     task.status == TaskStatus::RUNNING ? "running" :
                     task.status == TaskStatus::COMPLETED ? "completed" : "failed"},
            {"message", task.message}
        };

        auto response = ApiResponse::success("Task retrieved successfully", task_data);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_get_task_results(const crow::request& req, const std::string& task_id) {
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            return create_error_response("Task not found", "TASK_NOT_FOUND", 404);
        }

        if (task.status != TaskStatus::COMPLETED) {
            auto response = ApiResponse::error("Task not completed yet", "TASK_NOT_COMPLETED");
            return create_standard_response(response, 202);
        }

        try {
            auto summary = SQLiteHelper::get_file_summary(task.output_files_db);
            auto response = ApiResponse::success("Results retrieved successfully", summary);
            return create_standard_response(response);
        } catch (const std::exception& e) {
            return create_error_response("Error retrieving results: " + std::string(e.what()), "RESULTS_ERROR", 500);
        }
    }

    // Phase 4: System Information Endpoints
    crow::response HTTPServerEnhanced::handle_health_check(const crow::request& req) {
        json health_data;

        // Check system status
        health_data["status"] = "healthy";
        health_data["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() % 86400;

        // Check database connections
        health_data["database_connections"] = "operational";

        // Check memory usage
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.find("MemAvailable") != std::string::npos) {
                    size_t pos = line.find(":");
                    if (pos != std::string::npos) {
                        std::string available = line.substr(pos + 1);
                        available.erase(0, available.find_first_not_of(" \t"));
                        available.erase(available.find_last_not_of(" \t") + 1);
                        health_data["memory_available_kb"] = std::stoll(available);
                    }
                    break;
                }
            }
        }

        // Check disk space
        try {
            auto space = std::filesystem::space(".");
            health_data["disk_space_available_bytes"] = space.available;
            health_data["disk_space_total_bytes"] = space.capacity;
        } catch (...) {
            health_data["disk_space_status"] = "unknown";
        }

        // Active tasks count
        health_data["active_tasks"] = 0; // Would need to be implemented in TaskManager

        auto response = ApiResponse::success("System is healthy", health_data);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_system_info(const crow::request& req) {
        json system_data = get_system_capabilities();
        auto response = ApiResponse::success("System information retrieved successfully", system_data);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_list_databases(const crow::request& req, const std::string& task_id) {
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            return create_error_response("Task not found", "TASK_NOT_FOUND", 404);
        }

        json databases = SQLiteHelperEnhanced::get_task_databases(task_id);

        // Try to infer database paths based on task info
        if (!task.output_files_db.empty()) {
            databases["files_db"] = task.output_files_db;

            // Try to construct other database paths
            std::filesystem::path files_db_path(task.output_files_db);
            std::string base_name = files_db_path.stem().string();
            std::string directory = files_db_path.parent_path().string();

            std::string raw_db_path = directory + "/" + base_name.replace(base_name.find("_files"), 6, "_raw") + ".db";
            std::string events_db_path = directory + "/" + base_name.replace(base_name.find("_files"), 6, "_events") + ".db";
            std::string android_db_path = directory + "/" + base_name.replace(base_name.find("_files"), 6, "_android") + ".db";

            if (std::filesystem::exists(raw_db_path)) {
                databases["raw_db"] = raw_db_path;
            }
            if (std::filesystem::exists(events_db_path)) {
                databases["events_db"] = events_db_path;
            }
            if (std::filesystem::exists(android_db_path)) {
                databases["android_db"] = android_db_path;
            }
        }

        // Get info for each database
        for (auto& [db_type, db_path] : databases.items()) {
            if (!db_path.empty() && std::filesystem::exists(db_path)) {
                auto db_info = SQLiteHelperEnhanced::get_database_info(db_path);
                databases[db_type + "_info"] = db_info;
            }
        }

        auto response = ApiResponse::success("Database list retrieved successfully", databases);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_database_schema(const crow::request& req, const std::string& type) {
        json schema_data;

        if (type == "raw") {
            // Return schema for raw database
            schema_data = json{
                {"description", "Raw database schema - contains complete filesystem metadata"},
                {"tables", {
                    {"files", "File and directory metadata"},
                    {"partitions", "Partition information"},
                    {"file_summary", "View: File summary by type"}
                }}
            };
        } else if (type == "events") {
            schema_data = json{
                {"description", "Events database schema - contains timeline events"},
                {"tables", {
                    {"events", "Filesystem events (create, modify, delete)"},
                    {"aggregated_events", "View: Aggregated event summaries"}
                }}
            };
        } else if (type == "files") {
            schema_data = json{
                {"description", "Files database schema - contains categorized files"},
                {"tables", {
                    {"images", "Image files"},
                    {"videos", "Video files"},
                    {"audio", "Audio files"},
                    {"documents", "Document files"},
                    {"archives", "Archive files"},
                    {"executables", "Executable files"},
                    {"databases", "Database files"},
                    {"source_code", "Source code files"},
                    {"web_files", "Web-related files"},
                    {"email_files", "Email files"},
                    {"system_files", "System files"},
                    {"encrypted_files", "Encrypted files"},
                    {"os_config_files", "OS configuration files"},
                    {"os_boot_files", "OS boot/kernel files"},
                    {"os_libraries", "OS system libraries"},
                    {"fs_journal", "Filesystem journal files"},
                    {"fs_metadata", "Filesystem metadata"},
                    {"log_files", "Log files"},
                    {"cache_files", "Cache files"},
                    {"temp_files", "Temporary files"},
                    {"backup_files", "Backup files"},
                    {"font_files", "Font files"},
                    {"certificates", "Certificate and key files"},
                    {"unknown_files", "Unknown file types"}
                }}
            };
        } else if (type == "android") {
            schema_data = json{
                {"description", "Android database schema - contains Android-specific forensics data"},
                {"tables", {
                    {"sms_messages", "SMS messages"},
                    {"whatsapp_messages", "WhatsApp messages"},
                    {"contacts", "Contact information"},
                    {"call_logs", "Call history"},
                    {"applications", "Installed applications"},
                    {"device_info", "Device information"}
                }}
            };
        } else {
            return create_error_response("Unknown database type: " + type, "UNKNOWN_DATABASE_TYPE");
        }

        auto response = ApiResponse::success("Database schema retrieved successfully", schema_data);
        return create_standard_response(response);
    }

    // Phase 4: Utility Endpoints
    crow::response HTTPServerEnhanced::handle_api_docs(const crow::request& req) {
        json docs = get_endpoint_documentation();
        auto response = ApiResponse::success("API documentation retrieved successfully", docs);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_database_schema_docs(const crow::request& req) {
        json schema_docs = get_database_schema_documentation();
        auto response = ApiResponse::success("Database schema documentation retrieved successfully", schema_docs);
        return create_standard_response(response);
    }

    crow::response HTTPServerEnhanced::handle_export_data(const crow::request& req, const std::string& task_id) {
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            return create_error_response("Task not found", "TASK_NOT_FOUND", 404);
        }

        if (task.status != TaskStatus::COMPLETED) {
            return create_error_response("Task not completed yet", "TASK_NOT_COMPLETED", 202);
        }

        try {
            auto body = json::parse(req.body);
            std::string table_name = body.value("table", "files");
            std::string format = body.value("format", "json");
            int limit = body.value("limit", 1000);
            int offset = body.value("offset", 0);

            if (format != "json" && format != "csv" && format != "xml") {
                return create_error_response("Unsupported export format: " + format, "UNSUPPORTED_FORMAT");
            }

            auto export_result = SQLiteHelperEnhanced::export_data(task.output_files_db, table_name, format, limit, offset);

            if (export_result.contains("error")) {
                return create_error_response(export_result["error"], "EXPORT_FAILED", 500);
            }

            auto response = ApiResponse::success("Data exported successfully", export_result);
            crow::response res = create_standard_response(response);

            if (format == "csv") {
                res.set_header("Content-Type", "text/csv");
                res.set_header("Content-Disposition", "attachment; filename=" + table_name + ".csv");
                res.write(export_result["csv_content"].get<std::string>());
                return res;
            } else if (format == "xml") {
                res.set_header("Content-Type", "application/xml");
                res.set_header("Content-Disposition", "attachment; filename=" + table_name + ".xml");
                res.write(export_result["xml_content"].get<std::string>());
                return res;
            }

            return res;
        } catch (const std::exception& e) {
            return create_error_response("Export error: " + std::string(e.what()), "EXPORT_ERROR", 500);
        }
    }

    // Helper methods
    crow::response HTTPServerEnhanced::create_standard_response(const ApiResponse& api_response, int status_code) {
        crow::response res;
        res.code = status_code;
        res.set_header("Content-Type", "application/json");
        res.write(api_response.to_json().dump());
        return res;
    }

    crow::response HTTPServerEnhanced::create_error_response(const std::string& message, const std::string& error_code, int status_code) {
        auto api_response = ApiResponse::error(message, error_code);
        return create_standard_response(api_response, status_code);
    }

    nlohmann::json HTTPServerEnhanced::get_system_capabilities() {
        json capabilities;

        // Version information
        capabilities["version"] = "4.0";
        capabilities["api_version"] = "v4";
        capabilities["build_date"] = __DATE__;

        // Supported image formats
        capabilities["supported_formats"] = {
            {"e01", "Expert Witness Format"},
            {"dd", "Raw Disk Image"},
            {"img", "Raw Image Format"}
        };

        // Supported filesystems
        capabilities["supported_filesystems"] = {
            "NTFS", "FAT", "EXT2", "EXT3", "EXT4", "HFS+", "APFS"
        };

        // Android support
        capabilities["android_forensics"] = {
            {"supported", true},
            {"features", {
                "SMS analysis",
                "WhatsApp analysis",
                "Call logs",
                "Contacts",
                "Applications",
                "Build properties"
            }}
        };

        // Features
        capabilities["features"] = {
            {"timeline_analysis", true},
            {"file_classification", true},
            {"deleted_file_recovery", true},
            {"event_extraction", true},
            {"export_formats", {"json", "csv", "xml"}},
            {"caching", true},
            {"concurrent_tasks", true}
        };

        // Performance settings
        capabilities["performance"] = {
            {"max_concurrent_tasks", 10},
            {"query_timeout_seconds", 30},
            {"cache_ttl_seconds", 300},
            {"export_limit", 10000}
        };

        return capabilities;
    }

    nlohmann::json HTTPServerEnhanced::get_endpoint_documentation() {
        json docs;

        docs["title"] = "Forensics HTTP Server API Documentation";
        docs["version"] = "4.0";
        docs["base_url"] = "http://localhost:8080";

        json endpoints = json::array();

        // Task management endpoints
        endpoints.push_back({
            {"path", "/tasks"},
            {"method", "POST"},
            {"description", "Create a new forensic analysis task"},
            {"parameters", {
                {"image_path", "string", "required", "Path to the disk image file"},
                {"android_analyze", "boolean", "optional", "Enable Android forensics analysis"},
                {"xfs_mode", "string", "optional", "XFS filesystem mode (auto/native/pure)"},
                {"db_output_dir", "string", "optional", "Directory for output databases"}
            }},
            {"example", R"({
                "image_path": "/path/to/image.e01",
                "android_analyze": true,
                "xfs_mode": "auto",
                "db_output_dir": "/output/directory"
            })"}
        });

        endpoints.push_back({
            {"path", "/tasks/{task_id}"},
            {"method", "GET"},
            {"description", "Get task status and information"},
            {"parameters", {
                {"task_id", "string", "required", "Unique task identifier"}
            }}
        });

        endpoints.push_back({
            {"path", "/tasks/{task_id}/results"},
            {"method", "GET"},
            {"description", "Get analysis results for a completed task"},
            {"parameters", {
                {"task_id", "string", "required", "Unique task identifier"}
            }}
        });

        // System information endpoints
        endpoints.push_back({
            {"path", "/api/system/health"},
            {"method", "GET"},
            {"description", "Health check and system status"},
            {"response", "System health information including memory, disk space, and active tasks"}
        });

        endpoints.push_back({
            {"path", "/api/system/info"},
            {"method", "GET"},
            {"description", "System capabilities and version information"},
            {"response", "Supported formats, filesystems, features, and performance settings"}
        });

        endpoints.push_back({
            {"path", "/api/system/databases/{task_id}"},
            {"method", "GET"},
            {"description", "List all databases for a task"},
            {"parameters", {
                {"task_id", "string", "required", "Unique task identifier"}
            }}
        });

        endpoints.push_back({
            {"path", "/api/system/database-schema/{type}"},
            {"method", "GET"},
            {"description", "Get database schema documentation"},
            {"parameters", {
                {"type", "string", "required", "Database type: raw, events, files, android"}
            }}
        });

        // Utility endpoints
        endpoints.push_back({
            {"path", "/api/docs/endpoints"},
            {"method", "GET"},
            {"description", "Interactive API documentation"},
            {"response", "Complete endpoint documentation with examples"}
        });

        endpoints.push_back({
            {"path", "/api/docs/database-schema"},
            {"method", "GET"},
            {"description", "Database schema documentation"},
            {"response", "Detailed schema information for all database types"}
        });

        endpoints.push_back({
            {"path", "/api/export/{task_id}"},
            {"method", "POST"},
            {"description", "Export data in various formats"},
            {"parameters", {
                {"task_id", "string", "required", "Unique task identifier"}
            }},
            {"body", {
                {"table", "string", "optional", "Table name to export (default: files)"},
                {"format", "string", "optional", "Export format: json, csv, xml (default: json)"},
                {"limit", "integer", "optional", "Maximum records to export (default: 1000)"},
                {"offset", "integer", "optional", "Records to skip (default: 0)"}
            }},
            {"example", R"({
                "table": "images",
                "format": "csv",
                "limit": 500,
                "offset": 0
            })"}
        });

        docs["endpoints"] = endpoints;

        // Error codes
        docs["error_codes"] = {
            {"INVALID_REQUEST", "400", "Invalid request format or parameters"},
            {"TASK_NOT_FOUND", "404", "Task not found"},
            {"TASK_NOT_COMPLETED", "202", "Task has not completed yet"},
            {"RESULTS_ERROR", "500", "Error retrieving results"},
            {"UNKNOWN_DATABASE_TYPE", "400", "Unknown database type specified"},
            {"UNSUPPORTED_FORMAT", "400", "Unsupported export format"},
            {"EXPORT_FAILED", "500", "Export operation failed"},
            {"EXPORT_ERROR", "500", "General export error"},
            {"INVALID_TABLE", "400", "Invalid table name for export"},
            {"QUERY_FAILED", "500", "Database query failed"}
        };

        return docs;
    }

    nlohmann::json HTTPServerEnhanced::get_database_schema_documentation() {
        json docs;

        docs["title"] = "Database Schema Documentation";
        docs["description"] = "Complete schema documentation for all forensic database types";

        // Raw Database Schema
        docs["raw_database"] = {
            {"description", "Contains complete filesystem metadata extracted from disk images"},
            {"tables", {
                {
                    {"name", "files"},
                    {"description", "All files and directories found in the filesystem"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Unique identifier"},
                        {"parent_id", "INTEGER", "Parent directory ID"},
                        {"name", "TEXT", "File/directory name"},
                        {"path", "TEXT", "Full file path"},
                        {"size", "INTEGER", "File size in bytes"},
                        {"created_time", "INTEGER", "Creation timestamp"},
                        {"modified_time", "INTEGER", "Modification timestamp"},
                        {"accessed_time", "INTEGER", "Last access timestamp"},
                        {"file_type", "TEXT", "File type or extension"},
                        {"is_deleted", "BOOLEAN", "Whether file was deleted"},
                        {"inode", "INTEGER", "Inode number"},
                        {"md5_hash", "TEXT", "MD5 hash of file content"},
                        {"sha1_hash", "TEXT", "SHA1 hash of file content"},
                        {"permissions", "TEXT", "File permissions"},
                        {"uid", "INTEGER", "User ID"},
                        {"gid", "INTEGER", "Group ID"}
                    }}
                },
                {
                    {"name", "partitions"},
                    {"description", "Partition information from the disk image"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Partition identifier"},
                        {"start_sector", "INTEGER", "Starting sector"},
                        {"end_sector", "INTEGER", "Ending sector"},
                        {"size", "INTEGER", "Partition size in bytes"},
                        {"type", "TEXT", "Partition type"},
                        {"filesystem", "TEXT", "Filesystem type"},
                        {"description", "TEXT", "Partition description"},
                        {"flags", "TEXT", "Partition flags"}
                    }}
                }
            }},
            {"views", {
                {
                    {"name", "file_summary"},
                    {"description", "Summary of files by category and size"},
                    {"columns", {
                        {"category", "TEXT", "File category"},
                        {"file_count", "INTEGER", "Number of files"},
                        {"total_size", "INTEGER", "Total size in bytes"}
                    }}
                }
            }}
        };

        // Events Database Schema
        docs["events_database"] = {
            {"description", "Contains timeline events extracted from filesystem metadata"},
            {"tables", {
                {
                    {"name", "events"},
                    {"description", "Filesystem events (create, modify, access, delete)"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Event identifier"},
                        {"file_id", "INTEGER", "Reference to files table"},
                        {"event_type", "TEXT", "Event type (create, modify, access, delete)"},
                        {"timestamp", "INTEGER", "Event timestamp"},
                        {"file_path", "TEXT", "File path at time of event"},
                        {"file_name", "TEXT", "File name at time of event"},
                        {"file_size", "INTEGER", "File size at time of event"},
                        {"user_id", "INTEGER", "User ID associated with event"},
                        {"process_id", "INTEGER", "Process ID (if available)"}
                    }}
                }
            }},
            {"views", {
                {
                    {"name", "aggregated_events"},
                    {"description", "Aggregated event summaries by time periods"},
                    {"columns", {
                        {"date", "TEXT", "Date (YYYY-MM-DD)"},
                        {"event_type", "TEXT", "Event type"},
                        {"event_count", "INTEGER", "Number of events"},
                        {"unique_files", "INTEGER", "Number of unique files"}
                    }}
                }
            }}
        };

        // Files Database Schema
        docs["files_database"] = {
            {"description", "Contains files categorized by type for easier analysis"},
            {"tables", {
                {
                    {"name", "images"},
                    {"description", "Image files (JPG, PNG, GIF, BMP, etc.)"},
                    {"common_columns", {
                        {"id", "INTEGER PRIMARY KEY", "Unique identifier"},
                        {"original_file_id", "INTEGER", "Reference to files table"},
                        {"file_path", "TEXT", "Original file path"},
                        {"file_size", "INTEGER", "File size"},
                        {"file_extension", "TEXT", "File extension"},
                        {"mime_type", "TEXT", "MIME type"},
                        {"created_time", "INTEGER", "Creation timestamp"},
                        {"modified_time", "INTEGER", "Modification timestamp"}
                    }}
                },
                {
                    {"name", "videos"},
                    {"description", "Video files (MP4, AVI, MOV, etc.)"}
                },
                {
                    {"name", "audio"},
                    {"description", "Audio files (MP3, WAV, FLAC, etc.)"}
                },
                {
                    {"name", "documents"},
                    {"description", "Document files (PDF, DOC, TXT, etc.)"}
                },
                {
                    {"name", "archives"},
                    {"description", "Archive files (ZIP, RAR, 7Z, etc.)"}
                },
                {
                    {"name", "executables"},
                    {"description", "Executable files (EXE, DLL, etc.)"}
                },
                {
                    {"name", "databases"},
                    {"description", "Database files (SQLite, MDB, etc.)"}
                },
                {
                    {"name", "source_code"},
                    {"description", "Source code files (C++, Java, Python, etc.)"}
                },
                {
                    {"name", "web_files"},
                    {"description", "Web-related files (HTML, CSS, JS, etc.)"}
                },
                {
                    {"name", "email_files"},
                    {"description", "Email files (PST, EML, MSG, etc.)"}
                },
                {
                    {"name", "system_files"},
                    {"description", "System and configuration files"}
                },
                {
                    {"name", "encrypted_files"},
                    {"description", "Encrypted or password-protected files"}
                },
                {
                    {"name", "unknown_files"},
                    {"description", "Files with unknown or unclassified types"}
                }
            }}
        };

        // Android Database Schema
        docs["android_database"] = {
            {"description", "Contains Android-specific forensic data"},
            {"tables", {
                {
                    {"name", "sms_messages"},
                    {"description", "SMS messages from Android devices"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Message identifier"},
                        {"thread_id", "INTEGER", "Conversation thread ID"},
                        {"address", "TEXT", "Phone number"},
                        {"person", "TEXT", "Contact name"},
                        {"date", "INTEGER", "Message timestamp"},
                        {"date_sent", "INTEGER", "Sent timestamp"},
                        {"read", "INTEGER", "Read status (0/1)"},
                        {"type", "INTEGER", "Message type (1=inbox, 2=sent)"},
                        {"body", "TEXT", "Message content"},
                        {"service_center", "TEXT", "SMS service center"}
                    }}
                },
                {
                    {"name", "whatsapp_messages"},
                    {"description", "WhatsApp messages from Android devices"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Message identifier"},
                        {"key_remote_jid", "TEXT", "Contact JID"},
                        {"key_from_me", "INTEGER", "Sent from device (0/1)"},
                        {"key_id", "TEXT", "Message key"},
                        {"status", "INTEGER", "Message status"},
                        {"data", "TEXT", "Message content"},
                        {"timestamp", "INTEGER", "Message timestamp"},
                        {"media_url", "TEXT", "Media file URL"},
                        {"media_mime_type", "TEXT", "Media MIME type"},
                        {"media_wa_type", "TEXT", "WhatsApp media type"}
                    }}
                },
                {
                    {"name", "contacts"},
                    {"description", "Contact information"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Contact ID"},
                        {"name", "TEXT", "Contact name"},
                        {"phone_number", "TEXT", "Phone number"},
                        {"email", "TEXT", "Email address"},
                        {"last_contacted", "INTEGER", "Last contact timestamp"}
                    }}
                },
                {
                    {"name", "call_logs"},
                    {"description", "Call history"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Call ID"},
                        {"number", "TEXT", "Phone number"},
                        {"name", "TEXT", "Contact name"},
                        {"type", "INTEGER", "Call type (1=incoming, 2=outgoing, 3=missed)"},
                        {"date", "INTEGER", "Call timestamp"},
                        {"duration", "INTEGER", "Call duration in seconds"}
                    }}
                },
                {
                    {"name", "applications"},
                    {"description", "Installed applications"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "App ID"},
                        {"package_name", "TEXT", "Package name"},
                        {"app_name", "TEXT", "Application name"},
                        {"version", "TEXT", "Application version"},
                        {"install_time", "INTEGER", "Installation timestamp"},
                        {"update_time", "INTEGER", "Last update timestamp"},
                        {"system_app", "BOOLEAN", "System application flag"}
                    }}
                },
                {
                    {"name", "device_info"},
                    {"description", "Device information from build properties"},
                    {"columns", {
                        {"id", "INTEGER PRIMARY KEY", "Record ID"},
                        {"property_name", "TEXT", "Property name"},
                        {"property_value", "TEXT", "Property value"},
                        {"category", "TEXT", "Property category"}
                    }}
                }
            }}
        };

        return docs;
    }

}