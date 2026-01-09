#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <asio.hpp>
#include <curl/curl.h>
#include "../HTTPServer/HTTPserver.h"

using namespace forensics;
using json = nlohmann::json;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Ne;

class HTTPServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start HTTP server in a separate thread
        server_port_ = 18080; // Use different port for testing
        ioc_ = std::make_unique<asio::io_context>();
        server_ = std::make_unique<HTTPServer>(*ioc_);

        // Start server thread
        server_thread_ = std::thread([this]() {
            server_->run(server_port_);
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Initialize curl
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    void TearDown() override {
        // Stop server
        if (ioc_) {
            ioc_->stop();
        }

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        curl_global_cleanup();
    }

    std::string make_http_request(const std::string& method, const std::string& url,
                                 const std::string& body = "", const std::vector<std::string>& headers = {}) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return "";
        }

        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Set method
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        } else if (method == "GET") {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        }

        // Set headers
        struct curl_slist* curl_headers = nullptr;
        for (const auto& header : headers) {
            curl_headers = curl_slist_append(curl_headers, header.c_str());
        }
        if (curl_headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        }

        CURLcode res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK) ? response : "";
    }

    json make_json_request(const std::string& method, const std::string& url,
                          const json& body = json(), const std::vector<std::string>& headers = {}) {
        std::string body_str = body.dump();
        std::vector<std::string> json_headers = headers;
        json_headers.push_back("Content-Type: application/json");

        auto response_str = make_http_request(method, url, body_str, json_headers);
        return response_str.empty() ? json() : json::parse(response_str);
    }

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::unique_ptr<asio::io_context> ioc_;
    std::unique_ptr<HTTPServer> server_;
    std::thread server_thread_;
    int server_port_;
    std::string base_url_;
};

// Test root endpoint
TEST_F(HTTPServerIntegrationTest, RootEndpoint) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].contains("endpoints"));
}

// Test system health endpoint
TEST_F(HTTPServerIntegrationTest, SystemHealth) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/health";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("status"));
    EXPECT_TRUE(data.contains("uptime_seconds"));
    EXPECT_TRUE(data.contains("database_connections"));
    EXPECT_EQ(data["status"], "healthy");
}

// Test system info endpoint
TEST_F(HTTPServerIntegrationTest, SystemInfo) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/info";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("version"));
    EXPECT_TRUE(data.contains("api_version"));
    EXPECT_TRUE(data.contains("supported_formats"));
    EXPECT_TRUE(data.contains("supported_filesystems"));
    EXPECT_TRUE(data.contains("features"));

    // Check supported formats
    auto formats = data["supported_formats"];
    EXPECT_TRUE(formats.is_array());

    // Check features
    auto features = data["features"];
    EXPECT_TRUE(features.contains("timeline_analysis"));
    EXPECT_TRUE(features.contains("file_classification"));
}

// Test API documentation endpoint
TEST_F(HTTPServerIntegrationTest, ApiDocumentation) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/docs/endpoints";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("title"));
    EXPECT_TRUE(data.contains("version"));
    EXPECT_TRUE(data.contains("base_url"));
    EXPECT_TRUE(data.contains("endpoints"));
    EXPECT_TRUE(data.contains("error_codes"));

    // Check endpoint documentation
    auto endpoints = data["endpoints"];
    EXPECT_TRUE(endpoints.is_array());
    EXPECT_GE(endpoints.size(), 8); // Should have at least 8 endpoints documented

    // Check for specific endpoints
    bool found_tasks_endpoint = false;
    bool found_health_endpoint = false;
    bool found_export_endpoint = false;

    for (const auto& endpoint : endpoints) {
        if (endpoint["path"] == "/tasks") {
            found_tasks_endpoint = true;
            EXPECT_EQ(endpoint["method"], "POST");
            EXPECT_TRUE(endpoint.contains("parameters"));
            EXPECT_TRUE(endpoint.contains("example"));
        }
        if (endpoint["path"] == "/api/system/health") {
            found_health_endpoint = true;
            EXPECT_EQ(endpoint["method"], "GET");
        }
        if (endpoint["path"] == "/api/export/{task_id}") {
            found_export_endpoint = true;
            EXPECT_EQ(endpoint["method"], "POST");
            EXPECT_TRUE(endpoint.contains("body"));
        }
    }

    EXPECT_TRUE(found_tasks_endpoint);
    EXPECT_TRUE(found_health_endpoint);
    EXPECT_TRUE(found_export_endpoint);
}

// Test database schema documentation endpoint
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaDocumentation) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/docs/database-schema";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("title"));
    EXPECT_TRUE(data.contains("description"));

    // Check raw database schema
    EXPECT_TRUE(data.contains("raw_database"));
    auto raw_db = data["raw_database"];
    EXPECT_TRUE(raw_db.contains("description"));
    EXPECT_TRUE(raw_db.contains("tables"));
    EXPECT_TRUE(raw_db.contains("views"));

    // Check files table in raw database
    auto raw_tables = raw_db["tables"];
    bool found_files_table = false;
    for (const auto& table : raw_tables) {
        if (table["name"] == "files") {
            found_files_table = true;
            EXPECT_TRUE(table.contains("description"));
            EXPECT_TRUE(table.contains("columns"));
            auto columns = table["columns"];
            EXPECT_TRUE(columns.is_array());
        }
    }
    EXPECT_TRUE(found_files_table);

    // Check other database schemas
    EXPECT_TRUE(data.contains("events_database"));
    EXPECT_TRUE(data.contains("files_database"));
    EXPECT_TRUE(data.contains("android_database"));
}

// Test database schema endpoint - raw database
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaRaw) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/database-schema/raw";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("description"));
    EXPECT_TRUE(data.contains("tables"));

    // Check for specific tables
    auto tables = data["tables"];
    bool found_files = false;
    bool found_partitions = false;

    for (const auto& table : tables) {
        std::string table_name = table["name"];
        if (table_name == "files") found_files = true;
        if (table_name == "partitions") found_partitions = true;
    }

    EXPECT_TRUE(found_files);
    EXPECT_TRUE(found_partitions);
}

// Test database schema endpoint - events database
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaEvents) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/database-schema/events";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("description"));
    EXPECT_TRUE(data.contains("tables"));

    // Check for events table
    auto tables = data["tables"];
    bool found_events = false;
    for (const auto& table : tables) {
        std::string table_name = table["name"];
        if (table_name == "events") found_events = true;
    }

    EXPECT_TRUE(found_events);
}

// Test database schema endpoint - files database
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaFiles) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/database-schema/files";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("description"));
    EXPECT_TRUE(data.contains("tables"));

    // Should mention all file category tables
    EXPECT_TRUE(data.dump().find("images") != std::string::npos);
    EXPECT_TRUE(data.dump().find("videos") != std::string::npos);
    EXPECT_TRUE(data.dump().find("documents") != std::string::npos);
}

// Test database schema endpoint - android database
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaAndroid) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/database-schema/android";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_TRUE(response.contains("data"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("description"));
    EXPECT_TRUE(data.contains("tables"));

    // Check for Android-specific tables
    auto tables = data["tables"];
    bool found_sms = false;
    bool found_whatsapp = false;
    bool found_contacts = false;

    for (const auto& table : tables) {
        std::string table_name = table["name"];
        if (table_name == "sms_messages") found_sms = true;
        if (table_name == "whatsapp_messages") found_whatsapp = true;
        if (table_name == "contacts") found_contacts = true;
    }

    EXPECT_TRUE(found_sms);
    EXPECT_TRUE(found_whatsapp);
    EXPECT_TRUE(found_contacts);
}

// Test database schema endpoint - invalid type
TEST_F(HTTPServerIntegrationTest, DatabaseSchemaInvalid) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/database-schema/invalid";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_FALSE(response["success"]);
    EXPECT_EQ(response["error_code"], "UNKNOWN_DATABASE_TYPE");
}

// Test task creation with enhanced response format
TEST_F(HTTPServerIntegrationTest, CreateTaskEnhanced) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/tasks";

    json task_request = {
        {"image_path", "/nonexistent/test.e01"},  // Use non-existent path for testing
        {"android_analyze", true},
        {"xfs_mode", "auto"}
    };

    auto response = make_json_request("POST", url, task_request);

    ASSERT_FALSE(response.empty());
    EXPECT_TRUE(response["success"]);
    EXPECT_EQ(response["message"], "Task created successfully");
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response.contains("timestamp"));

    auto data = response["data"];
    EXPECT_TRUE(data.contains("task_id"));
    EXPECT_TRUE(data.contains("image_path"));
    EXPECT_TRUE(data.contains("android_analyze"));
    EXPECT_TRUE(data.contains("xfs_mode"));

    EXPECT_EQ(data["image_path"], "/nonexistent/test.e01");
    EXPECT_TRUE(data["android_analyze"]);
    EXPECT_EQ(data["xfs_mode"], "auto");
}

// Test task status with enhanced response format
TEST_F(HTTPServerIntegrationTest, GetTaskEnhanced) {
    // First create a task
    std::string create_url = "http://localhost:" + std::to_string(server_port_) + "/tasks";
    json task_request = {{"image_path", "/nonexistent/test2.e01"}};
    auto create_response = make_json_request("POST", create_url, task_request);

    ASSERT_TRUE(create_response["success"]);
    std::string task_id = create_response["data"]["task_id"];

    // Now get task status
    std::string get_url = "http://localhost:" + std::to_string(server_port_) + "/tasks/" + task_id;
    auto get_response = make_json_request("GET", get_url);

    ASSERT_FALSE(get_response.empty());
    EXPECT_TRUE(get_response["success"]);
    EXPECT_EQ(get_response["message"], "Task retrieved successfully");
    EXPECT_TRUE(get_response.contains("data"));
    EXPECT_TRUE(get_response.contains("timestamp"));

    auto data = get_response["data"];
    EXPECT_EQ(data["id"], task_id);
    EXPECT_EQ(data["image_path"], "/nonexistent/test2.e01");
    EXPECT_TRUE(data.contains("status"));
    EXPECT_TRUE(data.contains("message"));
}

// Test task not found error
TEST_F(HTTPServerIntegrationTest, TaskNotFound) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/tasks/nonexistent-task-id";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_FALSE(response["success"]);
    EXPECT_EQ(response["error_code"], "TASK_NOT_FOUND");
    EXPECT_TRUE(response["message"].find("Task not found") != std::string::npos);
}

// Test invalid task creation request
TEST_F(HTTPServerIntegrationTest, InvalidTaskCreation) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/tasks";

    // Send invalid JSON
    json invalid_request = {{"invalid_field", "value"}};
    auto response = make_json_request("POST", url, invalid_request);

    ASSERT_FALSE(response.empty());
    EXPECT_FALSE(response["success"]);
    EXPECT_TRUE(response.contains("error_code"));
}

// Test task databases endpoint for non-existent task
TEST_F(HTTPServerIntegrationTest, TaskDatabasesNotFound) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/databases/nonexistent-task";
    auto response = make_json_request("GET", url);

    ASSERT_FALSE(response.empty());
    EXPECT_FALSE(response["success"]);
    EXPECT_EQ(response["error_code"], "TASK_NOT_FOUND");
}

// Test export endpoint for non-existent task
TEST_F(HTTPServerIntegrationTest, ExportTaskNotFound) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/export/nonexistent-task";

    json export_request = {
        {"table", "files"},
        {"format", "json"},
        {"limit", 100}
    };

    auto response = make_json_request("POST", url, export_request);

    ASSERT_FALSE(response.empty());
    EXPECT_FALSE(response["success"]);
    EXPECT_EQ(response["error_code"], "TASK_NOT_FOUND");
}

// Test export endpoint with invalid format
TEST_F(HTTPServerIntegrationTest, ExportInvalidFormat) {
    // First create a task (it will likely fail since we use non-existent path, but that's fine for testing)
    std::string create_url = "http://localhost:" + std::to_string(server_port_) + "/tasks";
    json task_request = {{"image_path", "/nonexistent/export-test.e01"}};
    auto create_response = make_json_request("POST", create_url, task_request);

    if (create_response["success"]) {
        std::string task_id = create_response["data"]["task_id"];
        std::string export_url = "http://localhost:" + std::to_string(server_port_) + "/api/export/" + task_id;

        json export_request = {
            {"table", "files"},
            {"format", "invalid_format"},
            {"limit", 100}
        };

        auto export_response = make_json_request("POST", export_url, export_request);

        ASSERT_FALSE(export_response.empty());
        // Should get either task not completed or other error, but response should be properly formatted
        EXPECT_TRUE(export_response.contains("success"));
        EXPECT_TRUE(export_response.contains("timestamp"));
    }
}

// Test response format consistency
TEST_F(HTTPServerIntegrationTest, ResponseFormatConsistency) {
    // Test that all endpoints return consistent response format
    std::vector<std::string> endpoints = {
        "/api/system/health",
        "/api/system/info",
        "/api/docs/endpoints",
        "/api/docs/database-schema",
        "/api/system/database-schema/raw",
        "/api/system/database-schema/events",
        "/api/system/database-schema/files",
        "/api/system/database-schema/android"
    };

    for (const auto& endpoint : endpoints) {
        std::string url = "http://localhost:" + std::to_string(server_port_) + endpoint;
        auto response = make_json_request("GET", url);

        ASSERT_FALSE(response.empty());
        // All responses should have these fields
        EXPECT_TRUE(response.contains("success"));
        EXPECT_TRUE(response.contains("timestamp"));

        if (response["success"]) {
            EXPECT_TRUE(response.contains("message"));
            EXPECT_TRUE(response.contains("data"));
        } else {
            EXPECT_TRUE(response.contains("message"));
            EXPECT_TRUE(response.contains("error_code"));
        }
    }
}

// Test concurrent requests
TEST_F(HTTPServerIntegrationTest, ConcurrentRequests) {
    const int num_requests = 10;
    std::vector<std::thread> threads;
    std::vector<json> responses(num_requests);
    std::vector<bool> success_flags(num_requests);

    // Make concurrent requests to health endpoint
    for (int i = 0; i < num_requests; ++i) {
        threads.emplace_back([this, &responses, &success_flags, i]() {
            std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/health";
            auto response = make_json_request("GET", url);
            responses[i] = response;
            success_flags[i] = !response.empty() && response["success"];
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All requests should succeed
    for (bool success : success_flags) {
        EXPECT_TRUE(success);
    }

    // All responses should have consistent structure
    for (const auto& response : responses) {
        EXPECT_TRUE(response.contains("success"));
        EXPECT_TRUE(response.contains("timestamp"));
        EXPECT_TRUE(response["success"]);
    }
}

// Test error handling with malformed requests
TEST_F(HTTPServerIntegrationTest, ErrorHandling) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/tasks";

    // Send malformed JSON
    std::string malformed_json = "{invalid json}";
    auto response = make_http_request("POST", url, malformed_json, {"Content-Type: application/json"});

    // Should get a proper error response or empty response for malformed JSON
    EXPECT_TRUE(response.empty() || response.find("error") != std::string::npos);
}

// Test CORS-like behavior and headers
TEST_F(HTTPServerIntegrationTest, ResponseHeaders) {
    std::string url = "http://localhost:" + std::to_string(server_port_) + "/api/system/health";

    CURL* curl = curl_easy_init();
    ASSERT_NE(curl, nullptr);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request

    struct curl_slist* headers = nullptr;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, [](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
        std::string* headers_str = static_cast<std::string*>(userdata);
        headers_str->append(buffer, size * nitems);
        return size * nitems;
    });
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    CURLcode res = curl_easy_perform(curl);

    EXPECT_EQ(res, CURLE_OK);
    EXPECT_FALSE(headers_str.empty());

    // Check that Content-Type is set properly
    EXPECT_TRUE(headers_str.find("Content-Type") != std::string::npos);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}