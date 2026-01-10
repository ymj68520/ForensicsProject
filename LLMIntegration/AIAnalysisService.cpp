#include "AIAnalysisService.h"
#include <sstream>
#include <algorithm>
#include <regex>
#include <chrono>
#include <random>
#include <fstream>

namespace forensics {
namespace llm {

AIAnalysisService::AIAnalysisService(std::shared_ptr<ModelManager> model_manager)
    : model_manager_(model_manager)
    , is_initialized_(false) {
    
    // 初始化分析参数
    analysis_parameters_ = getDefaultAnalysisParameters();
}

bool AIAnalysisService::initialize() {
    try {
        // 初始化分析模板
        initializeAnalysisTemplates();
        
        // 检查模型管理器是否可用
        if (!model_manager_) {
            return false;
        }

        auto system_status = model_manager_->getSystemStatus();
        if (!system_status.is_connected || system_status.available_models.empty()) {
            return false;
        }

        is_initialized_ = true;
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

EvidenceAnalysisResult AIAnalysisService::analyzeEvidence(const std::string& evidence, 
                                                     const std::map<std::string, std::string>& metadata) {
    if (!is_initialized_) {
        EvidenceAnalysisResult error_result;
        error_result.evidence_id = generateEvidenceId();
        error_result.confidence_level = "ERROR";
        error_result.error_message = "Service not initialized";
        return error_result;
    }

    try {
        // 选择分析模板
        std::string evidence_type = "text";
        auto type_it = metadata.find("type");
        if (type_it != metadata.end()) {
            evidence_type = type_it->second;
        }
        
        AnalysisTemplate template_config = selectAnalysisTemplate(evidence_type);

        // 构建分析任务
        AITask task = buildEvidenceAnalysisTask(evidence, metadata, template_config);
        task.id = generateEvidenceId();

        // 执行分析
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        EvidenceAnalysisResult result = parseEvidenceAnalysisResponse(response);
        result.evidence_id = task.id;

        // 验证分析质量
        double quality_score = validateAnalysisQuality(result);
        if (quality_score < 0.5) {
            result.recommended_actions.push_back("建议重新分析 - 质量分数较低: " + std::to_string(quality_score));
        }

        return result;
    }
    catch (const std::exception& e) {
        EvidenceAnalysisResult error_result;
        error_result.evidence_id = generateEvidenceId();
        error_result.confidence_level = "ERROR";
        error_result.error_message = e.what();
        return error_result;
    }
}

std::vector<EvidenceAnalysisResult> AIAnalysisService::analyzeBatchEvidence(
    const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& evidences) {
    
    std::vector<EvidenceAnalysisResult> results;
    results.reserve(evidences.size());

    // 转换为任务列表
    std::vector<AITask> tasks;
    for (const auto& [evidence, metadata] : evidences) {
        std::string evidence_type = "text";
        auto type_it = metadata.find("type");
        if (type_it != metadata.end()) {
            evidence_type = type_it->second;
        }
        
        AnalysisTemplate template_config = selectAnalysisTemplate(evidence_type);
        AITask task = buildEvidenceAnalysisTask(evidence, metadata, template_config);
        task.id = generateEvidenceId();
        tasks.push_back(task);
    }

    // 批量处理
    std::vector<AIResponse> responses = model_manager_->processBatch(tasks);

    // 解析结果
    for (size_t i = 0; i < responses.size(); ++i) {
        EvidenceAnalysisResult result = parseEvidenceAnalysisResponse(responses[i]);
        result.evidence_id = tasks[i].id;
        
        // 验证质量
        double quality_score = validateAnalysisQuality(result);
        if (quality_score < 0.5) {
            result.recommended_actions.push_back("建议重新分析 - 质量分数较低");
        }
        
        results.push_back(result);
    }

    return results;
}

ConversationAnalysisResult AIAnalysisService::analyzeConversation(
    const std::vector<std::map<std::string, std::string>>& conversation,
    const std::map<std::string, std::string>& metadata) {
    
    ConversationAnalysisResult result;
    
    try {
        // 构建对话分析任务
        AITask task = buildConversationAnalysisTask(conversation, metadata);
        task.id = generateEvidenceId();

        // 执行分析
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        result = parseConversationAnalysisResponse(response);

        return result;
    }
    catch (const std::exception& e) {
        result.error_message = e.what();
        return result;
    }
}

ReportGenerationResult AIAnalysisService::generateReport(const nlohmann::json& case_context,
                                                     const std::vector<EvidenceAnalysisResult>& evidence_analyses,
                                                     const std::string& report_type) {
    
    try {
        // 构建报告生成任务
        AITask task = buildReportGenerationTask(case_context, evidence_analyses, report_type);
        task.id = "report_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        // 执行生成
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        ReportGenerationResult result = parseReportGenerationResponse(response, report_type);
        
        // 添加证据引用
        for (const auto& analysis : evidence_analyses) {
            result.evidence_references[analysis.evidence_id] = 1; // 基础引用计数
        }

        return result;
    }
    catch (const std::exception& e) {
        ReportGenerationResult error_result;
        error_result.error_message = e.what();
        error_result.confidence_level = "ERROR";
        return error_result;
    }
}

std::vector<nlohmann::json> AIAnalysisService::detectAnomalies(const std::string& data,
                                                           const std::string& baseline) {
    try {
        // 构建异常检测任务
        AITask task = buildAnomalyDetectionTask(data, baseline);
        task.id = "anomaly_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        // 执行检测
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        return parseAnomalyDetectionResponse(response);
    }
    catch (const std::exception& e) {
        return {nlohmann::json{{"error", e.what()}}};
    }
}

nlohmann::json AIAnalysisService::recognizePatterns(
    const std::vector<std::map<std::string, std::string>>& time_series_data) {
    
    try {
        // 构建模式识别任务
        AITask task = buildPatternRecognitionTask(time_series_data);
        task.id = "pattern_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        // 执行识别
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        return parsePatternRecognitionResponse(response);
    }
    catch (const std::exception& e) {
        return nlohmann::json{{"error", e.what()}};
    }
}

MultiModalResult AIAnalysisService::performMultiModalAnalysis(const std::string& text,
                                                           const std::string& image_path,
                                                           const std::map<std::string, std::string>& metadata) {
    
    try {
        // 构建多模态分析任务
        AITask task = buildMultiModalTask(text, image_path, metadata);
        task.id = "multimodal_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        // 执行分析
        auto [model, response] = model_manager_->processTask(task);

        // 解析结果
        return parseMultiModalResponse(response);
    }
    catch (const std::exception& e) {
        MultiModalResult error_result;
        error_result.error_message = e.what();
        return error_result;
    }
}

std::map<std::string, double> AIAnalysisService::analyzeSentiment(const std::string& text) {
    try {
        AITask task;
        task.id = "sentiment_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        task.type = TaskType::SENTIMENT_ANALYSIS;
        task.complexity = TaskComplexity::LOW;
        task.input_text = text;
        task.system_prompt = R"(你是一位专业的情感分析专家。请分析提供的文本，给出以下维度的情感分数：
1. 积极情感 (0-1分)
2. 消极情感 (0-1分)  
3. 中性情感 (0-1分)
4. 总体情感倾向 (positive/negative/neutral)

请以JSON格式返回结果，确保所有分数相加等于1.0。)";

        auto [model, response] = model_manager_->processTask(task);

        // 解析情感分析结果
        std::map<std::string, double> sentiment_scores;
        try {
            auto json_result = nlohmann::json::parse(response.content);
            
            if (json_result.contains("positive")) {
                sentiment_scores["positive"] = json_result["positive"];
            }
            if (json_result.contains("negative")) {
                sentiment_scores["negative"] = json_result["negative"];
            }
            if (json_result.contains("neutral")) {
                sentiment_scores["neutral"] = json_result["neutral"];
            }
            if (json_result.contains("overall")) {
                sentiment_scores["overall"] = json_result["overall"] == "positive" ? 1.0 : 
                                        json_result["overall"] == "negative" ? -1.0 : 0.0;
            }
        }
        catch (...) {
            // 解析失败，返回默认值
            sentiment_scores["positive"] = 0.33;
            sentiment_scores["negative"] = 0.33;
            sentiment_scores["neutral"] = 0.34;
            sentiment_scores["overall"] = 0.0;
        }

        return sentiment_scores;
    }
    catch (const std::exception& e) {
        return {{"error", 1.0}};
    }
}

std::vector<std::map<std::string, std::string>> AIAnalysisService::recognizeEntities(
    const std::string& text,
    const std::vector<std::string>& entity_types) {
    
    try {
        AITask task;
        task.id = "entity_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        task.type = TaskType::ENTITY_RECOGNITION;
        task.complexity = TaskComplexity::MEDIUM;
        task.input_text = text;
        
        // 构建实体识别提示词
        std::ostringstream prompt;
        prompt << R"(你是一位专业的实体识别专家。请从以下文本中提取指定类型的实体：)";
        
        for (const auto& type : entity_types) {
            prompt << "\n- " << type;
        }
        
        prompt << R"(

文本内容：)" << text << R"(

请以JSON数组格式返回结果，每个实体包含：
- "text": 实体文本
- "type": 实体类型
- "confidence": 置信度 (0-1)

示例格式：
[
  {"text": "张三", "type": "person", "confidence": 0.95},
  {"text": "北京", "type": "location", "confidence": 0.88}
])";

        task.system_prompt = prompt.str();

        auto [model, response] = model_manager_->processTask(task);

        // 解析实体识别结果
        std::vector<std::map<std::string, std::string>> entities;
        try {
            auto json_result = nlohmann::json::parse(response.content);
            if (json_result.is_array()) {
                for (const auto& entity : json_result) {
                    std::map<std::string, std::string> entity_map;
                    if (entity.contains("text")) {
                        entity_map["text"] = entity["text"];
                    }
                    if (entity.contains("type")) {
                        entity_map["type"] = entity["type"];
                    }
                    if (entity.contains("confidence")) {
                        entity_map["confidence"] = std::to_string(entity["confidence"].get<double>());
                    }
                    entities.push_back(entity_map);
                }
            }
        }
        catch (...) {
            // 解析失败，返回空结果
        }

        return entities;
    }
    catch (const std::exception& e) {
        return {{{"error", e.what()}}};
    }
}

nlohmann::json AIAnalysisService::analyzeEvidenceRelations(
    const std::vector<EvidenceAnalysisResult>& evidences) {
    
    try {
        AITask task = buildRelationAnalysisTask(evidences);
        task.id = "relation_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        auto [model, response] = model_manager_->processTask(task);

        return parseRelationAnalysisResponse(response);
    }
    catch (const std::exception& e) {
        return nlohmann::json{{"error", e.what()}};
    }
}

nlohmann::json AIAnalysisService::rebuildTimeline(
    const std::vector<std::map<std::string, std::string>>& events) {
    
    nlohmann::json timeline = nlohmann::json::array();
    
    try {
        // 格式化事件数据
        std::string events_text;
        for (const auto& event : events) {
            auto timestamp_it = event.find("timestamp");
            auto description_it = event.find("description");
            auto type_it = event.find("type");
            
            if (timestamp_it != event.end() && description_it != event.end()) {
                events_text += "[" + timestamp_it->second + "] " + 
                             description_it->second + " (" + 
                             (type_it != event.end() ? type_it->second : "unknown") + ")\n";
            }
        }

        AITask task;
        task.id = "timeline_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        task.type = TaskType::PATTERN_RECOGNITION;
        task.complexity = TaskComplexity::MEDIUM;
        task.input_text = events_text;
        task.system_prompt = R"(你是一位专业的时间线分析专家。请根据提供的事件信息，重建结构化的时间线。

要求：
1. 分析事件的时间顺序
2. 识别事件间的因果关系
3. 检测异常的时间模式
4. 提供时间线总结

请以JSON格式返回，包含：
- "events": 事件数组（按时间排序）
- "patterns": 识别出的模式
- "anomalies": 异常事件
- "summary": 时间线总结

每个事件应包含：
- "timestamp": 时间戳
- "description": 事件描述
- "type": 事件类型
- "importance": 重要性 (1-5)
- "related_events": 相关事件索引列表)";

        auto [model, response] = model_manager_->processTask(task);

        try {
            return nlohmann::json::parse(response.content);
        }
        catch (...) {
            // 解析失败，返回基础时间线
            for (const auto& event : events) {
                timeline.push_back(event);
            }
            return nlohmann::json{{"events", timeline}};
        }
    }
    catch (const std::exception& e) {
        return nlohmann::json{{"error", e.what()}};
    }
}

void AIAnalysisService::analyzeEvidenceAsync(const std::string& evidence,
                                        const std::map<std::string, std::string>& metadata,
                                        std::function<void(const EvidenceAnalysisResult&)> callback) {
    // 构建异步任务
    AITask task;
    task.id = generateEvidenceId();
    std::string evidence_type = "text";
    auto type_it = metadata.find("type");
    if (type_it != metadata.end()) {
        evidence_type = type_it->second;
    }
    
    AnalysisTemplate template_config = selectAnalysisTemplate(evidence_type);
    task = buildEvidenceAnalysisTask(evidence, metadata, template_config);

    // 提交异步任务
    model_manager_->processTaskAsync(task, [callback, this](const AIResponse& response) {
        EvidenceAnalysisResult result = this->parseEvidenceAnalysisResponse(response);
        result.evidence_id = task.id;
        callback(result);
    });
}

nlohmann::json AIAnalysisService::getServiceStatus() {
    nlohmann::json status;
    
    status["initialized"] = is_initialized_;
    status["model_manager_status"] = model_manager_->getSystemStatus();
    
    // 分析参数
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    status["analysis_parameters"] = analysis_parameters_;
    
    // 模板数量
    status["available_templates"] = static_cast<int>(analysis_templates_.size());
    
    return status;
}

void AIAnalysisService::setAnalysisParameters(const std::map<std::string, std::string>& parameters) {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    for (const auto& [key, value] : parameters) {
        analysis_parameters_[key] = value;
    }
}

// 私有方法实现
void AIAnalysisService::initializeAnalysisTemplates() {
    // 证据分析模板
    AnalysisTemplate evidence_template;
    evidence_template.name = "evidence_analysis";
    evidence_template.system_prompt = R"(你是一位专业的数字取证专家。请对提供的证据进行深入分析。

分析要求：
1. 内容摘要：简洁准确地总结证据内容
2. 关键发现：识别重要的信息点
3. 相关性评估：评估证据与案件的关联程度
4. 可疑程度：评估证据的异常或可疑程度
5. 建议行动：基于分析结果提出建议

请以结构化格式返回分析结果。)";
    evidence_template.required_fields = {"content", "type"};
    evidence_template.default_weights = {
        {"relevance", 0.3},
        {"suspiciousness", 0.3},
        {"completeness", 0.2},
        {"accuracy", 0.2}
    };
    analysis_templates_["evidence_analysis"] = evidence_template;

    // 对话分析模板
    AnalysisTemplate conversation_template;
    conversation_template.name = "conversation_analysis";
    conversation_template.system_prompt = R"(你是一位专业的对话分析专家。请分析提供的对话记录。

分析要求：
1. 对话主题：识别讨论的主要话题
2. 参与者关系：分析参与者之间的关系
3. 情感倾向：分析对话的情感变化
4. 时间模式：识别对话的时间规律
5. 关键信息：提取重要信息点

请提供全面、客观的对话分析。)";
    conversation_template.required_fields = {"messages", "participants"};
    conversation_template.default_weights = {
        {"relevance", 0.25},
        {"completeness", 0.25},
        {"accuracy", 0.25},
        {"insightfulness", 0.25}
    };
    analysis_templates_["conversation_analysis"] = conversation_template;

    // 报告生成模板
    AnalysisTemplate report_template;
    report_template.name = "report_generation";
    report_template.system_prompt = R"(你是一位资深的数字取证报告专家。请基于提供的案件信息和证据分析结果，生成专业的取证报告。

报告要求：
1. 执行摘要：案件概述和关键发现
2. 证据分析：主要证据及其重要性
3. 技术方法：使用的技术和分析方法
4. 结论：基于证据的结论
5. 建议：后续调查建议

报告应专业、准确、逻辑清晰。)";
    report_template.required_fields = {"case_context", "evidence_analyses"};
    report_template.default_weights = {
        {"completeness", 0.3},
        {"accuracy", 0.3},
        {"professionalism", 0.2},
        {"actionability", 0.2}
    };
    analysis_templates_["report_generation"] = report_template;
}

AnalysisTemplate AIAnalysisService::selectAnalysisTemplate(const std::string& evidence_type) {
    auto it = analysis_templates_.find(evidence_type);
    if (it != analysis_templates_.end()) {
        return it->second;
    }
    
    // 默认返回证据分析模板
    return analysis_templates_["evidence_analysis"];
}

AITask AIAnalysisService::buildEvidenceAnalysisTask(const std::string& evidence,
                                                   const std::map<std::string, std::string>& metadata,
                                                   const AnalysisTemplate& template_config) {
    AITask task;
    task.type = TaskType::EVIDENCE_SUMMARY;
    task.complexity = TaskComplexity::MEDIUM;
    task.input_text = evidence;
    task.system_prompt = template_config.system_prompt;
    task.temperature = 0.3;
    task.max_tokens = 2048;
    
    // 添加元数据到提示词
    if (!metadata.empty()) {
        task.input_text += "\n\n元数据：\n";
        for (const auto& [key, value] : metadata) {
            task.input_text += key + ": " + value + "\n";
        }
    }
    
    return task;
}

EvidenceAnalysisResult AIAnalysisService::parseEvidenceAnalysisResponse(const AIResponse& response) {
    EvidenceAnalysisResult result;
    
    if (response.error_message.empty()) {
        try {
            auto json_response = nlohmann::json::parse(response.content);
            
            // 解析内容摘要
            if (json_response.contains("summary")) {
                result.content_summary = json_response["summary"];
            }
            
            // 解析关键发现
            if (json_response.contains("key_findings") && json_response["key_findings"].is_array()) {
                for (const auto& finding : json_response["key_findings"]) {
                    result.key_findings.push_back(finding.get<std::string>());
                }
            }
            
            // 解析相关性分数
            if (json_response.contains("relevance_score")) {
                result.relevance_score = json_response["relevance_score"];
            }
            
            // 解析可疑度分数
            if (json_response.contains("suspicious_score")) {
                result.suspicious_score = json_response["suspicious_score"];
            }
            
            // 解析建议行动
            if (json_response.contains("recommended_actions") && json_response["recommended_actions"].is_array()) {
                for (const auto& action : json_response["recommended_actions"]) {
                    result.recommended_actions.push_back(action.get<std::string>());
                }
            }
            
            // 解析置信度
            result.confidence_level = "MEDIUM";
            if (response.confidence > 0.8) {
                result.confidence_level = "HIGH";
            } else if (response.confidence < 0.5) {
                result.confidence_level = "LOW";
            }
        }
        catch (...) {
            result.error_message = "Failed to parse analysis response";
            result.confidence_level = "ERROR";
        }
    } else {
        result.error_message = response.error_message;
        result.confidence_level = "ERROR";
    }
    
    return result;
}

std::string AIAnalysisService::generateEvidenceId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "evd_" + std::to_string(timestamp);
}

double AIAnalysisService::validateAnalysisQuality(const EvidenceAnalysisResult& result) {
    double quality_score = 0.0;
    
    // 检查必要字段
    if (!result.content_summary.empty()) {
        quality_score += 0.2;
    }
    
    if (!result.key_findings.empty()) {
        quality_score += 0.2;
    }
    
    if (result.relevance_score > 0) {
        quality_score += 0.2;
    }
    
    if (result.suspicious_score >= 0) {
        quality_score += 0.2;
    }
    
    if (!result.recommended_actions.empty()) {
        quality_score += 0.2;
    }
    
    // 检查错误状态
    if (!result.error_message.empty() || result.confidence_level == "ERROR") {
        quality_score = 0.0;
    }
    
    return quality_score;
}

// 静态方法实现
std::map<std::string, std::string> AIAnalysisService::getDefaultAnalysisParameters() {
    return {
        {"default_temperature", "0.3"},
        {"default_max_tokens", "2048"},
        {"quality_threshold", "0.7"},
        {"enable_validation", "true"},
        {"cache_enabled", "true"},
        {"retry_attempts", "3"}
    };
}

std::vector<std::string> AIAnalysisService::getSupportedReportTypes() {
    return {
        "comprehensive",    // 综合报告
        "summary",          // 摘要报告
        "technical",        // 技术报告
        "executive",        // 执行摘要
        "legal"           // 法律报告
    };
}

// ForensicAnalysisPresets 实现
std::map<std::string, std::string> ForensicAnalysisPresets::getMalwareAnalysisPreset() {
    return {
        {"focus", "malware_behavior,file_modifications,network_connections,process_creation"},
        {"depth", "deep"},
        {"include_memory_analysis", "true"},
        {"check_persistence_mechanisms", "true"},
        {"analyze_anti_forensics", "true"}
    };
}

std::map<std::string, std::string> ForensicAnalysisPresets::getNetworkIntrusionPreset() {
    return {
        {"focus", "unauthorized_access,privilege_escalation,data_exfiltration,lateral_movement"},
        {"depth", "comprehensive"},
        {"include_traffic_analysis", "true"},
        {"check_firewall_bypass", "true"},
        {"analyze_attacker_tools", "true"}
    };
}

std::map<std::string, std::string> ForensicAnalysisPresets::getDataLeakagePreset() {
    return {
        {"focus", "sensitive_data,access_patterns,exfiltration_methods,insider_threats"},
        {"depth", "thorough"},
        {"include_data_classification", "true"},
        {"check_upload_services", "true"},
        {"analyze_communication_channels", "true"}
    };
}

} // namespace llm
} // namespace forensics