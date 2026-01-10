#include "ModelManager.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace forensics {
namespace llm {

ModelManager::ModelManager(const std::string& lm_studio_url)
    : client_(std::make_unique<LMStudioClient>(lm_studio_url))
    , classifier_(std::make_unique<TaskClassifier>())
    , intelligent_classifier_(std::make_unique<IntelligentTaskClassifier>(lm_studio_url))
    , auto_model_selection_(true)
    , selection_strategy_("performance_based")
    , max_concurrent_tasks_(5)
    , current_task_count_(0)
    , is_shutdown_(false) {
    
    stats_.start_time = std::chrono::system_clock::now();
}

ModelManager::~ModelManager() {
    shutdown();
}

bool ModelManager::initialize() {
    try {
        // 初始化LM Studio客户端
        if (!client_->initialize()) {
            return false;
        }

        // 初始化智能分类器
        if (!intelligent_classifier_->initialize()) {
            // 智能分类器初始化失败不影响基本功能
        }

        // 启动工作线程
        worker_thread_ = std::thread(&ModelManager::workerThreadFunc, this);

        // 扫描可用模型
        int loaded_models = scanAndLoadModels();
        
        // 初始化默认模型配置
        initializeDefaultModels();

        return loaded_models > 0;
    }
    catch (const std::exception& e) {
        return false;
    }
}

void ModelManager::shutdown() {
    if (is_shutdown_) {
        return;
    }

    is_shutdown_ = true;
    queue_cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    client_->shutdown();
}

int ModelManager::scanAndLoadModels() {
    try {
        std::vector<ModelConfig> models = client_->getAvailableModels();
        int loaded_count = 0;

        for (const auto& model : models) {
            if (validateModelConfig(model)) {
                available_models_[model.model_id] = model;
                
                // 初始化性能统计
                ModelPerformance perf;
                perf.model_id = model.model_id;
                perf.avg_response_time = 0.0;
                perf.success_rate = 1.0;
                perf.avg_confidence = 0.8;
                perf.total_requests = 0;
                perf.last_used = std::chrono::system_clock::now();
                perf.memory_usage = model.memory_usage;
                perf.error_count = 0;
                
                model_performance_[model.model_id] = perf;
                loaded_count++;
            }
        }

        // 如果有可用模型，加载第一个作为默认模型
        if (!models.empty()) {
            current_model_ = models[0].model_id;
            client_->loadModel(current_model_);
        }

        return loaded_count;
    }
    catch (const std::exception& e) {
        return 0;
    }
}

std::pair<ModelConfig, AIResponse> ModelManager::processTask(const AITask& task) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 1. 分类任务
        TaskClassification classification = classifier_->classifyTask(task);
        
        // 2. 如果分类置信度低，使用智能分类器
        if (classification.confidence < 0.7 && intelligent_classifier_->isModelAvailable()) {
            TaskClassification intelligent_classification = intelligent_classifier_->classifyWithModel(task);
            if (intelligent_classification.confidence > classification.confidence) {
                classification = intelligent_classification;
            }
        }

        // 3. 选择最佳模型
        ModelConfig selected_model = selectBestModel(task, classification);
        
        // 4. 加载模型（如果需要）
        if (selected_model.model_id != current_model_) {
            if (client_->loadModel(selected_model.model_id)) {
                current_model_ = selected_model.model_id;
            } else {
                // 模型加载失败，使用故障转移
                selected_model = performFailover(selected_model.model_id, task);
            }
        }

        // 5. 构建提示词
        auto [system_prompt, user_prompt] = buildTaskPrompts(task, classification);

        // 6. 调用模型
        auto api_response = client_->chatCompletion(
            user_prompt,
            system_prompt,
            selected_model.model_id,
            task.temperature,
            task.max_tokens
        );

        // 7. 解析响应
        AIResponse ai_response = parseModelResponse(task, selected_model, 
                                                api_response.data.dump());

        // 8. 更新性能统计
        auto end_time = std::chrono::high_resolution_clock::now();
        double processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count() / 1000.0;
        
        updateModelPerformance(selected_model.model_id, api_response.is_success(), 
                          processing_time, ai_response.confidence);
        updateGlobalStatistics(task, ai_response, processing_time);

        return {selected_model, ai_response};
    }
    catch (const std::exception& e) {
        AIResponse error_response;
        error_response.task_id = task.id;
        error_response.error_message = e.what();
        error_response.confidence = 0.0;
        error_response.processing_time = 0.0;
        error_response.timestamp = std::chrono::system_clock::now();

        return {ModelConfig{}, error_response};
    }
}

void ModelManager::processTaskAsync(const AITask& task, 
                                   std::function<void(const AIResponse&)> callback) {
    if (!is_shutdown_) {
        QueuedTask queued_task;
        queued_task.task = task;
        queued_task.callback = [callback, this](const AIResponse& response) {
            callback(response);
        };
        queued_task.retry_count = 0;
        queued_task.next_attempt = std::chrono::system_clock::now();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(queued_task);
        }
        queue_cv_.notify_one();
    }
}

std::vector<AIResponse> ModelManager::processBatch(const std::vector<AITask>& tasks) {
    std::vector<AIResponse> responses;
    responses.reserve(tasks.size());

    // 并行处理（限制并发数）
    std::vector<std::thread> threads;
    std::mutex response_mutex;
    size_t completed_count = 0;

    for (const auto& task : tasks) {
        threads.emplace_back([this, &task, &responses, &response_mutex, &completed_count]() {
            auto [model, response] = this->processTask(task);
            
            std::lock_guard<std::mutex> lock(response_mutex);
            responses.push_back(response);
            completed_count++;
        });

        // 控制并发数
        if (threads.size() >= max_concurrent_tasks_) {
            for (auto& t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            threads.clear();
        }
    }

    // 等待剩余线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return responses;
}

std::vector<ModelConfig> ModelManager::getAvailableModels() {
    std::vector<ModelConfig> models;
    for (const auto& [id, config] : available_models_) {
        models.push_back(config);
    }
    return models;
}

ModelPerformance ModelManager::getModelPerformance(const std::string& model_id) {
    auto it = model_performance_.find(model_id);
    return (it != model_performance_.end()) ? it->second : ModelPerformance{};
}

std::map<std::string, ModelPerformance> ModelManager::getAllModelPerformance() {
    return model_performance_;
}

bool ModelManager::loadModel(const std::string& model_id) {
    auto it = available_models_.find(model_id);
    if (it == available_models_.end()) {
        return false;
    }

    if (client_->loadModel(model_id)) {
        current_model_ = model_id;
        return true;
    }

    return false;
}

bool ModelManager::unloadModel(const std::string& model_id) {
    // LM Studio可能不支持显式卸载，这里只是逻辑卸载
    if (current_model_ == model_id) {
        current_model_ = "";
        return true;
    }
    return false;
}

void ModelManager::setPreferredModel(const std::string& model_id, TaskType task_type) {
    if (task_type == TaskType::UNKNOWN) {
        // 设置为所有任务的首选
        for (int i = 0; i < static_cast<int>(TaskType::NETWORK_ANALYSIS) + 1; ++i) {
            TaskType type = static_cast<TaskType>(i);
            preferred_models_[type] = model_id;
        }
    } else {
        preferred_models_[task_type] = model_id;
    }
}

std::string ModelManager::getCurrentModel() {
    return current_model_;
}

LLMSystemStatus ModelManager::getSystemStatus() {
    LLMSystemStatus status;
    status.is_connected = client_->checkConnection();
    status.active_tasks = current_task_count_;
    status.queued_tasks = static_cast<int>(task_queue_.size());
    
    // 获取可用模型列表
    for (const auto& [id, config] : available_models_) {
        status.available_models.push_back(id);
    }
    
    status.model_stats = model_performance_;
    status.last_update = std::chrono::system_clock::now();
    
    // 简单的系统资源估算（实际项目中应使用更精确的方法）
    status.cpu_usage = 0.3; // 假设30%
    status.memory_usage = 0.5; // 假设50%

    return status;
}

void ModelManager::setAutoModelSelection(bool enabled) {
    auto_model_selection_ = enabled;
}

void ModelManager::setSelectionStrategy(const std::string& strategy) {
    selection_strategy_ = strategy;
}

// 私有方法实现
void ModelManager::initializeDefaultModels() {
    // 如果没有扫描到模型，添加一些默认配置
    if (available_models_.empty()) {
        // 轻量级分类模型
        ModelConfig classifier;
        classifier.name = "轻量级分类器";
        classifier.model_id = "llama3.1-8b-instruct";
        classifier.max_tokens = 2048;
        classifier.context_length = 8192;
        classifier.default_temperature = 0.1;
        classifier.inference_speed = 50.0;
        classifier.memory_usage = 8.0;
        classifier.is_multimodal = false;
        classifier.capabilities = {
            ModelCapability::TEXT_PROCESSING,
            ModelCapability::CLASSIFICATION,
            ModelCapability::FAST_INFERENCE
        };
        classifier.description = "用于任务分类的轻量级模型";
        available_models_[classifier.model_id] = classifier;

        // 通用分析模型
        ModelConfig analyzer;
        analyzer.name = "通用分析模型";
        analyzer.model_id = "qwen2-7b-instruct";
        analyzer.max_tokens = 4096;
        analyzer.context_length = 32768;
        analyzer.default_temperature = 0.3;
        analyzer.inference_speed = 25.0;
        analyzer.memory_usage = 14.0;
        analyzer.is_multimodal = false;
        analyzer.capabilities = {
            ModelCapability::TEXT_PROCESSING,
            ModelCapability::ANALYSIS,
            ModelCapability::REASONING,
            ModelCapability::CHINESE_PROCESSING
        };
        analyzer.description = "通用文本分析模型，支持中文";
        available_models_[analyzer.model_id] = analyzer;

        // 多模态模型
        ModelConfig multimodal;
        multimodal.name = "多模态模型";
        multimodal.model_id = "llava-v1.6-34b";
        multimodal.max_tokens = 4096;
        multimodal.context_length = 4096;
        multimodal.default_temperature = 0.2;
        multimodal.inference_speed = 5.0;
        multimodal.memory_usage = 24.0;
        multimodal.is_multimodal = true;
        multimodal.capabilities = {
            ModelCapability::MULTIMODAL,
            ModelCapability::IMAGE_PROCESSING,
            ModelCapability::TEXT_PROCESSING
        };
        multimodal.description = "支持图像和文本的多模态模型";
        available_models_[multimodal.model_id] = multimodal;
    }
}

ModelConfig ModelManager::selectBestModel(const AITask& task, const TaskClassification& classification) {
    // 如果有首选模型且支持当前任务，优先使用
    if (auto_model_selection_) {
        auto preferred_it = preferred_models_.find(classification.primary_type);
        if (preferred_it != preferred_models_.end()) {
            auto model_it = available_models_.find(preferred_it->second);
            if (model_it != available_models_.end() && 
                isModelCapable(model_it->second, task)) {
                return model_it->second;
            }
        }
    }

    // 筛选支持当前任务的模型
    std::vector<ModelConfig> capable_models;
    for (const auto& [id, model] : available_models_) {
        if (isModelCapable(model, task)) {
            capable_models.push_back(model);
        }
    }

    if (capable_models.empty()) {
        // 如果没有合适的模型，返回当前模型（如果有的话）
        if (!current_model_.empty()) {
            return available_models_[current_model_];
        }
        // 否则返回第一个可用模型
        return available_models_.begin()->second;
    }

    // 根据策略选择最佳模型
    SelectionStrategy strategy = getStrategy(selection_strategy_);
    
    switch (strategy) {
        case SelectionStrategy::PERFORMANCE_BASED:
            return selectByPerformance(capable_models, task);
        case SelectionStrategy::CAPABILITY_BASED:
            return selectByCapability(capable_models, 
                                     getRequiredCapabilities(task, classification));
        case SelectionStrategy::LOAD_BALANCED:
            return selectByLoadBalancing(capable_models);
        case SelectionStrategy::COST_OPTIMIZED:
            // 选择内存使用最少的模型
            return *std::min_element(capable_models.begin(), capable_models.end(),
                [](const ModelConfig& a, const ModelConfig& b) {
                    return a.memory_usage < b.memory_usage;
                });
        case SelectionStrategy::QUALITY_FOCUSED:
            // 选择上下文最长的模型
            return *std::max_element(capable_models.begin(), capable_models.end(),
                [](const ModelConfig& a, const ModelConfig& b) {
                    return a.context_length < b.context_length;
                });
        default:
            return capable_models[0];
    }
}

bool ModelManager::isModelCapable(const ModelConfig& model, const AITask& task) {
    // 检查复杂度要求
    if (model.max_complexity < task.complexity) {
        return false;
    }

    // 检查多模态要求
    if (task.requires_multi_modal && !model.is_multimodal) {
        return false;
    }

    // 检查token数量要求
    if (task.max_tokens > model.max_tokens) {
        return false;
    }

    // 检查任务类型特定的能力要求
    std::vector<ModelCapability> required_caps = getRequiredCapabilities(task, 
                                                                    TaskClassification{});
    for (const auto& required : required_caps) {
        if (std::find(model.capabilities.begin(), model.capabilities.end(), required) 
            == model.capabilities.end()) {
            return false;
        }
    }

    return true;
}

double ModelManager::calculateModelScore(const ModelConfig& model, const AITask& task, 
                                      const TaskClassification& classification) {
    double score = 0.0;

    // 性能分数 (40%)
    auto perf_it = model_performance_.find(model.model_id);
    if (perf_it != model_performance_.end()) {
        double performance_score = perf_it->second.success_rate * 0.5 + 
                               (1.0 / (1.0 + perf_it->second.avg_response_time)) * 0.5;
        score += performance_score * 0.4;
    }

    // 能力匹配分数 (30%)
    std::vector<ModelCapability> required = getRequiredCapabilities(task, classification);
    int matched_caps = 0;
    for (const auto& required : required) {
        if (std::find(model.capabilities.begin(), model.capabilities.end(), required) 
            != model.capabilities.end()) {
            matched_caps++;
        }
    }
    double capability_score = required.empty() ? 1.0 : 
                               static_cast<double>(matched_caps) / required.size();
    score += capability_score * 0.3;

    // 资源效率分数 (20%)
    double resource_score = 1.0 / (1.0 + model.memory_usage / 16.0); // 假设16GB为基准
    score += resource_score * 0.2;

    // 速度分数 (10%)
    double speed_score = model.inference_speed / 50.0; // 假设50 tokens/s为基准
    score += std::min(1.0, speed_score) * 0.1;

    return std::min(1.0, score);
}

void ModelManager::workerThreadFunc() {
    while (!is_shutdown_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this] {
            return !task_queue_.empty() || is_shutdown_;
        });
        
        if (is_shutdown_) {
            break;
        }

        if (!task_queue_.empty() && current_task_count_ < max_concurrent_tasks_) {
            QueuedTask queued_task = task_queue_.top();
            task_queue_.pop();
            lock.unlock();
            
            current_task_count_++;
            
            try {
                AIResponse response = executeTask(queued_task.task);
                if (queued_task.callback) {
                    queued_task.callback(response);
                }
            }
            catch (const std::exception& e) {
                // 错误处理
                AIResponse error_response;
                error_response.task_id = queued_task.task.id;
                error_response.error_message = e.what();
                error_response.confidence = 0.0;
                error_response.processing_time = 0.0;
                error_response.timestamp = std::chrono::system_clock::now();
                
                if (queued_task.callback) {
                    queued_task.callback(error_response);
                }
            }
            
            current_task_count_--;
        } else {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

AIResponse ModelManager::executeTask(const AITask& task) {
    auto [model, response] = processTask(task);
    return response;
}

std::pair<std::string, std::string> ModelManager::buildTaskPrompts(const AITask& task, 
                                                               const TaskClassification& classification) {
    std::string system_prompt;
    std::string user_prompt = task.input_text;

    // 根据任务类型构建系统提示词
    switch (classification.primary_type) {
        case TaskType::EVIDENCE_SUMMARY:
            system_prompt = R"(你是一位专业的数字取证专家。请对提供的证据进行客观、准确的分析和总结。
要求：
1. 提取关键信息
2. 识别重要发现
3. 评估证据价值
4. 保持客观中立
请用简洁、专业的语言回答。)";
            break;

        case TaskType::ANOMALY_DETECTION:
            system_prompt = R"(你是一位专业的安全分析师。请分析提供的数据，识别异常模式或可疑行为。
要求：
1. 识别异常模式
2. 评估风险等级
3. 提供可能的解释
4. 建议进一步调查方向
请基于数据特征进行分析，避免主观臆断。)";
            break;

        case TaskType::TEXT_ANALYSIS:
            system_prompt = R"(你是一位专业的文本分析专家。请对提供的文本进行深入分析。
要求：
1. 提取主要观点
2. 识别关键信息
3. 分析文本特征
4. 提供结构化总结
请保持分析的准确性和客观性。)";
            break;

        case TaskType::CONVERSATION_ANALYSIS:
            system_prompt = R"(你是一位专业的对话分析专家。请分析提供的对话记录。
要求：
1. 识别对话主题
2. 分析参与者关系
3. 提取关键信息
4. 识别情感倾向
5. 注意时间模式和频率
请提供客观、全面的分析。)";
            break;

        default:
            system_prompt = R"(你是一位专业的数字取证分析师。请对提供的内容进行专业、准确的分析。
要求：
1. 保持客观中立
2. 基于事实进行分析
3. 提供有价值的洞察
4. 使用专业术语
请确保分析的准确性和专业性。)";
            break;
    }

    // 添加复杂度相关的指导
    if (task.complexity == TaskComplexity::HIGH || 
        task.complexity == TaskComplexity::CRITICAL) {
        system_prompt += "\n\n这是一个复杂的分析任务，请进行深度思考和多角度分析。";
    }

    return {system_prompt, user_prompt};
}

void ModelManager::updateModelPerformance(const std::string& model_id, bool success, 
                                     double response_time, double confidence) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto& perf = model_performance_[model_id];
    
    if (perf.total_requests == 0) {
        perf.avg_response_time = response_time;
        perf.avg_confidence = confidence;
        perf.success_rate = success ? 1.0 : 0.0;
    } else {
        // 使用指数移动平均
        double alpha = 0.1;
        perf.avg_response_time = alpha * response_time + (1 - alpha) * perf.avg_response_time;
        perf.avg_confidence = alpha * confidence + (1 - alpha) * perf.avg_confidence;
        perf.success_rate = alpha * (success ? 1.0 : 0.0) + (1 - alpha) * perf.success_rate;
    }
    
    perf.total_requests++;
    perf.last_used = std::chrono::system_clock::now();
    
    if (!success) {
        perf.error_count++;
    }
}

// 静态方法实现
std::string ModelManager::selectionStrategyToString(SelectionStrategy strategy) {
    switch (strategy) {
        case SelectionStrategy::PERFORMANCE_BASED: return "performance_based";
        case SelectionStrategy::CAPABILITY_BASED: return "capability_based";
        case SelectionStrategy::LOAD_BALANCED: return "load_balanced";
        case SelectionStrategy::COST_OPTIMIZED: return "cost_optimized";
        case SelectionStrategy::QUALITY_FOCUSED: return "quality_focused";
        default: return "unknown";
    }
}

ModelManager::SelectionStrategy ModelManager::stringToSelectionStrategy(const std::string& strategy) {
    std::map<std::string, SelectionStrategy> strategy_map = {
        {"performance_based", SelectionStrategy::PERFORMANCE_BASED},
        {"capability_based", SelectionStrategy::CAPABILITY_BASED},
        {"load_balanced", SelectionStrategy::LOAD_BALANCED},
        {"cost_optimized", SelectionStrategy::COST_OPTIMIZED},
        {"quality_focused", SelectionStrategy::QUALITY_FOCUSED}
    };
    
    auto it = strategy_map.find(strategy);
    return (it != strategy_map.end()) ? it->second : SelectionStrategy::PERFORMANCE_BASED;
}

nlohmann::json ModelManager::modelPerformanceToJson(const ModelPerformance& performance) {
    return nlohmann::json{
        {"model_id", performance.model_id},
        {"avg_response_time", performance.avg_response_time},
        {"success_rate", performance.success_rate},
        {"avg_confidence", performance.avg_confidence},
        {"total_requests", performance.total_requests},
        {"last_used", std::chrono::duration_cast<std::chrono::seconds>(
            performance.last_used.time_since_epoch()).count()},
        {"memory_usage", performance.memory_usage},
        {"error_count", performance.error_count}
    };
}

} // namespace llm
} // namespace forensics