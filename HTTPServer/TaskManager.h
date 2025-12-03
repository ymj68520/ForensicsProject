#pragma once
#include <string>
#include <map>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

enum class TaskStatus { PENDING, RUNNING, COMPLETED, FAILED };

struct AnalysisTask{
    std::string id;
    std::string image_path;
    TaskStatus status;
    std::string message;
    std::string output_files_db;
};

class TaskManager {
public:
    static TaskManager& instance() {
        static TaskManager instance;
        return instance;
    }

    std::string create_task(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string id = boost::uuids::to_string(uuid);
        
        tasks_[id] = {id, path, TaskStatus::PENDING, "Waiting to start", ""};
        return id;
    }

    void update_status(const std::string& id, TaskStatus status, const std::string& msg = "") {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tasks_.count(id)) {
            tasks_[id].status = status;
            if (!msg.empty()) tasks_[id].message = msg;
        }
    }

    void set_result_db(const std::string& id, const std::string& db_path) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tasks_.count(id)) {
            tasks_[id].output_files_db = db_path;
        }
    }

    AnalysisTask get_task(const std::string& id) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tasks_.count(id)) return tasks_[id];
        return {"", "", TaskStatus::FAILED, "Task not found", ""};
    }

private:
    TaskManager() = default;
    std::map<std::string, AnalysisTask> tasks_;
    std::mutex mtx_;
};
