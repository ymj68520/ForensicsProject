#pragma once

#include "LLMIntegrationDataTypes.h"
#include "LMStudioClient.h"
#include "TaskClassifier.h"
#include <memory>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace forensics {
namespace llm {

/**
 * @brief 智能模型管理器
 * 
 * 核心功能：
 * 1. 自动选择最适合的模型处理任务
 * 2. 管理模型生命周期
 * 3. 监控模型性能
 * 4. 负载均衡和资源调度
 * 5. 模型热切换和故障转移
 */
class ModelManager {
public:
    explicit ModelManager(const std::string& lm_studio_url = "http://localhost:1234");
    ~ModelManager();

    /**
     * @brief 初始化模型管理器
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 关闭模型管理器
     */
    void shutdown();

    /**
     * @brief 扫描并加载可用模型
     * @return 成功加载的模型数量
     */
    int scanAndLoadModels();

    /**
     * @brief 智能选择模型处理任务
     * @param task AI任务
     * @return 选择的模型配置和响应
     */
    std::pair<ModelConfig, AIResponse> processTask(const AITask& task);

    /**
     * @brief 异步处理任务
     * @param task AI任务
     * @param callback 完成回调
     */
    void processTaskAsync(const AITask& task, 
                         std::function<void(const AIResponse&)> callback);

    /**
     * @brief 批量处理任务
     * @param tasks 任务列表
     * @return 响应列表
     */
    std::vector<AIResponse> processBatch(const std::vector<AITask>& tasks);

    /**
     * @brief 获取当前可用的模型
     * @return 模型配置列表
     */
    std::vector<ModelConfig> getAvailableModels();

    /**
     * @brief 获取模型性能统计
     * @param model_id 模型ID
     * @return 性能指标
     */
    ModelPerformance getModelPerformance(const std::string& model_id);

    /**
     * @brief 获取所有模型的性能统计
     * @return 模型性能映射
     */
    std::map<std::string, ModelPerformance> getAllModelPerformance();

    /**
     * @brief 手动加载模型
     * @param model_id 模型ID
     * @return 是否成功
     */
    bool loadModel(const std::string& model_id);

    /**
     * @brief 卸载模型
     * @param model_id 模型ID
     * @return 是否成功
     */
    bool unloadModel(const std::string& model_id);

    /**
     * @brief 设置首选模型
     * @param model_id 模型ID
     * @param task_type 任务类型（可选，设置为所有任务的首选）
     */
    void setPreferredModel(const std::string& model_id, TaskType task_type = TaskType::UNKNOWN);

    /**
     * @brief 获取当前加载的模型
     * @return 当前模型ID
     */
    std::string getCurrentModel();

    /**
     * @brief 获取系统状态
     * @return LLM系统状态
     */
    LLMSystemStatus getSystemStatus();

    /**
     * @brief 启用/禁用自动模型选择
     * @param enabled 是否启用
     */
    void setAutoModelSelection(bool enabled);

    /**
     * @brief 设置模型选择策略
     * @param strategy 策略名称
     */
    void setSelectionStrategy(const std::string& strategy);

    /**
     * @brief 更新模型配置
     * @param model_id 模型ID
     * @param config 新配置
     * @return 是否成功
     */
    bool updateModelConfig(const std::string& model_id, const ModelConfig& config);

    /**
     * @brief 导出模型配置
     * @param file_path 导出路径
     * @return 是否成功
     */
    bool exportModelConfigs(const std::string& file_path);

    /**
     * @brief 导入模型配置
     * @param file_path 导入路径
     * @return 是否成功
     */
    bool importModelConfigs(const std::string& file_path);

private:
    std::unique_ptr<LMStudioClient> client_;
    std::unique_ptr<TaskClassifier> classifier_;
    std::unique_ptr<IntelligentTaskClassifier> intelligent_classifier_;

    // 模型管理
    std::map<std::string, ModelConfig> available_models_;
    std::map<std::string, ModelPerformance> model_performance_;
    std::string current_model_;
    std::map<TaskType, std::string> preferred_models_;
    
    // 任务处理
    std::queue<QueuedTask> task_queue_;
    std::thread worker_thread_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> is_shutdown_;
    
    // 配置
    bool auto_model_selection_;
    std::string selection_strategy_;
    int max_concurrent_tasks_;
    int current_task_count_;
    
    // 统计
    struct Statistics {
        int total_tasks_processed = 0;
        int successful_tasks = 0;
        int failed_tasks = 0;
        double avg_processing_time = 0.0;
        std::chrono::system_clock::time_point start_time;
        std::map<TaskType, int> task_type_counts;
        std::map<std::string, int> model_usage_counts;
    } stats_;
    
    std::mutex stats_mutex_;

    /**
     * @brief 模型选择策略枚举
     */
    enum class SelectionStrategy {
        PERFORMANCE_BASED,    // 基于性能
        CAPABILITY_BASED,    // 基于能力匹配
        LOAD_BALANCED,       // 负载均衡
        COST_OPTIMIZED,      // 成本优化
        QUALITY_FOCUSED      // 质量优先
    };

    /**
     * @brief 初始化默认模型配置
     */
    void initializeDefaultModels();

    /**
     * @brief 根据任务选择最佳模型
     * @param task AI任务
     * @param classification 任务分类结果
     * @return 选择的模型配置
     */
    ModelConfig selectBestModel(const AITask& task, const TaskClassification& classification);

    /**
     * @brief 基于性能选择模型
     * @param candidates 候选模型列表
     * @param task 任务对象
     * @return 最佳模型
     */
    ModelConfig selectByPerformance(const std::vector<ModelConfig>& candidates, const AITask& task);

    /**
     * @brief 基于能力匹配选择模型
     * @param candidates 候选模型列表
     * @param required_capabilities 所需能力
     * @return 最佳模型
     */
    ModelConfig selectByCapability(const std::vector<ModelConfig>& candidates, 
                                 const std::vector<ModelCapability>& required_capabilities);

    /**
     * @brief 基于负载均衡选择模型
     * @param candidates 候选模型列表
     * @return 最佳模型
     */
    ModelConfig selectByLoadBalancing(const std::vector<ModelConfig>& candidates);

    /**
     * @brief 检查模型是否支持任务
     * @param model 模型配置
     * @param task 任务对象
     * @return 是否支持
     */
    bool isModelCapable(const ModelConfig& model, const AITask& task);

    /**
     * @brief 计算模型匹配分数
     * @param model 模型配置
     * @param task 任务对象
     * @param classification 任务分类
     * @return 匹配分数 (0-1)
     */
    double calculateModelScore(const ModelConfig& model, const AITask& task, 
                            const TaskClassification& classification);

    /**
     * @brief 更新模型性能统计
     * @param model_id 模型ID
     * @param success 是否成功
     * @param response_time 响应时间
     * @param confidence 置信度
     */
    void updateModelPerformance(const std::string& model_id, bool success, 
                              double response_time, double confidence);

    /**
     * @brief 工作线程函数
     */
    void workerThreadFunc();

    /**
     * @brief 执行单个任务
     * @param task 任务对象
     * @return AI响应
     */
    AIResponse executeTask(const AITask& task);

    /**
     * @brief 构建任务特定的提示词
     * @param task 任务对象
     * @param classification 任务分类
     * @return 系统提示词和用户提示词对
     */
    std::pair<std::string, std::string> buildTaskPrompts(const AITask& task, 
                                                         const TaskClassification& classification);

    /**
     * @brief 解析模型响应
     * @param task 原始任务
     * @param model 使用的模型
     * @param raw_response 原始响应
     * @return 解析后的AI响应
     */
    AIResponse parseModelResponse(const AITask& task, const ModelConfig& model, 
                               const std::string& raw_response);

    /**
     * @brief 执行模型故障转移
     * @param failed_model 失败的模型ID
     * @param task 任务对象
     * @return 新选择的模型
     */
    ModelConfig performFailover(const std::string& failed_model, const AITask& task);

    /**
     * @brief 监控模型健康状态
     */
    void monitorModelHealth();

    /**
     * @brief 定期清理过期的统计数据
     */
    void cleanupExpiredStats();

    /**
     * @brief 获取选择策略枚举
     * @param strategy_name 策略名称
     * @return 策略枚举
     */
    SelectionStrategy getStrategy(const std::string& strategy_name);

    /**
     * @brief 任务所需的能力
     * @param task 任务对象
     * @param classification 任务分类
     * @return 能力列表
     */
    std::vector<ModelCapability> getRequiredCapabilities(const AITask& task, 
                                                   const TaskClassification& classification);

    /**
     * @brief 验证模型配置
     * @param config 模型配置
     * @return 验证结果
     */
    bool validateModelConfig(const ModelConfig& config);

    /**
     * @brief 更新全局统计
     * @param task 任务对象
     * @param response 响应结果
     * @param processing_time 处理时间
     */
    void updateGlobalStatistics(const AITask& task, const AIResponse& response, 
                             double processing_time);

    /**
     * @brief 生成任务ID
     * @return 唯一的任务ID
     */
    std::string generateTaskId();

    /**
     * @brief 检查模型是否可用
     * @param model_id 模型ID
     * @return 是否可用
     */
    bool isModelAvailable(const std::string& model_id);

public:
    // 静态工具方法
    static std::string selectionStrategyToString(SelectionStrategy strategy);
    static SelectionStrategy stringToSelectionStrategy(const std::string& strategy);
    static nlohmann::json modelPerformanceToJson(const ModelPerformance& performance);
    static ModelPerformance jsonToModelPerformance(const nlohmann::json& json);
};

/**
 * @brief 模型池管理器
 * 
 * 管理多个LM Studio实例，实现高可用和负载分散
 */
class ModelPoolManager {
public:
    struct PoolNode {
        std::string id;
        std::string url;
        std::unique_ptr<ModelManager> manager;
        bool is_active;
        double weight;  // 负载权重
        int current_load;
    };

    explicit ModelPoolManager();
    ~ModelPoolManager();

    /**
     * @brief 添加模型节点到池中
     * @param node 节点配置
     * @return 是否成功
     */
    bool addNode(const PoolNode& node);

    /**
     * @brief 从池中移除节点
     * @param node_id 节点ID
     * @return 是否成功
     */
    bool removeNode(const std::string& node_id);

    /**
     * @brief 选择最佳节点处理任务
     * @param task AI任务
     * @return 选择的节点和响应
     */
    std::pair<PoolNode*, AIResponse> processTask(const AITask& task);

    /**
     * @brief 获取池状态
     * @return 池状态信息
     */
    nlohmann::json getPoolStatus();

    /**
     * @brief 启用健康检查
     * @param interval_seconds 检查间隔
     */
    void enableHealthCheck(int interval_seconds = 30);

    /**
     * @brief 停止健康检查
     */
    void disableHealthCheck();

private:
    std::map<std::string, std::unique_ptr<PoolNode>> nodes_;
    std::thread health_check_thread_;
    std::atomic<bool> health_check_enabled_;
    int health_check_interval_;
    std::mutex nodes_mutex_;

    /**
     * @brief 健康检查线程函数
     */
    void healthCheckThreadFunc();

    /**
     * @brief 检查单个节点的健康状态
     * @param node 节点指针
     * @return 是否健康
     */
    bool checkNodeHealth(PoolNode* node);

    /**
     * @brief 根据负载选择最佳节点
     * @param available_nodes 可用节点列表
     * @return 最佳节点
     */
    PoolNode* selectBestNode(const std::vector<PoolNode*>& available_nodes);
};

} // namespace llm
} // namespace forensics