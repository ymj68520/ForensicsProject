#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

namespace forensics {
namespace llm {

// 任务类型枚举
enum class TaskType {
    UNKNOWN,
    TEXT_ANALYSIS,
    IMAGE_ANALYSIS,
    DOCUMENT_ANALYSIS,
    CONVERSATION_ANALYSIS,
    EVIDENCE_SUMMARY,
    REPORT_GENERATION,
    ANOMALY_DETECTION,
    PATTERN_RECOGNITION,
    SENTIMENT_ANALYSIS,
    ENTITY_RECOGNITION,
    MULTIMODAL_ANALYSIS,
    CODE_ANALYSIS,
    NETWORK_ANALYSIS
};

// 任务复杂度枚举
enum class TaskComplexity {
    LOW,      // 简单分类、识别
    MEDIUM,   // 中等复杂度分析
    HIGH,     // 复杂推理、综合分析
    CRITICAL  // 最复杂的多模态、深度分析
};

// 模型能力枚举
enum class ModelCapability {
    TEXT_PROCESSING,
    IMAGE_PROCESSING,
    MULTIMODAL,
    CODE_UNDERSTANDING,
    REASONING,
    ANALYSIS,
    SUMMARIZATION,
    TRANSLATION,
    CHINESE_PROCESSING,
    ENGLISH_PROCESSING,
    LONG_CONTEXT,
    FAST_INFERENCE
};

// AI任务结构
struct AITask {
    std::string id;
    TaskType type;
    TaskComplexity complexity;
    std::string input_text;
    std::string input_image_path;
    std::map<std::string, std::string> metadata;
    std::string system_prompt;
    double temperature = 0.3;
    int max_tokens = 2048;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point deadline;
    
    // 任务优先级
    int priority = 5;
    std::string user_id;
    std::string case_id;
};

// 任务分类结果
struct TaskClassification {
    TaskType primary_type;
    TaskType secondary_type;
    TaskComplexity complexity;
    double confidence;
    std::string reasoning;
    std::vector<TaskType> possible_types;
};

// 模型配置
struct ModelConfig {
    std::string name;
    std::string model_id;
    std::string api_endpoint;
    std::vector<ModelCapability> capabilities;
    TaskComplexity max_complexity;
    int max_tokens;
    double default_temperature;
    int context_length;
    double inference_speed;  // tokens/second
    double memory_usage;    // GB
    bool is_multimodal;
    std::vector<std::string> supported_languages;
    std::string description;
};

// AI响应结果
struct AIResponse {
    std::string task_id;
    std::string model_used;
    std::string content;
    double confidence;
    int tokens_used;
    double processing_time;
    std::string error_message;
    nlohmann::json metadata;
    std::chrono::system_clock::time_point timestamp;
};

// 文本分析结果
struct TextAnalysisResult {
    std::string summary;
    std::vector<std::string> key_findings;
    std::vector<std::string> entities;
    std::map<std::string, double> sentiment_scores;
    std::vector<std::string> topics;
    std::string language_detected;
    double suspicious_score;
    std::string relevance_assessment;
};

// 图像分析结果
struct ImageAnalysisResult {
    std::string description;
    std::vector<std::string> objects_detected;
    std::vector<std::string> text_extracted;
    std::string scene_type;
    std::vector<std::string> suspicious_elements;
    double confidence_score;
    std::map<std::string, double> object_scores;
};

// 多模态分析结果
struct MultiModalResult {
    TextAnalysisResult text_analysis;
    ImageAnalysisResult image_analysis;
    std::string cross_modal_insights;
    std::vector<std::string> correlations;
    std::string overall_assessment;
};

// 证据分析结果
struct EvidenceAnalysisResult {
    std::string evidence_id;
    std::string content_summary;
    std::vector<std::string> key_findings;
    double relevance_score;
    double suspicious_score;
    std::string recommended_actions;
    std::vector<std::string> related_evidence;
    std::string confidence_level;
};

// 报告生成结果
struct ReportGenerationResult {
    std::string report_content;
    std::vector<std::string> sections;
    std::string executive_summary;
    std::vector<std::string> recommendations;
    std::string confidence_level;
    std::map<std::string, int> evidence_references;
};

// 模型性能指标
struct ModelPerformance {
    std::string model_id;
    double avg_response_time;
    double success_rate;
    double avg_confidence;
    int total_requests;
    std::chrono::system_clock::time_point last_used;
    double memory_usage;
    int error_count;
};

// LLM系统状态
struct LLMSystemStatus {
    bool is_connected;
    std::vector<std::string> available_models;
    std::map<std::string, ModelPerformance> model_stats;
    int active_tasks;
    int queued_tasks;
    double cpu_usage;
    double memory_usage;
    std::chrono::system_clock::time_point last_update;
};

// API错误类型
enum class APIError {
    SUCCESS,
    CONNECTION_ERROR,
    TIMEOUT_ERROR,
    INVALID_REQUEST,
    MODEL_NOT_FOUND,
    RATE_LIMIT_EXCEEDED,
    INSUFFICIENT_RESOURCES,
    AUTHENTICATION_ERROR,
    PARSING_ERROR,
    UNKNOWN_ERROR
};

// API响应包装
struct APIResponse {
    APIError error_code;
    std::string error_message;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;
    
    bool is_success() const { return error_code == APIError::SUCCESS; }
    static APIResponse success(const nlohmann::json& data);
    static APIResponse error(APIError code, const std::string& message);
};

// 任务队列项
struct QueuedTask {
    AITask task;
    std::function<void(const AIResponse&)> callback;
    int retry_count;
    std::chrono::system_clock::time_point next_attempt;
    
    bool operator<(const QueuedTask& other) const {
        return task.priority < other.task.priority; // 高优先级排在前面
    }
};

// 缓存项
struct CacheItem {
    std::string response;
    std::chrono::system_clock::time_point cached_at;
    std::chrono::seconds ttl;
    int access_count;
    
    bool is_expired() const {
        return std::chrono::system_clock::now() > cached_at + ttl;
    }
};

} // namespace llm
} // namespace forensics