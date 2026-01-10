#pragma once

#include "LLMIntegrationDataTypes.h"
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <memory>

namespace forensics {
namespace llm {

/**
 * @brief 任务分类器 - 使用小模型智能分类任务类型和复杂度
 * 
 * 该模块负责：
 * 1. 分析输入内容，自动识别任务类型
 * 2. 评估任务复杂度
 * 3. 为任务分配合适的模型
 * 4. 提供任务优先级建议
 */
class TaskClassifier {
public:
    explicit TaskClassifier();
    ~TaskClassifier() = default;

    /**
     * @brief 分类任务
     * @param task 待分类的任务
     * @return 任务分类结果
     */
    TaskClassification classifyTask(const AITask& task);

    /**
     * @brief 从文本内容推断任务类型
     * @param content 输入文本内容
     * @param metadata 额外的元数据
     * @return 推断的任务类型
     */
    TaskType inferTaskType(const std::string& content, 
                        const std::map<std::string, std::string>& metadata = {});

    /**
     * @brief 评估任务复杂度
     * @param task 任务对象
     * @return 任务复杂度
     */
    TaskComplexity assessComplexity(const AITask& task);

    /**
     * @brief 获取任务优先级建议
     * @param task 任务对象
     * @return 建议的优先级 (1-10, 10为最高)
     */
    int getRecommendedPriority(const AITask& task);

    /**
     * @brief 检查是否需要多模态处理
     * @param task 任务对象
     * @return 是否需要多模态处理
     */
    bool requiresMultiModal(const AITask& task);

    /**
     * @brief 生成任务描述
     * @param task 任务对象
     * @return 任务描述字符串
     */
    std::string generateTaskDescription(const AITask& task);

private:
    // 任务类型关键词映射
    std::map<TaskType, std::vector<std::string>> task_keywords_;
    
    // 文件扩展名到任务类型的映射
    std::map<std::string, TaskType> extension_mapping_;
    
    // 正则表达式模式
    std::map<TaskType, std::vector<std::regex>> task_patterns_;

    /**
     * @brief 初始化关键词和模式
     */
    void initializeKeywords();

    /**
     * @brief 基于关键词匹配推断任务类型
     * @param content 输入内容
     * @return 匹配的任务类型
     */
    TaskType inferByKeywords(const std::string& content);

    /**
     * @brief 基于文件扩展名推断任务类型
     * @param file_path 文件路径
     * @return 任务类型
     */
    TaskType inferByExtension(const std::string& file_path);

    /**
     * @brief 基于正则表达式推断任务类型
     * @param content 输入内容
     * @return 匹配的任务类型
     */
    TaskType inferByPatterns(const std::string& content);

    /**
     * @brief 基于元数据推断任务类型
     * @param metadata 元数据
     * @return 任务类型
     */
    TaskType inferByMetadata(const std::map<std::string, std::string>& metadata);

    /**
     * @brief 计算文本复杂度指标
     * @param text 文本内容
     * @return 复杂度评分 (0-1)
     */
    double calculateTextComplexity(const std::string& text);

    /**
     * @brief 估算处理时间需求
     * @param task 任务对象
     * @return 预估处理时间（秒）
     */
    double estimateProcessingTime(const AITask& task);

    /**
     * @brief 检查内容语言
     * @param content 文本内容
     * @return 语言代码 ("zh", "en", "unknown")
     */
    std::string detectLanguage(const std::string& content);

    /**
     * @brief 检查是否包含敏感信息
     * @param content 文本内容
     * @return 是否包含敏感信息
     */
    bool containsSensitiveInfo(const std::string& content);

    /**
     * @brief 合并多种分类结果
     * @param types 多种推断结果
     * @param weights 各结果的权重
     * @return 最终的任务类型
     */
    TaskType mergeClassificationResults(const std::vector<TaskType>& types,
                                   const std::vector<double>& weights);

    /**
     * @brief 生成分类推理过程
     * @param task 任务对象
     * @param result 分类结果
     * @return 推理说明
     */
    std::string generateReasoning(const AITask& task, const TaskClassification& result);

public:
    // 静态工具方法
    static std::string taskTypeToString(TaskType type);
    static std::string complexityToString(TaskComplexity complexity);
    static TaskType stringToTaskType(const std::string& type_str);
    static TaskComplexity stringToComplexity(const std::string& complexity_str);
};

/**
 * @brief 智能任务分类器 - 使用小模型进行高级分类
 * 
 * 当基础分类器无法确定类型时，使用小模型进行智能分类
 */
class IntelligentTaskClassifier {
public:
    explicit IntelligentTaskClassifier(const std::string& lm_studio_url = "http://localhost:1234");
    ~IntelligentTaskClassifier() = default;

    /**
     * @brief 初始化分类器
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 使用小模型智能分类任务
     * @param task 待分类任务
     * @return 分类结果
     */
    TaskClassification classifyWithModel(const AITask& task);

    /**
     * @brief 批量分类任务
     * @param tasks 任务列表
     * @return 分类结果列表
     */
    std::vector<TaskClassification> classifyBatch(const std::vector<AITask>& tasks);

    /**
     * @brief 验证分类结果
     * @param task 原始任务
     * @param classification 分类结果
     * @return 验证结果和置信度
     */
    std::pair<bool, double> validateClassification(const AITask& task, 
                                              const TaskClassification& classification);

    /**
     * @brief 获取模型状态
     * @return 模型是否可用
     */
    bool isModelAvailable() const;

private:
    std::string lm_studio_url_;
    std::string classification_model_id_;
    bool is_initialized_;

    /**
     * @brief 构建分类提示词
     * @param task 任务对象
     * @return 分类提示词
     */
    std::string buildClassificationPrompt(const AITask& task);

    /**
     * @brief 解析模型响应
     * @param response 模型响应
     * @return 分类结果
     */
    TaskClassification parseModelResponse(const std::string& response);

    /**
     * @brief 调用模型API
     * @param prompt 提示词
     * @return 模型响应
     */
    std::string callModelAPI(const std::string& prompt);

    /**
     * @brief 构建验证提示词
     * @param task 任务对象
     * @param classification 分类结果
     * @return 验证提示词
     */
    std::string buildValidationPrompt(const AITask& task, 
                                 const TaskClassification& classification);
};

} // namespace llm
} // namespace forensics