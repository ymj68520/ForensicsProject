#include "LMStudioClient.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <regex>
#include <fstream>

namespace forensics {
namespace llm {

LMStudioClient::LMStudioClient(const std::string& base_url)
    : base_url_(base_url)
    , timeout_seconds_(30)
    , cache_enabled_(true)
    , cache_ttl_(300)
    , is_initialized_(false)
    , is_shutdown_(false) {
    
    stats_.start_time = std::chrono::system_clock::now();
    
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LMStudioClient::~LMStudioClient() {
    shutdown();
}

bool LMStudioClient::initialize() {
    try {
        // 检查连接
        if (!checkConnection()) {
            return false;
        }

        // 启动工作线程
        worker_thread_ = std::thread(&LMStudioClient::workerThreadFunc, this);
        
        is_initialized_ = true;
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool LMStudioClient::checkConnection() {
    try {
        auto response = httpGet("/health");
        return !response.empty();
    }
    catch (...) {
        return false;
    }
}

std::vector<ModelConfig> LMStudioClient::getAvailableModels() {
    try {
        std::string response = httpGet("/v1/models");
        if (response.empty()) {
            return {};
        }

        auto json_response = nlohmann::json::parse(response);
        std::vector<ModelConfig> models;

        if (json_response.contains("data") && json_response["data"].is_array()) {
            for (const auto& model_json : json_response["data"]) {
                ModelConfig config = ModelConfigParser::parseFromJson(model_json);
                if (ModelConfigParser::validateConfig(config)) {
                    models.push_back(config);
                }
            }
        }

        return models;
    }
    catch (const std::exception& e) {
        return {};
    }
}

bool LMStudioClient::loadModel(const std::string& model_id) {
    try {
        nlohmann::json request = {
            {"model", model_id}
        };

        std::string response = httpPost("/v1/load", request.dump());
        if (response.empty()) {
            return false;
        }

        auto json_response = nlohmann::json::parse(response);
        if (json_response.contains("success") && json_response["success"].get<bool>()) {
            current_model_ = model_id;
            return true;
        }

        return false;
    }
    catch (const std::exception& e) {
        return false;
    }
}

APIResponse LMStudioClient::chatCompletion(const std::string& prompt,
                                       const std::string& system_prompt,
                                       const std::string& model_id,
                                       double temperature,
                                       int max_tokens) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 检查缓存
        if (cache_enabled_) {
            std::string cache_key = generateCacheKey(prompt, system_prompt, 
                                                model_id.empty() ? current_model_ : model_id, 
                                                temperature);
            std::string cached_response = getCachedResponse(cache_key);
            if (!cached_response.empty()) {
                recordRequest(true, 0.001, true); // 缓存命中，响应时间极短
                return APIResponse::success(nlohmann::json::parse(cached_response));
            }
        }

        // 构建请求
        nlohmann::json request = {
            {"model", model_id.empty() ? current_model_ : model_id},
            {"messages", nlohmann::json::array({
                {{"role", "system"}, {"content", system_prompt}},
                {{"role", "user"}, {"content", prompt}}
            })},
            {"temperature", temperature},
            {"max_tokens", max_tokens},
            {"stream", false}
        };

        std::string response = httpPost("/v1/chat/completions", request.dump());
        if (response.empty()) {
            auto end_time = std::chrono::high_resolution_clock::now();
            double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count() / 1000.0;
            recordRequest(false, response_time);
            return APIResponse::error(APIError::CONNECTION_ERROR, "Empty response from server");
        }

        // 缓存响应
        if (cache_enabled_) {
            std::string cache_key = generateCacheKey(prompt, system_prompt,
                                                model_id.empty() ? current_model_ : model_id,
                                                temperature);
            cacheResponse(cache_key, response);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count() / 1000.0;
        recordRequest(true, response_time);

        return APIResponse::success(nlohmann::json::parse(response));
    }
    catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double response_time = std::chrono::duration<double, std::milli>(end_time - start_time).count() / 1000.0;
        recordRequest(false, response_time);
        return APIResponse::error(APIError::UNKNOWN_ERROR, e.what());
    }
}

void LMStudioClient::chatCompletionAsync(const std::string& prompt,
                                     std::function<void(const APIResponse&)> callback,
                                     const std::string& system_prompt,
                                     const std::string& model_id,
                                     double temperature,
                                     int max_tokens) {
    if (!is_initialized_) {
        callback(APIResponse::error(APIError::CONNECTION_ERROR, "Client not initialized"));
        return;
    }

    AITask task;
    task.id = generateTaskId();
    task.input_text = prompt;
    task.system_prompt = system_prompt;
    task.temperature = temperature;
    task.max_tokens = max_tokens;

    QueuedTask queued_task;
    queued_task.task = task;
    queued_task.callback = [callback, model_id, prompt, system_prompt, temperature, max_tokens, this](const AIResponse& response) {
        // 重新构造请求以获取完整响应
        auto api_response = this->chatCompletion(prompt, system_prompt, model_id, temperature, max_tokens);
        callback(api_response);
    };
    queued_task.retry_count = 0;
    queued_task.next_attempt = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(queued_task);
    }
    queue_cv_.notify_one();
}

void LMStudioClient::chatCompletionStream(const std::string& prompt,
                                       std::function<void(const std::string&)> stream_callback,
                                       const std::string& system_prompt,
                                       const std::string& model_id,
                                       double temperature,
                                       int max_tokens) {
    try {
        nlohmann::json request = {
            {"model", model_id.empty() ? current_model_ : model_id},
            {"messages", nlohmann::json::array({
                {{"role", "system"}, {"content", system_prompt}},
                {{"role", "user"}, {"content", prompt}}
            })},
            {"temperature", temperature},
            {"max_tokens", max_tokens},
            {"stream", true}
        };

        HttpRequest http_request;
        http_request.url = base_url_ + "/v1/chat/completions";
        http_request.method = "POST";
        http_request.headers = buildDefaultHeaders();
        http_request.headers["Content-Type"] = "application/json";
        http_request.body = request.dump();

        CURL* curl = curl_easy_init();
        if (!curl) {
            return;
        }

        // 设置流式回调
        struct StreamData {
            std::function<void(const std::string&)> callback;
            std::string buffer;
        } stream_data;
        stream_data.callback = stream_callback;

        curl_easy_setopt(curl, CURLOPT_URL, http_request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, http_request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStreamCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);

        // 设置请求头
        struct curl_slist* headers = nullptr;
        for (const auto& [key, value] : http_request.headers) {
            std::string header = key + ": " + value;
            headers = curl_slist_append(headers, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            // 错误处理
        }
    }
    catch (const std::exception& e) {
        // 错误处理
    }
}

APIResponse LMStudioClient::getModelInfo(const std::string& model_id) {
    try {
        std::string response = httpGet("/v1/models/" + model_id);
        if (response.empty()) {
            return APIResponse::error(APIError::MODEL_NOT_FOUND, "Model not found");
        }

        return APIResponse::success(nlohmann::json::parse(response));
    }
    catch (const std::exception& e) {
        return APIResponse::error(APIError::UNKNOWN_ERROR, e.what());
    }
}

std::string LMStudioClient::getCurrentModel() {
    return current_model_;
}

void LMStudioClient::setApiKey(const std::string& api_key) {
    api_key_ = api_key;
}

void LMStudioClient::setTimeout(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
}

void LMStudioClient::enableCache(bool enabled, int ttl_seconds) {
    cache_enabled_ = enabled;
    cache_ttl_ = ttl_seconds;
}

void LMStudioClient::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    response_cache_.clear();
}

nlohmann::json LMStudioClient::getStatistics() {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.start_time).count();

    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);

    return nlohmann::json{
        {"total_requests", stats_.total_requests},
        {"successful_requests", stats_.successful_requests},
        {"failed_requests", stats_.failed_requests},
        {"success_rate", stats_.total_requests > 0 ? 
            static_cast<double>(stats_.successful_requests) / stats_.total_requests : 0.0},
        {"average_response_time", stats_.successful_requests > 0 ? 
            stats_.total_response_time / stats_.successful_requests : 0.0},
        {"cache_hits", stats_.cache_hits},
        {"cache_misses", stats_.cache_misses},
        {"cache_hit_rate", (stats_.cache_hits + stats_.cache_misses) > 0 ?
            static_cast<double>(stats_.cache_hits) / (stats_.cache_hits + stats_.cache_misses) : 0.0},
        {"uptime_seconds", uptime},
        {"current_model", current_model_},
        {"cache_size", response_cache_.size()},
        {"queued_tasks", task_queue_.size()}
    };
}

void LMStudioClient::shutdown() {
    if (is_shutdown_) {
        return;
    }

    is_shutdown_ = true;
    queue_cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    curl_global_cleanup();
    is_initialized_ = false;
}

// 私有方法实现
std::string LMStudioClient::httpGet(const std::string& endpoint,
                                 const std::map<std::string, std::string>& headers) {
    HttpRequest request;
    request.url = base_url_ + endpoint;
    request.method = "GET";
    request.headers = headers;

    auto [response, status_code] = executeHttpRequest(request);
    
    if (status_code == 200) {
        return response;
    } else {
        return "";
    }
}

std::string LMStudioClient::httpPost(const std::string& endpoint,
                                  const std::string& data,
                                  const std::map<std::string, std::string>& headers) {
    HttpRequest request;
    request.url = base_url_ + endpoint;
    request.method = "POST";
    request.headers = headers;
    request.body = data;

    auto [response, status_code] = executeHttpRequest(request);
    
    if (status_code == 200) {
        return response;
    } else {
        return "";
    }
}

std::pair<std::string, long> LMStudioClient::executeHttpRequest(const HttpRequest& request) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {"", -1};
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);

    // 设置请求头
    struct curl_slist* headers = nullptr;
    for (const auto& [key, value] : request.headers) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // 设置POST数据
    if (request.method == "POST" && !request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
    }

    CURLcode res = curl_easy_perform(curl);
    
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        return {response, status_code};
    } else {
        return {"", -1};
    }
}

size_t LMStudioClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

size_t LMStudioClient::writeStreamCallback(void* contents, size_t size, size_t nmemb, void* user_data) {
    StreamData* stream_data = static_cast<StreamData*>(user_data);
    size_t total_size = size * nmemb;
    
    std::string chunk((char*)contents, total_size);
    std::string parsed_content = parseStreamChunk(chunk);
    
    if (!parsed_content.empty() && stream_data->callback) {
        stream_data->callback(parsed_content);
    }
    
    return total_size;
}

std::string LMStudioClient::generateCacheKey(const std::string& prompt,
                                          const std::string& system_prompt,
                                          const std::string& model_id,
                                          double temperature) {
    std::ostringstream oss;
    oss << prompt << "|" << system_prompt << "|" << model_id << "|" << std::fixed << std::setprecision(2) << temperature;
    
    // 简单的哈希（实际项目中应使用更好的哈希函数）
    std::hash<std::string> hasher;
    return std::to_string(hasher(oss.str()));
}

std::string LMStudioClient::getCachedResponse(const std::string& cache_key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = response_cache_.find(cache_key);
    if (it != response_cache_.end() && !it->second.is_expired()) {
        stats_.cache_hits++;
        it->second.access_count++;
        return it->second.response;
    }
    
    stats_.cache_misses++;
    return "";
}

void LMStudioClient::cacheResponse(const std::string& cache_key, const std::string& response) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    CacheItem item;
    item.response = response;
    item.cached_at = std::chrono::system_clock::now();
    item.ttl = std::chrono::seconds(cache_ttl_);
    item.access_count = 1;
    
    response_cache_[cache_key] = item;
    
    // 清理过期缓存
    cleanupExpiredCache();
}

void LMStudioClient::cleanupExpiredCache() {
    auto now = std::chrono::system_clock::now();
    auto it = response_cache_.begin();
    
    while (it != response_cache_.end()) {
        if (it->second.is_expired()) {
            it = response_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void LMStudioClient::workerThreadFunc() {
    while (!is_shutdown_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this] {
            return !task_queue_.empty() || is_shutdown_;
        });
        
        if (is_shutdown_) {
            break;
        }
        
        if (!task_queue_.empty()) {
            QueuedTask task = task_queue_.top();
            task_queue_.pop();
            lock.unlock();
            
            // 执行任务
            try {
                auto api_response = chatCompletion(
                    task.task.input_text,
                    task.task.system_prompt,
                    "", // model_id
                    task.task.temperature,
                    task.task.max_tokens
                );
                
                task.callback(AIResponse{});
            }
            catch (const std::exception& e) {
                // 重试逻辑
                if (task.retry_count < 3) {
                    task.retry_count++;
                    task.next_attempt = std::chrono::system_clock::now() + std::chrono::seconds(5);
                    
                    std::lock_guard<std::mutex> retry_lock(queue_mutex_);
                    task_queue_.push(task);
                }
            }
        }
    }
}

std::string LMStudioClient::parseStreamChunk(const std::string& chunk) {
    // 解析SSE格式
    std::regex data_regex(R"(data:\s*(.*?)(?=\n\n|$))");
    std::sregex_iterator iter(chunk.begin(), chunk.end(), data_regex);
    std::sregex_iterator end;
    
    std::string content;
    for (; iter != end; ++iter) {
        std::string data = iter->str();
        if (data.find("data: [DONE]") != std::string::npos) {
            break;
        }
        
        try {
            size_t pos = data.find("data: ");
            if (pos != std::string::npos) {
                std::string json_str = data.substr(pos + 6);
                auto json_obj = nlohmann::json::parse(json_str);
                
                if (json_obj.contains("choices") && 
                    json_obj["choices"].is_array() && 
                    !json_obj["choices"].empty()) {
                    
                    auto choice = json_obj["choices"][0];
                    if (choice.contains("delta") && 
                        choice["delta"].contains("content")) {
                        
                        content += choice["delta"]["content"].get<std::string>();
                    }
                }
            }
        }
        catch (...) {
            // 忽略解析错误
        }
    }
    
    return content;
}

std::map<std::string, std::string> LMStudioClient::buildDefaultHeaders() {
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    
    if (!api_key_.empty()) {
        headers["Authorization"] = "Bearer " + api_key_;
    }
    
    return headers;
}

void LMStudioClient::recordRequest(bool success, double response_time, bool from_cache) {
    stats_.total_requests++;
    
    if (success) {
        stats_.successful_requests++;
        if (!from_cache) {
            stats_.total_response_time += response_time;
        }
    } else {
        stats_.failed_requests++;
    }
}

std::string LMStudioClient::generateTaskId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "task_" + std::to_string(timestamp);
}

// ModelConfigParser 实现
ModelConfig ModelConfigParser::parseFromJson(const nlohmann::json& json) {
    ModelConfig config;
    
    if (json.contains("id")) {
        config.model_id = json["id"].get<std::string>();
        config.name = config.model_id;
    }
    
    if (json.contains("name")) {
        config.name = json["name"].get<std::string>();
    }
    
    if (json.contains("max_length")) {
        config.context_length = json["max_length"].get<int>();
    }
    
    if (json.contains("context_length")) {
        config.context_length = json["context_length"].get<int>();
    }
    
    // 设置默认值
    if (config.context_length == 0) {
        config.context_length = 4096;
    }
    
    config.max_tokens = config.context_length;
    config.default_temperature = 0.3;
    config.inference_speed = 10.0; // 默认值
    config.memory_usage = 4.0; // 默认4GB
    config.is_multimodal = false; // 默认非多模态
    
    // 解析能力
    if (json.contains("capabilities")) {
        config.capabilities = parseCapabilities(json["capabilities"]);
    }
    
    config.description = "LM Studio模型";
    
    return config;
}

nlohmann::json ModelConfigParser::toJson(const ModelConfig& config) {
    nlohmann::json json;
    
    json["name"] = config.name;
    json["model_id"] = config.model_id;
    json["api_endpoint"] = config.api_endpoint;
    json["max_tokens"] = config.max_tokens;
    json["context_length"] = config.context_length;
    json["default_temperature"] = config.default_temperature;
    json["inference_speed"] = config.inference_speed;
    json["memory_usage"] = config.memory_usage;
    json["is_multimodal"] = config.is_multimodal;
    json["supported_languages"] = config.supported_languages;
    json["description"] = config.description;
    
    nlohmann::json capabilities_json;
    for (const auto& capability : config.capabilities) {
        capabilities_json.push_back(capabilityToString(capability));
    }
    json["capabilities"] = capabilities_json;
    
    return json;
}

bool ModelConfigParser::validateConfig(const ModelConfig& config) {
    return !config.model_id.empty() && 
           !config.name.empty() && 
           config.context_length > 0 && 
           config.max_tokens > 0;
}

std::vector<ModelCapability> ModelConfigParser::parseCapabilities(const nlohmann::json& capabilities_json) {
    std::vector<ModelCapability> capabilities;
    
    if (capabilities_json.is_array()) {
        for (const auto& cap_str : capabilities_json) {
            ModelCapability capability = stringToCapability(cap_str.get<std::string>());
            if (capability != ModelCapability::TEXT_PROCESSING) { // 假设TEXT_PROCESSING是默认值
                capabilities.push_back(capability);
            }
        }
    }
    
    return capabilities;
}

std::string ModelConfigParser::capabilityToString(ModelCapability capability) {
    switch (capability) {
        case ModelCapability::TEXT_PROCESSING: return "TEXT_PROCESSING";
        case ModelCapability::IMAGE_PROCESSING: return "IMAGE_PROCESSING";
        case ModelCapability::MULTIMODAL: return "MULTIMODAL";
        case ModelCapability::CODE_UNDERSTANDING: return "CODE_UNDERSTANDING";
        case ModelCapability::REASONING: return "REASONING";
        case ModelCapability::ANALYSIS: return "ANALYSIS";
        case ModelCapability::SUMMARIZATION: return "SUMMARIZATION";
        case ModelCapability::TRANSLATION: return "TRANSLATION";
        case ModelCapability::CHINESE_PROCESSING: return "CHINESE_PROCESSING";
        case ModelCapability::ENGLISH_PROCESSING: return "ENGLISH_PROCESSING";
        case ModelCapability::LONG_CONTEXT: return "LONG_CONTEXT";
        case ModelCapability::FAST_INFERENCE: return "FAST_INFERENCE";
        default: return "UNKNOWN";
    }
}

ModelCapability ModelConfigParser::stringToCapability(const std::string& capability_str) {
    std::map<std::string, ModelCapability> capability_map = {
        {"TEXT_PROCESSING", ModelCapability::TEXT_PROCESSING},
        {"IMAGE_PROCESSING", ModelCapability::IMAGE_PROCESSING},
        {"MULTIMODAL", ModelCapability::MULTIMODAL},
        {"CODE_UNDERSTANDING", ModelCapability::CODE_UNDERSTANDING},
        {"REASONING", ModelCapability::REASONING},
        {"ANALYSIS", ModelCapability::ANALYSIS},
        {"SUMMARIZATION", ModelCapability::SUMMARIZATION},
        {"TRANSLATION", ModelCapability::TRANSLATION},
        {"CHINESE_PROCESSING", ModelCapability::CHINESE_PROCESSING},
        {"ENGLISH_PROCESSING", ModelCapability::ENGLISH_PROCESSING},
        {"LONG_CONTEXT", ModelCapability::LONG_CONTEXT},
        {"FAST_INFERENCE", ModelCapability::FAST_INFERENCE}
    };
    
    auto it = capability_map.find(capability_str);
    return (it != capability_map.end()) ? it->second : ModelCapability::TEXT_PROCESSING;
}

// APIResponse 静态方法实现
APIResponse APIResponse::success(const nlohmann::json& data) {
    APIResponse response;
    response.error_code = APIError::SUCCESS;
    response.error_message = "";
    response.data = data;
    response.timestamp = std::chrono::system_clock::now();
    return response;
}

APIResponse APIResponse::error(APIError code, const std::string& message) {
    APIResponse response;
    response.error_code = code;
    response.error_message = message;
    response.timestamp = std::chrono::system_clock::now();
    return response;
}

} // namespace llm
} // namespace forensics