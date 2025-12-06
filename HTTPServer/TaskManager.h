#pragma once
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <filesystem>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "../ImageAnalyzer/ImageAnalyzer.h"
#include "../DatabaseManager/EventExtractor/EventExtractor.h"
#include "../DatabaseManager/FileClassifier/FileClassifier.h"
#include "../AndroidAnalyzer/AndroidAnalyzer.h"

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

    void start_analysis(const std::string& task_id, bool android_analyze, XFSMode xfs_mode, const std::string& db_output_dir = "") {
        std::thread([this, task_id, android_analyze, xfs_mode, db_output_dir]() {
            try {
                AnalysisTask task = get_task(task_id);
                if (task.id.empty()) return;

                std::string imagePath = task.image_path;
                update_status(task_id, TaskStatus::RUNNING, "Initializing analysis...");

                if (!std::filesystem::exists(imagePath)) {
                    update_status(task_id, TaskStatus::FAILED, "Image file not found");
                    return;
                }

                // Generate DB paths
                std::filesystem::path p(imagePath);
                std::string baseName = p.stem().string();
                std::string outPrefix = "";
                if (!db_output_dir.empty()) {
                    std::filesystem::create_directories(db_output_dir);
                    outPrefix = db_output_dir + "/";
                }
                std::string rawDbPath = outPrefix + baseName + "_raw.db";
                std::string eventDbPath = outPrefix + baseName + "_events.db";
                std::string fileDbPath = outPrefix + baseName + "_files.db";

                // 1. Image Analysis
                update_status(task_id, TaskStatus::RUNNING, "Analyzing image structure...");
                auto analyzer = std::make_unique<ImageAnalyzer>(imagePath);
                analyzer->setXFSMode(xfs_mode);

                if (!analyzer->analyze()) {
                    update_status(task_id, TaskStatus::FAILED, "Failed to analyze image");
                    return;
                }

                if (!analyzer->extractToDatabase(rawDbPath)) {
                    update_status(task_id, TaskStatus::FAILED, "Failed to create raw database");
                    return;
                }

                // 2. Event Extraction
                update_status(task_id, TaskStatus::RUNNING, "Extracting events...");
                auto eventExtractor = std::make_unique<EventExtractor>(rawDbPath, eventDbPath);
                if (!eventExtractor->extractEvents()) {
                    // Log but maybe continue?
                    std::cerr << "Warning: Failed to extract events" << std::endl;
                }

                // 3. File Classification
                update_status(task_id, TaskStatus::RUNNING, "Classifying files...");
                auto fileClassifier = std::make_unique<FileClassifier>(rawDbPath, fileDbPath);
                if (!fileClassifier->classifyAndExtract()) {
                    update_status(task_id, TaskStatus::FAILED, "Failed to classify files");
                    return;
                }

                // 4. Android Analysis (Optional)
                if (android_analyze) {
                    update_status(task_id, TaskStatus::RUNNING, "Analyzing Android artifacts...");
                    auto dbManager = std::make_unique<DatabaseManager>(rawDbPath);
                    auto androidAnalyzer = std::make_unique<AndroidAnalyzer>(imagePath, dbManager.get());
                    
                    std::string androidDbPath = outPrefix + baseName + "_android.db";
                    androidAnalyzer->setOutputDatabasePath(androidDbPath);
                    
                    if (androidAnalyzer->initialize()) {
                        androidAnalyzer->analyzeAndroidData();
                    } else {
                         std::cerr << "Warning: Failed to initialize Android analyzer" << std::endl;
                    }
                }

                set_result_db(task_id, fileDbPath);
                update_status(task_id, TaskStatus::COMPLETED, "Analysis completed successfully");

            } catch (const std::exception& e) {
                update_status(task_id, TaskStatus::FAILED, std::string("Analysis error: ") + e.what());
            }
        }).detach();
    }

private:
    TaskManager() = default;
    std::map<std::string, AnalysisTask> tasks_;
    std::mutex mtx_;
};
