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
#include "TaskManager.h"
#include "SQLiteHelper.h"
#include "Utils.h"

namespace forensics {
    using json = nlohmann::json;

    class HTTPServer {
    public:
        HTTPServer(asio::io_context& ioc);
        ~HTTPServer() = default;

        void run(int port = 8080);

    private:
        crow::App<> app_;
        TaskManager& task_manager_;
        asio::io_context& ioc_;

        // Basic task management
        crow::response handle_create_task(const crow::request& req);
        crow::response handle_get_task(const crow::request& req, const std::string& task_id);
        crow::response handle_get_task_results(const crow::request& req, const std::string& task_id);

        // Enhanced task management endpoints
        crow::response handle_list_tasks(const crow::request& req);
        crow::response handle_cancel_task(const crow::request& req, const std::string& task_id);
        crow::response handle_get_task_progress(const crow::request& req, const std::string& task_id);
        crow::response handle_get_task_statistics(const crow::request& req);
        crow::response handle_cleanup_tasks(const crow::request& req);

        // Batch operations
        crow::response handle_batch_create_tasks(const crow::request& req);
        crow::response handle_batch_status(const crow::request& req);
        crow::response handle_batch_cancel(const crow::request& req);

        // Advanced task features
        crow::response handle_get_task_audit_log(const crow::request& req, const std::string& task_id);
        crow::response handle_update_task_priority(const crow::request& req, const std::string& task_id);

        // Timeline Analysis Endpoints
        crow::response handle_timeline_comprehensive(const crow::request& req);
        crow::response handle_timeline_file_activity(const crow::request& req);
        crow::response handle_timeline_suspicious_patterns(const crow::request& req);
        crow::response handle_timeline_user_activity(const crow::request& req);

        // File Analysis Endpoints
        crow::response handle_files_largest(const crow::request& req);
        crow::response handle_files_recent(const crow::request& req);
        crow::response handle_files_suspicious(const crow::request& req);
        crow::response handle_files_duplicates(const crow::request& req);
        crow::response handle_files_extensions_analysis(const crow::request& req);

        // Android Forensics Specialized Endpoints
        crow::response handle_android_communication_summary(const crow::request& req);
        crow::response handle_android_app_usage(const crow::request& req);
        crow::response handle_android_device_info(const crow::request& req);
        crow::response handle_android_media_analysis(const crow::request& req);

        // Statistical Analysis Endpoints
        crow::response handle_statistics_overview(const crow::request& req);
        crow::response handle_statistics_file_distribution(const crow::request& req);
        crow::response handle_statistics_activity_patterns(const crow::request& req);
        crow::response handle_statistics_deleted_files_analysis(const crow::request& req);

        // Raw Database Query Endpoints
        crow::response handle_raw_files(const crow::request& req);
        crow::response handle_raw_partitions(const crow::request& req);
        crow::response handle_raw_search(const crow::request& req);

        // Events Database Query Endpoints
        crow::response handle_events_timeline(const crow::request& req);
        crow::response handle_events_statistics(const crow::request& req);
        crow::response handle_events_hourly_activity(const crow::request& req);

        // Files Database Query Endpoints
        crow::response handle_files_category(const crow::request& req, const std::string& category);
        crow::response handle_files_deleted(const crow::request& req);
        crow::response handle_files_extensions(const crow::request& req);

        // Android Database Query Endpoints
        crow::response handle_android_sms(const crow::request& req);
        crow::response handle_android_contacts(const crow::request& req);
        crow::response handle_android_call_logs(const crow::request& req);

        // Advanced Query Endpoint
        crow::response handle_advanced_query(const crow::request& req);

        // System Information and Monitoring Endpoints
        crow::response handle_system_health(const crow::request& req);
        crow::response handle_system_info(const crow::request& req);
        crow::response handle_system_databases(const crow::request& req);
        crow::response handle_system_database_schema(const crow::request& req, const std::string& db_type);

        // Utility Endpoints
        crow::response handle_docs_endpoints(const crow::request& req);
        crow::response handle_docs_database_schema(const crow::request& req);
        crow::response handle_export_results(const crow::request& req, const std::string& task_id);

        // Helper methods
        json task_to_json(const AnalysisTask& task);
        TaskPriority priority_from_string(const std::string& priority_str);
        std::string priority_to_string(TaskPriority priority);
        std::string phase_to_string(TaskPhase phase);
        std::string status_to_string(TaskStatus status);

        // Database helper methods
        std::string get_database_path(const std::string& task_id, const std::string& db_type);
    };
}