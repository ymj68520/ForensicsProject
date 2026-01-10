#pragma once

#include "LLMIntegrationDataTypes.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace forensics {
namespace llm {

/**
 * @brief LM Studio HTTP客户端
 * 
 * 提供与LM Studio API的完整交互功能，支持：
 * - 多模型管理
 * - 异步请求处理
 * - 自动重试机制
 * - 响应缓存
 * - 流式响应处理
 */
class LMStudioClient {
public:
    explicit LMStudioClient(const std::string& base_url = "http://localhost:1234");
    ~LMStudioClient();

    /**
     * @brief 初始化客户端
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 检查连接状态
     * @return 是否连接成功
     */
    bool checkConnection();

    /**
     * @brief 获取可用模型列表
     * @return 模型列表
     */
    std::vector<ModelConfig> getAvailableModels();

    /**
     * @brief 加载指定模型
     * @param model_id 模型ID
     * @return 是否成功
     */
    bool loadModel(const std::string& model_id);

    /**
     * @brief 发送聊天完成请求
     * @param prompt 用户提示词
     * @param system_prompt 系统提示词
     * @param model_id 模型ID（可选）
     * @param temperature 温度参数
     * @param max_tokens 最大token数
     * @return 响应结果
     */
    APIResponse chatCompletion(const std::string& prompt,
                           const std::string& system_prompt = "",
                           const std::string& model_id = "",
                           double temperature = 0.3,
                           int max_tokens = 2048);

    /**
     * @brief 异步发送聊天完成请求
     * @param prompt 用户提示词
     * @param callback 响应回调函数
     * @param system_prompt 系统提示词
     * @param model_id 模型ID
     * @param temperature 温度参数
     * @param max_tokens 最大token数
     */
    void chatCompletionAsync(const std::string& prompt,
                          std::function<void(const APIResponse&)> callback,
                          const std::string& system_prompt = "",
                          const std::string& model_id = "",
                          double temperature = 0.3,
                          int max_tokens = 2048);

    /**
     * @brief 流式聊天完成
     * @param prompt 用户提示词
     * @param stream_callback 流式回调函数
     * @param system_prompt 系统提示词
     * @param model_id 模型ID
     * @param temperature 温度参数
     * @param max_tokens 最大token数
     */
    void chatCompletionStream(const std::string& prompt,
                          std::function<void(const std::string&)> stream_callback,
                          const std::string& system_prompt = "",
                          const std::string& model_id = "",
                          double temperature = 0.3,
                          int max_tokens = 2048);

    /**
     * @brief 获取模型信息
     * @param model_id 模型ID
     * @return 模型信息
     */
    APIResponse getModelInfo(const std::string& model_id);

    /**
     * @brief 获取当前加载的模型
     * @return 当前模型ID
     */
    std::string getCurrentModel();

    /**
     * @brief 设置API密钥（如果需要）
     * @param api_key API密钥
     */
    void setApiKey(const std::string& api_key);

    /**
     * @brief 设置请求超时时间
     * @param timeout_seconds 超时时间（秒）
     */
    void setTimeout(int timeout_seconds);

    /**
     * @brief 启用/禁用响应缓存
     * @param enabled 是否启用
     * @param ttl_seconds 缓存生存时间
     */
    void enableCache(bool enabled, int ttl_seconds = 300);

    /**
     * @brief 清除缓存
     */
    void clearCache();

    /**
     * @brief 获取客户端统计信息
     * @return 统计信息
     */
    nlohmann::json getStatistics();

    /**
     * @brief 关闭客户端
     */
    void shutdown();

private:
    std::string base_url_;
    std::string api_key_;
    std::string current_model_;
    int timeout_seconds_;
    bool cache_enabled_;
    int cache_ttl_;
    bool is_initialized_;
    bool is_shutdown_;

    // 异步处理相关
    std::thread worker_thread_;
    std::queue<QueuedTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 缓存相关
    std::map<std::string, CacheItem> response_cache_;
    std::mutex cache_mutex_;

    // 统计信息
    struct Statistics {
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        double total_response_time = 0.0;
        int cache_hits = 0;
        int cache_misses = 0;
        std::chrono::system_clock::time_point start_time;
    } stats_;

    // HTTP请求结构
    struct HttpRequest {
        std::string url;
        std::string method;
        std::map<std::string, std::string> headers;
        std::string body;
        std::function<void(const std::string&)> callback;
    };

    /**
     * @brief HTTP GET请求
     * @param endpoint API端点
     * @param headers 请求头
     * @return 响应内容
     */
    std::string httpGet(const std::string& endpoint,
                      const std::map<std::string, std::string>& headers = {});

    /**
     * @brief HTTP POST请求
     * @param endpoint API端点
     * @param data 请求数据
     * @param headers 请求头
     * @return 响应内容
     */
    std::string httpPost(const std::string& endpoint,
                       const std::string& data,
                       const std::map<std::string, std::string>& headers = {});

    /**
     * @brief 执行HTTP请求
     * @param request HTTP请求
     * @return 响应内容和状态码
     */
    std::pair<std::string, long> executeHttpRequest(const HttpRequest& request);

    /**
     * @brief CURL写入回调函数
     */
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* response);

    /**
     * @brief 流式CURL写入回调函数
     */
    static size_t writeStreamCallback(void* contents, size_t size, size_t nmemb, void* user_data);

    /**
     * @brief 生成缓存键
     * @param prompt 提示词
     * @param system_prompt 系统提示词
     * @param model_id 模型ID
     * @param temperature 温度
     * @return 缓存键
     */
    std::string generateCacheKey(const std::string& prompt,
                            const std::string& system_prompt,
                            const std::string& model_id,
                            double temperature);

    /**
     * @brief 从缓存获取响应
     * @param cache_key 缓存键
     * @return 缓存的响应（如果存在且未过期）
     */
    std::string getCachedResponse(const std::string& cache_key);

    /**
     * @brief 缓存响应
     * @param cache_key 缓存键
     * @param response 响应内容
     */
    void cacheResponse(const std::string& cache_key, const std::string& response);

    /**
     * @brief 清理过期缓存
     */
    void cleanupExpiredCache();

    /**
     * @brief 异步工作线程函数
     */
    void workerThreadFunc();

    /**
     * @brief 解析聊天完成响应
     * @param response JSON响应
     * @return 解析后的响应对象
     */
    AIResponse parseChatCompletionResponse(const nlohmann::json& response);

    /**
     * @brief 解析流式响应块
     * @param chunk SSE数据块
     * @return 解析后的内容
     */
    std::string parseStreamChunk(const std::string& chunk);

    /**
     * @brief 构建默认请求头
     * @return 请求头映射
     */
    std::map<std::string, std::string> buildDefaultHeaders();

    /**
     * @brief 验证响应格式
     * @param response 响应内容
     * @return 是否有效
     */
    bool validateResponse(const std::string& response);

    /**
     * @brief 记录请求统计
     * @param success 是否成功
     * @param response_time 响应时间
     * @param from_cache 是否来自缓存
     */
    void recordRequest(bool success, double response_time, bool from_cache = false);

    /**
     * @brief 重试请求
     * @param request_func 请求函数
     * @param max_retries 最大重试次数
     * @return 请求结果
     */
    template<typename Func>
    auto retryRequest(Func request_func, int max_retries = 3) -> decltype(request_func());

    /**
     * @brief 处理错误响应
     * @param status_code HTTP状态码
     * @param response 响应内容
     * @return API错误对象
     */
    APIResponse handleError(long status_code, const std::string& response);
};

/**
 * @brief 模型配置解析器
 * 
 * 解析和管理LM Studio中模型的配置信息
 */
class ModelConfigParser {
public:
    /**
     * @brief 从JSON解析模型配置
     * @param json JSON对象
     * @return 模型配置
     */
    static ModelConfig parseFromJson(const nlohmann::json& json);

    /**
     * @brief 将模型配置转换为JSON
     * @param config 模型配置
     * @return JSON对象
     */
    static nlohmann::json toJson(const ModelConfig& config);

    /**
     * @brief 从文件加载模型配置
     * @param file_path 文件路径
     * @return 模型配置列表
     */
    static std::vector<ModelConfig> loadFromFile(const std::string& file_path);

    /**
     * @brief 保存模型配置到文件
     * @param configs 模型配置列表
     * @param file_path 文件路径
     * @return 是否成功
     */
    static bool saveToFile(const std::vector<ModelConfig>& configs, const std::string& file_path);

    /**
     * @brief 验证模型配置
     * @param config 模型配置
     * @return 验证结果
     */
    static bool validateConfig(const ModelConfig& config);

private:
    /**
     * @brief 解析模型能力
     * @param capabilities_json 能力JSON数组
     * @return 能力列表
     */
    static std::vector<ModelCapability> parseCapabilities(const nlohmann::json& capabilities_json);

    /**
     * @brief 能力枚举转字符串
     * @param capability 能力枚举
     * @return 字符串
     */
    static std::string capabilityToString(ModelCapability capability);

    /**
     * @brief 字符串转能力枚举
     * @param capability_str 字符串
     * @return 能力枚举
     */
    static ModelCapability stringToCapability(const std::string& capability_str);
};

} // namespace llm
} // namespace forensics