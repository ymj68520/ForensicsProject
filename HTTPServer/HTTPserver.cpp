#include "HTTPserver.h"
#include <iostream>
/*
 * 一个合法的创建请求如下：
 * curl -X POST http://localhost:8080/tasks 
 * -H "Content-Type: application/json" 
 * -d '{"image_path": "/home/ymj68520/ForensicsProject/build/David_USB_8GB.e01"}'
 *   响应：
 * {"status":"created","task_id":"c95065c7-624e-4fc7-83d8-9bee17f556e3"}
 * 一个合法的查询任务状态请求如下：
 * curl -X GET http://localhost:8080/tasks/c95065c7-624e-4fc7-83d8-9bee17f556e3
 *   响应：
 * {
  "id": "c95065c7-624e-4fc7-83d8-9bee17f556e3",
  "image_path": "/home/ymj68520/ForensicsProject/build/David_USB_8GB.e01",
  "message": "Waiting to start",
  "status": "pending"
    }
 * 一个合法的结果查询请求如下：
 * curl -X GET http://localhost:8080/tasks/c95065c7-624e-4fc7-83d8-9bee17f556e3/results
 *   响应：
 *  "Task not completed yet"
*/
namespace forensics {

    HTTPServer::HTTPServer(asio::io_context& ioc) : task_manager_(TaskManager::instance()), ioc_(ioc) {
        // 定义路由
        CROW_ROUTE(app_, "/tasks").methods("POST"_method)([this](const crow::request& req) {
            return handle_create_task(req);
        });

        CROW_ROUTE(app_, "/tasks/<string>").methods("GET"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_get_task(req, task_id);
        });

        CROW_ROUTE(app_, "/tasks/<string>/results").methods("GET"_method)([this](const crow::request& req, const std::string& task_id) {
            return handle_get_task_results(req, task_id);
        });
    }

    void HTTPServer::run(int port) {
        std::cout << "Starting HTTP server on port " << port << std::endl;
        app_.port(port).multithreaded().run();
    }

    crow::response HTTPServer::handle_create_task(const crow::request& req) {
        crow::response res;
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

            json response = {
                {"task_id", task_id},
                {"status", "created"}
            };

            res.code = 201;
            res.set_header("Content-Type", "application/json");
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 400;
            res.write("Invalid request: " + std::string(e.what()));
        }
        return res;
    }

    crow::response HTTPServer::handle_get_task(const crow::request& req, const std::string& task_id) {
        crow::response res;
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            res.code = 404;
            res.write("Task not found");
            return res;
        }

        json response = {
            {"id", task.id},
            {"image_path", task.image_path},
            {"status", task.status == TaskStatus::PENDING ? "pending" :
                     task.status == TaskStatus::RUNNING ? "running" :
                     task.status == TaskStatus::COMPLETED ? "completed" : "failed"},
            {"message", task.message}
        };

        res.set_header("Content-Type", "application/json");
        res.write(response.dump());
        return res;
    }

    crow::response HTTPServer::handle_get_task_results(const crow::request& req, const std::string& task_id) {
        crow::response res;
        AnalysisTask task = task_manager_.get_task(task_id);

        if (task.id.empty()) {
            res.code = 404;
            res.write("Task not found");
            return res;
        }

        if (task.status != TaskStatus::COMPLETED) {
            res.code = 202;
            res.write("Task not completed yet");
            return res;
        }

        try {
            auto summary = SQLiteHelper::get_file_summary(task.output_files_db);
            res.set_header("Content-Type", "application/json");
            res.write(summary.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write("Error retrieving results: " + std::string(e.what()));
        }
        return res;
    }

}