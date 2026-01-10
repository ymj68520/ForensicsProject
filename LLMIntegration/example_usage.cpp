/**
 * @file example_usage.cpp
 * @brief LLM集成模块使用示例
 * 
 * 本文件展示了如何使用大模型集成模块进行各种取证分析任务
 */

#include "LLMIntegration/ModelManager.h"
#include "LLMIntegration/AIAnalysisService.h"
#include "LLMIntegration/LLMHttpServer.h"
#include "LLMIntegration/TaskClassifier.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace forensics::llm;

void demonstrateTaskClassification() {
    std::cout << "\n=== 任务分类演示 ===" << std::endl;
    
    // 创建任务分类器
    TaskClassifier classifier;
    
    // 示例任务
    std::vector<AITask> tasks = {
        [](){
            AITask task;
            task.id = "task_1";
            task.input_text = "请分析以下聊天记录，识别可疑行为模式";
            task.system_prompt = "你是数字取证专家";
            return task;
        }(),
        [](){
            AITask task;
            task.id = "task_2";
            task.input_text = "请对这张图片中的物体进行识别";
            task.input_image_path = "/path/to/evidence.jpg";
            return task;
        }(),
        [](){
            AITask task;
            task.id = "task_3";
            task.input_text = "请生成一份详细的取证报告";
            task.system_prompt = "你是资深取证报告专家";
            return task;
        }()
    };
    
    for (const auto& task : tasks) {
        TaskClassification classification = classifier.classifyTask(task);
        
        std::cout << "任务ID: " << task.id << std::endl;
        std::cout << "主要类型: " << TaskClassifier::taskTypeToString(classification.primary_type) << std::endl;
        std::cout << "复杂度: " << TaskClassifier::complexityToString(classification.complexity) << std::endl;
        std::cout << "置信度: " << std::fixed << std::setprecision(2) << classification.confidence << std::endl;
        std::cout << "推理过程: " << classification.reasoning << std::endl;
        std::cout << "---" << std::endl;
    }
}

void demonstrateModelManager() {
    std::cout << "\n=== 模型管理器演示 ===" << std::endl;
    
    // 创建模型管理器（确保LM Studio在localhost:1234运行）
    auto model_manager = std::make_shared<ModelManager>("http://localhost:1234");
    
    if (!model_manager->initialize()) {
        std::cout << "模型管理器初始化失败，请确保LM Studio正在运行" << std::endl;
        return;
    }
    
    // 获取可用模型
    auto models = model_manager->getAvailableModels();
    std::cout << "可用模型数量: " << models.size() << std::endl;
    
    for (const auto& model : models) {
        std::cout << "- " << model.name << " (" << model.model_id << ")" << std::endl;
        std::cout << "  最大tokens: " << model.max_tokens << std::endl;
        std::cout << "  上下文长度: " << model.context_length << std::endl;
        std::cout << "  多模态: " << (model.is_multimodal ? "是" : "否") << std::endl;
    }
    
    // 获取系统状态
    auto status = model_manager->getSystemStatus();
    std::cout << "\n系统状态:" << std::endl;
    std::cout << "连接状态: " << (status.is_connected ? "已连接" : "未连接") << std::endl;
    std::cout << "当前模型: " << status.current_model << std::endl;
    std::cout << "活跃任务: " << status.active_tasks << std::endl;
}

void demonstrateAIAnalysisService() {
    std::cout << "\n=== AI分析服务演示 ===" << std::endl;
    
    auto model_manager = std::make_shared<ModelManager>("http://localhost:1234");
    if (!model_manager->initialize()) {
        std::cout << "模型管理器初始化失败" << std::endl;
        return;
    }
    
    auto analysis_service = std::make_shared<AIAnalysisService>(model_manager);
    if (!analysis_service->initialize()) {
        std::cout << "AI分析服务初始化失败" << std::endl;
        return;
    }
    
    // 证据分析示例
    std::cout << "\n1. 证据分析示例:" << std::endl;
    std::string evidence = "系统日志显示用户admin在2024年1月15日凌晨2:30分从IP地址192.168.1.100登录系统，随后在2:35分访问了敏感文件/confidential/customer_data.db，并在2:42分通过SSH连接到外部服务器203.0.113.10，传输了约2.3GB的数据。";
    
    std::map<std::string, std::string> metadata = {
        {"type", "log"},
        {"source", "system_log"},
        {"timestamp", "2024-01-15T02:30:00Z"}
    };
    
    auto evidence_result = analysis_service->analyzeEvidence(evidence, metadata);
    
    std::cout << "证据ID: " << evidence_result.evidence_id << std::endl;
    std::cout << "内容摘要: " << evidence_result.content_summary << std::endl;
    std::cout << "相关性分数: " << evidence_result.relevance_score << std::endl;
    std::cout << "可疑度分数: " << evidence_result.suspicious_score << std::endl;
    std::cout << "置信级别: " << evidence_result.confidence_level << std::endl;
    
    if (!evidence_result.key_findings.empty()) {
        std::cout << "关键发现:" << std::endl;
        for (const auto& finding : evidence_result.key_findings) {
            std::cout << "  - " << finding << std::endl;
        }
    }
    
    // 情感分析示例
    std::cout << "\n2. 情感分析示例:" << std::endl;
    std::string text = "我对这次系统入侵事件感到非常愤怒，这严重影响了我们的业务运营，必须尽快找到解决方案。";
    
    auto sentiment_scores = analysis_service->analyzeSentiment(text);
    std::cout << "文本: " << text << std::endl;
    for (const auto& [emotion, score] : sentiment_scores) {
        std::cout << emotion << ": " << std::fixed << std::setprecision(2) << score << std::endl;
    }
    
    // 实体识别示例
    std::cout << "\n3. 实体识别示例:" << std::endl;
    std::string entity_text = "张三在2024年1月15日从北京发送邮件到zhangsan@company.com，电话号码是13812345678，联系地址是北京市朝阳区建国路88号。";
    
    auto entities = analysis_service->recognizeEntities(entity_text);
    std::cout << "文本: " << entity_text << std::endl;
    std::cout << "识别的实体:" << std::endl;
    for (const auto& entity : entities) {
        std::cout << "  文本: " << entity.at("text") 
                  << ", 类型: " << entity.at("type")
                  << ", 置信度: " << entity.at("confidence") << std::endl;
    }
}

void demonstrateHttpServer() {
    std::cout << "\n=== HTTP服务器演示 ===" << std::endl;
    
    auto model_manager = std::make_shared<ModelManager>("http://localhost:1234");
    auto analysis_service = std::make_shared<AIAnalysisService>(model_manager);
    
    // 创建HTTP服务器
    LLMHttpServer http_server("http://localhost:1234", 8080);
    http_server.setModelManager(model_manager);
    http_server.setAnalysisService(analysis_service);
    
    // 启用CORS
    http_server.enableCORS(true, {"*"});
    
    if (!http_server.initialize()) {
        std::cout << "HTTP服务器初始化失败" << std::endl;
        return;
    }
    
    std::cout << "HTTP服务器初始化成功" << std::endl;
    
    // 获取服务器状态
    auto status = http_server.getServerStatus();
    std::cout << "服务器端口: " << status["port"] << std::endl;
    std::cout << "API前缀: " << status["api_prefix"] << std::endl;
    std::cout << "CORS状态: " << (status["cors_enabled"] ? "启用" : "禁用") << std::endl;
    
    // 启动服务器（在后台线程中运行）
    std::thread server_thread([&http_server]() {
        if (http_server.start()) {
            std::cout << "HTTP服务器启动成功，监听端口8080" << std::endl;
            std::cout << "API端点示例:" << std::endl;
            std::cout << "  POST http://localhost:8080/api/llm/analyze/evidence" << std::endl;
            std::cout << "  POST http://localhost:8080/api/llm/analyze/sentiment" << std::endl;
            std::cout << "  GET  http://localhost:8080/api/llm/models" << std::endl;
            std::cout << "  GET  http://localhost:8080/api/llm/status" << std::endl;
            
            // 保持服务器运行
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        } else {
            std::cout << "HTTP服务器启动失败" << std::endl;
        }
    });
    
    // 等待一段时间让服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n服务器已在后台启动，按Enter键停止..." << std::endl;
    std::cin.get();
    
    http_server.stop();
    server_thread.join();
}

void demonstrateAsyncAnalysis() {
    std::cout << "\n=== 异步分析演示 ===" << std::endl;
    
    auto model_manager = std::make_shared<ModelManager>("http://localhost:1234");
    auto analysis_service = std::make_shared<AIAnalysisService>(model_manager);
    
    if (!analysis_service->initialize()) {
        std::cout << "AI分析服务初始化失败" << std::endl;
        return;
    }
    
    // 异步证据分析
    std::vector<std::string> evidences = {
        "用户在凌晨3点登录系统，访问了多个敏感文件",
        "检测到异常的网络连接，数据传输量异常",
        "系统进程列表中发现可疑的后台进程"
    };
    
    std::cout << "开始异步分析 " << evidences.size() << " 个证据..." << std::endl;
    
    int completed_count = 0;
    std::mutex count_mutex;
    
    for (size_t i = 0; i < evidences.size(); ++i) {
        analysis_service->analyzeEvidenceAsync(evidences[i], 
            [&completed_count, &count_mutex, i, evidences](const EvidenceAnalysisResult& result) {
                std::lock_guard<std::mutex> lock(count_mutex);
                completed_count++;
                
                std::cout << "证据 " << (i + 1) << " 分析完成:" << std::endl;
                std::cout << "  ID: " << result.evidence_id << std::endl;
                std::cout << "  摘要: " << result.content_summary << std::endl;
                std::cout << "  可疑度: " << result.suspicious_score << std::endl;
                std::cout << "  进度: " << completed_count << "/" << evidences.size() << std::endl;
                std::cout << "---" << std::endl;
            });
    }
    
    // 等待所有任务完成
    while (completed_count < static_cast<int>(evidences.size())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "所有异步分析任务已完成" << std::endl;
}

void printUsage() {
    std::cout << "LLM集成模块使用示例" << std::endl;
    std::cout << "=========================" << std::endl;
    std::cout << "请选择要演示的功能:" << std::endl;
    std::cout << "1. 任务分类演示" << std::endl;
    std::cout << "2. 模型管理器演示" << std::endl;
    std::cout << "3. AI分析服务演示" << std::endl;
    std::cout << "4. HTTP服务器演示" << std::endl;
    std::cout << "5. 异步分析演示" << std::endl;
    std::cout << "0. 退出" << std::endl;
    std::cout << "请输入选项: ";
}

int main() {
    std::cout << "=== 数字取证工具 - 大模型集成演示 ===" << std::endl;
    std::cout << "注意：请确保LM Studio在http://localhost:1234运行" << std::endl;
    
    int choice;
    do {
        printUsage();
        std::cin >> choice;
        
        // 清除输入缓冲区
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        switch (choice) {
            case 1:
                demonstrateTaskClassification();
                break;
            case 2:
                demonstrateModelManager();
                break;
            case 3:
                demonstrateAIAnalysisService();
                break;
            case 4:
                demonstrateHttpServer();
                break;
            case 5:
                demonstrateAsyncAnalysis();
                break;
            case 0:
                std::cout << "退出演示" << std::endl;
                break;
            default:
                std::cout << "无效选项，请重新选择" << std::endl;
                break;
        }
        
        if (choice != 0) {
            std::cout << "\n按Enter键继续..." << std::endl;
            std::cin.get();
        }
        
    } while (choice != 0);
    
    return 0;
}