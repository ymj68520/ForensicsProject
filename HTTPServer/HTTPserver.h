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

        // 处理函数
        crow::response handle_create_task(const crow::request& req);
        crow::response handle_get_task(const crow::request& req, const std::string& task_id);
        crow::response handle_get_task_results(const crow::request& req, const std::string& task_id);
    };
}