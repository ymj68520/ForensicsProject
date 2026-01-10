#include "LLMHttpServer.h"
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>

namespace forensics {
namespace llm {

LLMHttpServer::LLMHttpServer(const std::string& lm_studio_url, int port)
    : model_manager_(nullptr)
    , analysis_service_(nullptr)
    , http_server_(nullptr)
    , lm_studio_url_(lm_studio_url)
    , port_(port)
    , api_prefix_("/api/llm")
    , cors_enabled_(false)
    , is_running_(false) {
    
    http_server_ = std::make_unique<HTTPserver>();
}

LLMHttpServer::~LLMHttpServer() {
    stop();
}

bool LLMHttpServer::initialize() {
    try {
        // 初始化模型管理器
        if (!model_manager_) {
            model_manager_ = std::make_shared<ModelManager>(lm_studio_url_);
            if (!model_manager_->initialize()) {
                return false;
            }
        }

        // 初始化AI分析服务
        if (!analysis_service_) {
            analysis_service_ = std::make_shared<AIAnalysisService>(model_manager_);
            if (!analysis_service_->initialize()) {
                return false;
            }
        }

        // 配置HTTP服务器
        http_server_->SetPort(port_);
        
        // 注册API路由
        registerRoutes();

        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool LLMHttpServer::start() {
    try {
        std::lock_guard<std::mutex> lock(server_mutex_);
        
        if (is_running_) {
            return true;
        }

        bool success = http_server_->Start();
        if (success) {
            is_running_ = true;
        }

        return success;
    }
    catch (const std::exception& e) {
        return false;
    }
}

void LLMHttpServer::stop() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    
    if (!is_running_) {
        return;
    }

    is_running_ = false;
    http_server_->Stop();
}

void LLMHttpServer::setModelManager(std::shared_ptr<ModelManager> model_manager) {
    model_manager_ = model_manager;
}

void LLMHttpServer::setAnalysisService(std::shared_ptr<AIAnalysisService> analysis_service) {
    analysis_service_ = analysis_service;
}

nlohmann::json LLMHttpServer::getServerStatus() {
    nlohmann::json status;
    
    status["is_running"] = is_running_;
    status["port"] = port_;
    status["api_prefix"] = api_prefix_;
    status["cors_enabled"] = cors_enabled_;
    status["lm_studio_url"] = lm_studio_url_;
    
    if (model_manager_) {
        status["model_manager"] = model_manager_->getSystemStatus();
    }
    
    if (analysis_service_) {
        status["analysis_service"] = analysis_service_->getServiceStatus();
    }
    
    return status;
}

void LLMHttpServer::setApiPrefix(const std::string& prefix) {
    api_prefix_ = prefix;
}

void LLMHttpServer::enableCORS(bool enabled, const std::vector<std::string>& allowed_origins) {
    cors_enabled_ = enabled;
    allowed_origins_ = allowed_origins;
}

void LLMHttpServer::setAuthMiddleware(std::function<bool(const std::string&)> auth_callback) {
    auth_callback_ = auth_callback;
}

void LLMHttpServer::registerRoutes() {
    // 证据分析
    http_server_->RegisterCallback("POST", api_prefix_ + "/analyze/evidence",
        [this](const std::string& request, std::string& response) {
            this->handleAnalyzeEvidence(request, response);
        });

    // 批量分析
    http_server_->RegisterCallback("POST", api_prefix_ + "/analyze/batch",
        [this](const std::string& request, std::string& response) {
            this->handleBatchAnalysis(request, response);
        });

    // 对话分析
    http_server_->RegisterCallback("POST", api_prefix_ + "/analyze/conversation",
        [this](const std::string& request, std::string& response) {
            this->handleConversationAnalysis(request, response);
        });

    // 报告生成
    http_server_->RegisterCallback("POST", api_prefix_ + "/generate/report",
        [this](const std::string& request, std::string& response) {
            this->handleReportGeneration(request, response);
        });

    // 异常检测
    http_server_->RegisterCallback("POST", api_prefix_ + "/detect/anomalies",
        [this](const std::string& request, std::string& response) {
            this->handleAnomalyDetection(request, response);
        });

    // 模式识别
    http_server_->RegisterCallback("POST", api_prefix_ + "/recognize/patterns",
        [this](const std::string& request, std::string& response) {
            this->handlePatternRecognition(request, response);
        });

    // 情感分析
    http_server_->RegisterCallback("POST", api_prefix_ + "/analyze/sentiment",
        [this](const std::string& request, std::string& response) {
            this->handleSentimentAnalysis(request, response);
        });

    // 实体识别
    http_server_->RegisterCallback("POST", api_prefix_ + "/recognize/entities",
        [this](const std::string& request, std::string& response) {
            this->handleEntityRecognition(request, response);
        });

    // 多模态分析
    http_server_->RegisterCallback("POST", api_prefix_ + "/analyze/multimodal",
        [this](const std::string& request, std::string& response) {
            this->handleMultiModalAnalysis(request, response);
        });

    // 获取模型列表
    http_server_->RegisterCallback("GET", api_prefix_ + "/models",
        [this](const std::string& request, std::string& response) {
            this->handleGetModels(request, response);
        });

    // 获取模型信息
    http_server_->RegisterCallback("GET", api_prefix_ + "/models/*",
        [this](const std::string& request, std::string& response) {
            this->handleGetModelInfo(request, response);
        });

    // 加载模型
    http_server_->RegisterCallback("POST", api_prefix_ + "/models/*/load",
        [this](const std::string& request, std::string& response) {
            this->handleLoadModel(request, response);
        });

    // 获取系统状态
    http_server_->RegisterCallback("GET", api_prefix_ + "/status",
        [this](const std::string& request, std::string& response) {
            this->handleGetStatus(request, response);
        });

    // 任务分类
    http_server_->RegisterCallback("POST", api_prefix_ + "/classify/task",
        [this](const std::string& request, std::string& response) {
            this->handleTaskClassification(request, response);
        });
}

void LLMHttpServer::setCORSHeaders(std::string& response) {
    if (!cors_enabled_) {
        return;
    }

    // 简化的CORS头设置
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
}

bool LLMHttpServer::authenticate(const std::string& request) {
    if (!auth_callback_) {
        return true; // 没有设置认证回调，默认通过
    }

    // 简化的认证检查（实际项目中应从HTTP头中提取token）
    return auth_callback_(request);
}

nlohmann::json LLMHttpServer::buildErrorResponse(const std::string& error_code, 
                                                const std::string& error_message) {
    return APIResponseBuilder::buildError(error_code, error_message);
}

nlohmann::json LLMHttpServer::buildSuccessResponse(const nlohmann::json& data) {
    return APIResponseBuilder::buildSuccess(data);
}

// API处理函数实现
void LLMHttpServer::handleAnalyzeEvidence(const std::string& request, std::string& response) {
    try {
        // 认证检查
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        // 解析请求
        auto json_request = parseJsonRequest(request);
        auto [valid, error_msg] = validateRequiredFields(json_request, {"evidence"});
        
        if (!valid) {
            response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n\r\n";
            setCORSHeaders(response);
            response += buildErrorResponse("MISSING_FIELDS", error_msg).dump();
            return;
        }

        // 提取参数
        std::string evidence = json_request["evidence"];
        std::map<std::string, std::string> metadata;
        
        if (json_request.contains("metadata") && json_request["metadata"].is_object()) {
            for (auto& [key, value] : json_request["metadata"].items()) {
                metadata[key] = value.get<std::string>();
            }
        }

        // 执行分析
        auto result = analysis_service_->analyzeEvidence(evidence, metadata);

        // 构建响应
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(evidenceAnalysisResultToJson(result)).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

void LLMHttpServer::handleBatchAnalysis(const std::string& request, std::string& response) {
    try {
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        auto json_request = parseJsonRequest(request);
        auto [valid, error_msg] = validateRequiredFields(json_request, {"evidences"});
        
        if (!valid) {
            response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n\r\n";
            setCORSHeaders(response);
            response += buildErrorResponse("MISSING_FIELDS", error_msg).dump();
            return;
        }

        // 解析证据列表
        std::vector<std::pair<std::string, std::map<std::string, std::string>>> evidences;
        
        for (const auto& evidence_item : json_request["evidences"]) {
            if (evidence_item.contains("content") && evidence_item.contains("metadata")) {
                std::string content = evidence_item["content"];
                std::map<std::string, std::string> metadata;
                
                if (evidence_item["metadata"].is_object()) {
                    for (auto& [key, value] : evidence_item["metadata"].items()) {
                        metadata[key] = value.get<std::string>();
                    }
                }
                
                evidences.emplace_back(content, metadata);
            }
        }

        // 执行批量分析
        auto results = analysis_service_->analyzeBatchEvidence(evidences);

        // 构建响应
        nlohmann::json results_array = nlohmann::json::array();
        for (const auto& result : results) {
            results_array.push_back(evidenceAnalysisResultToJson(result));
        }

        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(results_array).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

void LLMHttpServer::handleConversationAnalysis(const std::string& request, std::string& response) {
    try {
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        auto json_request = parseJsonRequest(request);
        auto [valid, error_msg] = validateRequiredFields(json_request, {"conversation"});
        
        if (!valid) {
            response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n\r\n";
            setCORSHeaders(response);
            response += buildErrorResponse("MISSING_FIELDS", error_msg).dump();
            return;
        }

        // 解析对话数据
        std::vector<std::map<std::string, std::string>> conversation;
        for (const auto& message : json_request["conversation"]) {
            std::map<std::string, std::string> msg_map;
            for (auto& [key, value] : message.items()) {
                msg_map[key] = value.get<std::string>();
            }
            conversation.push_back(msg_map);
        }

        std::map<std::string, std::string> metadata;
        if (json_request.contains("metadata")) {
            for (auto& [key, value] : json_request["metadata"].items()) {
                metadata[key] = value.get<std::string>();
            }
        }

        // 执行分析
        auto result = analysis_service_->analyzeConversation(conversation, metadata);

        // 构建响应
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(conversationAnalysisResultToJson(result)).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

void LLMHttpServer::handleReportGeneration(const std::string& request, std::string& response) {
    try {
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        auto json_request = parseJsonRequest(request);
        auto [valid, error_msg] = validateRequiredFields(json_request, {"case_context", "evidence_analyses"});
        
        if (!valid) {
            response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n\r\n";
            setCORSHeaders(response);
            response += buildErrorResponse("MISSING_FIELDS", error_msg).dump();
            return;
        }

        // 提取参数
        nlohmann::json case_context = json_request["case_context"];
        std::string report_type = json_request.value("report_type", "comprehensive");

        // 解析证据分析结果（简化处理）
        std::vector<EvidenceAnalysisResult> evidence_analyses;
        // 这里需要根据实际情况解析证据分析结果

        // 生成报告
        auto result = analysis_service_->generateReport(case_context, evidence_analyses, report_type);

        // 构建响应
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(reportGenerationResultToJson(result)).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

void LLMHttpServer::handleGetModels(const std::string& request, std::string& response) {
    try {
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        auto models = model_manager_->getAvailableModels();
        nlohmann::json models_array = nlohmann::json::array();
        
        for (const auto& model : models) {
            models_array.push_back({
                {"model_id", model.model_id},
                {"name", model.name},
                {"max_tokens", model.max_tokens},
                {"context_length", model.context_length},
                {"is_multimodal", model.is_multimodal},
                {"description", model.description}
            });
        }

        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(models_array).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

void LLMHttpServer::handleGetStatus(const std::string& request, std::string& response) {
    try {
        if (!authenticate(request)) {
            response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            setCORSHeaders(response);
            return;
        }

        auto status = getServerStatus();

        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildSuccessResponse(status).dump();
    }
    catch (const std::exception& e) {
        response = "HTTP/1.1 500 Internal Server Error\r\n";
        response += "Content-Type: application/json\r\n\r\n";
        setCORSHeaders(response);
        response += buildErrorResponse("INTERNAL_ERROR", e.what()).dump();
    }
}

// 工具函数实现
nlohmann::json LLMHttpServer::parseJsonRequest(const std::string& request) {
    // 简化的JSON解析（实际项目中应从HTTP请求体中提取）
    size_t body_start = request.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        std::string body = request.substr(body_start + 4);
        return nlohmann::json::parse(body);
    }
    return nlohmann::json{};
}

std::pair<bool, std::string> LLMHttpServer::validateRequiredFields(
    const nlohmann::json& json,
    const std::vector<std::string>& required_fields) {
    
    for (const auto& field : required_fields) {
        if (!json.contains(field)) {
            return {false, "Missing required field: " + field};
        }
    }
    return {true, ""};
}

nlohmann::json LLMHttpServer::evidenceAnalysisResultToJson(const EvidenceAnalysisResult& result) {
    return nlohmann::json{
        {"evidence_id", result.evidence_id},
        {"content_summary", result.content_summary},
        {"key_findings", result.key_findings},
        {"relevance_score", result.relevance_score},
        {"suspicious_score", result.suspicious_score},
        {"recommended_actions", result.recommended_actions},
        {"confidence_level", result.confidence_level},
        {"error_message", result.error_message}
    };
}

nlohmann::json LLMHttpServer::conversationAnalysisResultToJson(const ConversationAnalysisResult& result) {
    return nlohmann::json{
        {"topics", result.topics},
        {"sentiment_scores", result.sentiment_scores},
        {"suspicious_patterns", result.suspicious_patterns},
        {"relationship_analysis", result.relationship_analysis},
        {"error_message", result.error_message}
    };
}

nlohmann::json LLMHttpServer::reportGenerationResultToJson(const ReportGenerationResult& result) {
    return nlohmann::json{
        {"report_content", result.report_content},
        {"sections", result.sections},
        {"executive_summary", result.executive_summary},
        {"recommendations", result.recommendations},
        {"confidence_level", result.confidence_level},
        {"evidence_references", result.evidence_references},
        {"error_message", result.error_message}
    };
}

nlohmann::json LLMHttpServer::multiModalResultToJson(const MultiModalResult& result) {
    return nlohmann::json{
        {"text_analysis", nlohmann::json{
            {"summary", result.text_analysis.summary},
            {"key_findings", result.text_analysis.key_findings}
        }},
        {"image_analysis", nlohmann::json{
            {"description", result.image_analysis.description},
            {"objects_detected", result.image_analysis.objects_detected}
        }},
        {"cross_modal_insights", result.cross_modal_insights},
        {"correlations", result.correlations},
        {"overall_assessment", result.overall_assessment},
        {"error_message", result.error_message}
    };
}

// APIResponseBuilder 实现
nlohmann::json APIResponseBuilder::buildSuccess(const nlohmann::json& data,
                                              const std::string& message,
                                              const std::chrono::system_clock::time_point& timestamp) {
    nlohmann::json response;
    response["success"] = true;
    response["message"] = message;
    response["timestamp"] = formatTimestamp(timestamp);
    
    if (data != nullptr) {
        response["data"] = data;
    }
    
    return response;
}

nlohmann::json APIResponseBuilder::buildError(const std::string& error_code,
                                            const std::string& error_message,
                                            const nlohmann::json& details,
                                            const std::chrono::system_clock::time_point& timestamp) {
    nlohmann::json response;
    response["success"] = false;
    response["error_code"] = error_code;
    response["error_message"] = error_message;
    response["timestamp"] = formatTimestamp(timestamp);
    
    if (details != nullptr) {
        response["details"] = details;
    }
    
    return response;
}

std::string APIResponseBuilder::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// APIRequestValidator 实现
bool APIRequestValidator::validateRequestSize(size_t content_length, size_t max_size) {
    return content_length <= max_size;
}

bool APIRequestValidator::validateJson(const std::string& json_string) {
    try {
        nlohmann::json::parse(json_string);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool APIRequestValidator::validateFilePath(const std::string& file_path) {
    // 检查路径遍历攻击
    if (containsPathTraversal(file_path)) {
        return false;
    }
    
    // 检查危险扩展名
    std::vector<std::string> dangerous_extensions = {".exe", ".bat", ".cmd", ".scr", ".pif"};
    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    return std::find(dangerous_extensions.begin(), dangerous_extensions.end(), extension) 
           == dangerous_extensions.end();
}

bool APIRequestValidator::validateInputText(const std::string& text) {
    // 检查SQL注入和XSS
    return !containsSqlInjection(text) && !containsXss(text);
}

bool APIRequestValidator::containsSqlInjection(const std::string& text) {
    std::vector<std::string> sql_patterns = {
        "' OR", "'; DROP", "UNION SELECT", "--", "/*", "XP_", "SP_"
    };
    
    std::string upper_text = text;
    std::transform(upper_text.begin(), upper_text.end(), upper_text.begin(), ::toupper);
    
    for (const auto& pattern : sql_patterns) {
        if (upper_text.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool APIRequestValidator::containsXss(const std::string& text) {
    std::vector<std::string> xss_patterns = {
        "<script>", "javascript:", "onload=", "onerror=", "onclick=", "eval("
    };
    
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    for (const auto& pattern : xss_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool APIRequestValidator::containsPathTraversal(const std::string& path) {
    return path.find("../") != std::string::npos || 
           path.find("..\\") != std::string::npos ||
           path.find("%2e%2e%2f") != std::string::npos;
}

std::string APIRequestValidator::sanitizeInput(const std::string& input) {
    std::string sanitized = input;
    
    // 简单的HTML实体编码
    std::replace(sanitized.begin(), sanitized.end(), '<', '&lt;');
    std::replace(sanitized.begin(), sanitized.end(), '>', '&gt;');
    std::replace(sanitized.begin(), sanitized.end(), '"', '&quot;');
    std::replace(sanitized.begin(), sanitized.end(), '\'', '&#39;');
    
    return sanitized;
}

} // namespace llm
} // namespace forensics