#pragma once

#include "LLMIntegrationDataTypes.h"
#include "ModelManager.h"
#include "AIAnalysisService.h"
#include "HTTPserver.h"
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <functional>

namespace forensics {
namespace llm {

/**
 * @brief LLM集成HTTP服务器
 * 
 * 提供RESTful API接口，集成大模型功能到现有HTTP服务中
 * 
 * API端点：
 * POST /api/llm/analyze/evidence - 分析单个证据
 * POST /api/llm/analyze/batch - 批量分析证据
 * POST /api/llm/analyze/conversation - 分析对话
 * POST /api/llm/generate/report - 生成报告
 * POST /api/llm/detect/anomalies - 异常检测
 * POST /api/llm/recognize/patterns - 模式识别
 * POST /api/llm/analyze/sentiment - 情感分析
 * POST /api/llm/recognize/entities - 实体识别
 * POST /api/llm/analyze/multimodal - 多模态分析
 * GET  /api/llm/models - 获取可用模型
 * GET  /api/llm/models/{id} - 获取模型信息
 * POST /api/llm/models/{id}/load - 加载模型
 * GET  /api/llm/status - 获取系统状态
 * POST /api/llm/classify/task - 任务分类
 */
class LLMHttpServer {
public:
    explicit LLMHttpServer(const std::string& lm_studio_url = "http://localhost:1234",
                         int port = 8080);
    ~LLMHttpServer();

    /**
     * @brief 初始化服务器
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 启动HTTP服务器
     * @return 是否成功
     */
    bool start();

    /**
     * @brief 停止HTTP服务器
     */
    void stop();

    /**
     * @brief 设置模型管理器
     * @param model_manager 模型管理器
     */
    void setModelManager(std::shared_ptr<ModelManager> model_manager);

    /**
     * @brief 设置AI分析服务
     * @param analysis_service AI分析服务
     */
    void setAnalysisService(std::shared_ptr<AIAnalysisService> analysis_service);

    /**
     * @brief 获取服务器状态
     * @return 状态信息
     */
    nlohmann::json getServerStatus();

    /**
     * @brief 设置API前缀
     * @param prefix API前缀
     */
    void setApiPrefix(const std::string& prefix);

    /**
     * @brief 启用CORS
     * @param enabled 是否启用
     * @param allowed_origins 允许的源
     */
    void enableCORS(bool enabled, const std::vector<std::string>& allowed_origins = {"*"});

    /**
     * @brief 设置认证中间件
     * @param auth_callback 认证回调函数
     */
    void setAuthMiddleware(std::function<bool(const std::string&)> auth_callback);

private:
    std::shared_ptr<ModelManager> model_manager_;
    std::shared_ptr<AIAnalysisService> analysis_service_;
    std::unique_ptr<HTTPserver> http_server_;
    std::string lm_studio_url_;
    int port_;
    std::string api_prefix_;
    bool cors_enabled_;
    std::vector<std::string> allowed_origins_;
    std::function<bool(const std::string&)> auth_callback_;
    std::mutex server_mutex_;
    bool is_running_;

    /**
     * @brief 注册所有API路由
     */
    void registerRoutes();

    /**
     * @brief 设置CORS头
     * @param response HTTP响应
     */
    void setCORSHeaders(std::string& response);

    /**
     * @brief 验证认证
     * @param request HTTP请求
     * @return 是否认证通过
     */
    bool authenticate(const std::string& request);

    /**
     * @brief 构建错误响应
     * @param error_code 错误代码
     * @param error_message 错误消息
     * @return JSON错误响应
     */
    nlohmann::json buildErrorResponse(const std::string& error_code, 
                                   const std::string& error_message);

    /**
     * @brief 构建成功响应
     * @param data 响应数据
     * @return JSON成功响应
     */
    nlohmann::json buildSuccessResponse(const nlohmann::json& data);

    /**
     * @brief 处理异步任务响应
     * @param callback 原始回调
     * @return 包装后的回调
     */
    template<typename T>
    std::function<void(const T&)> wrapAsyncCallback(std::function<void(const nlohmann::json&)> callback);

    // API处理函数
    void handleAnalyzeEvidence(const std::string& request, std::string& response);
    void handleBatchAnalysis(const std::string& request, std::string& response);
    void handleConversationAnalysis(const std::string& request, std::string& response);
    void handleReportGeneration(const std::string& request, std::string& response);
    void handleAnomalyDetection(const std::string& request, std::string& response);
    void handlePatternRecognition(const std::string& request, std::string& response);
    void handleSentimentAnalysis(const std::string& request, std::string& response);
    void handleEntityRecognition(const std::string& request, std::string& response);
    void handleMultiModalAnalysis(const std::string& request, std::string& response);
    void handleGetModels(const std::string& request, std::string& response);
    void handleGetModelInfo(const std::string& request, std::string& response);
    void handleLoadModel(const std::string& request, std::string& response);
    void handleGetStatus(const std::string& request, std::string& response);
    void handleTaskClassification(const std::string& request, std::string& response);

    /**
     * @brief 解析JSON请求体
     * @param request HTTP请求
     * @return 解析后的JSON对象
     */
    nlohmann::json parseJsonRequest(const std::string& request);

    /**
     * @brief 提取路径参数
     * @param path URL路径
     * @param param_name 参数名
     * @return 参数值
     */
    std::string extractPathParameter(const std::string& path, const std::string& param_name);

    /**
     * @brief 验证必需字段
     * @param json JSON对象
     * @param required_fields 必需字段列表
     * @return 验证结果
     */
    std::pair<bool, std::string> validateRequiredFields(const nlohmann::json& json,
                                                       const std::vector<std::string>& required_fields);

    /**
     * @brief 转换EvidenceAnalysisResult为JSON
     * @param result 分析结果
     * @return JSON对象
     */
    nlohmann::json evidenceAnalysisResultToJson(const EvidenceAnalysisResult& result);

    /**
     * @brief 转换ConversationAnalysisResult为JSON
     * @param result 对话分析结果
     * @return JSON对象
     */
    nlohmann::json conversationAnalysisResultToJson(const ConversationAnalysisResult& result);

    /**
     * @brief 转换ReportGenerationResult为JSON
     * @param result 报告生成结果
     * @return JSON对象
     */
    nlohmann::json reportGenerationResultToJson(const ReportGenerationResult& result);

    /**
     * @brief 转换MultiModalResult为JSON
     * @param result 多模态分析结果
     * @return JSON对象
     */
    nlohmann::json multiModalResultToJson(const MultiModalResult& result);
};

/**
 * @brief API请求验证器
 * 
 * 提供请求验证和安全检查功能
 */
class APIRequestValidator {
public:
    /**
     * @brief 验证请求大小
     * @param content_length 内容长度
     * @param max_size 最大允许大小
     * @return 是否有效
     */
    static bool validateRequestSize(size_t content_length, size_t max_size = 10 * 1024 * 1024);

    /**
     * @brief 验证JSON格式
     * @param json_string JSON字符串
     * @return 是否有效
     */
    static bool validateJson(const std::string& json_string);

    /**
     * @brief 验证文件路径安全性
     * @param file_path 文件路径
     * @return 是否安全
     */
    static bool validateFilePath(const std::string& file_path);

    /**
     * @brief 验证输入文本安全性
     * @param text 输入文本
     * @return 是否安全
     */
    static bool validateInputText(const std::string& text);

    /**
     * @brief 验证参数范围
     * @param value 参数值
     * @param min_value 最小值
     * @param max_value 最大值
     * @return 是否有效
     */
    static bool validateParameterRange(double value, double min_value, double max_value);

    /**
     * @brief 清理和转义输入
     * @param input 原始输入
     * @return 清理后的输入
     */
    static std::string sanitizeInput(const std::string& input);

private:
    /**
     * @brief 检查SQL注入模式
     * @param text 输入文本
     * @return 是否包含SQL注入
     */
    static bool containsSqlInjection(const std::string& text);

    /**
     * @brief 检查XSS模式
     * @param text 输入文本
     * @return 是否包含XSS
     */
    static bool containsXss(const std::string& text);

    /**
     * @brief 检查路径遍历攻击
     * @param path 文件路径
     * @return 是否包含路径遍历
     */
    static bool containsPathTraversal(const std::string& path);
};

/**
 * @brief API响应构建器
 * 
 * 提供统一的响应格式构建功能
 */
class APIResponseBuilder {
public:
    /**
     * @brief 构建成功响应
     * @param data 响应数据
     * @param message 响应消息
     * @param timestamp 时间戳
     * @return JSON响应
     */
    static nlohmann::json buildSuccess(const nlohmann::json& data = nullptr,
                                     const std::string& message = "Success",
                                     const std::chrono::system_clock::time_point& timestamp = std::chrono::system_clock::now());

    /**
     * @brief 构建错误响应
     * @param error_code 错误代码
     * @param error_message 错误消息
     * @param details 错误详情
     * @param timestamp 时间戳
     * @return JSON响应
     */
    static nlohmann::json buildError(const std::string& error_code,
                                    const std::string& error_message,
                                    const nlohmann::json& details = nullptr,
                                    const std::chrono::system_clock::time_point& timestamp = std::chrono::system_clock::now());

    /**
     * @brief 构建分页响应
     * @param data 数据数组
     * @param page 当前页码
     * @param page_size 页面大小
     * @param total_count 总记录数
     * @return JSON响应
     */
    static nlohmann::json buildPaginated(const nlohmann::json& data,
                                        int page,
                                        int page_size,
                                        int total_count);

    /**
     * @brief 构建异步任务响应
     * @param task_id 任务ID
     * @param status 任务状态
     * @param message 状态消息
     * @return JSON响应
     */
    static nlohmann::json buildAsyncTask(const std::string& task_id,
                                        const std::string& status,
                                        const std::string& message);

private:
    /**
     * @brief 格式化时间戳
     * @param timestamp 时间点
     * @return ISO格式时间字符串
     */
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp);
};

} // namespace llm
} // namespace forensics