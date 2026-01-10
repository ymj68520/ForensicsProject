#pragma once

#include "LLMIntegrationDataTypes.h"
#include "ModelManager.h"
#include "TaskClassifier.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace forensics {
namespace llm {

/**
 * @brief AI分析服务 - 提供高级取证分析功能
 * 
 * 核心功能：
 * 1. 证据智能分析和总结
 * 2. 报告自动生成
 * 3. 异常行为检测
 * 4. 模式识别和趋势分析
 * 5. 多模态证据关联分析
 */
class AIAnalysisService {
public:
    explicit AIAnalysisService(std::shared_ptr<ModelManager> model_manager);
    ~AIAnalysisService() = default;

    /**
     * @brief 初始化服务
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 分析单个证据
     * @param evidence 证据内容
     * @param metadata 证据元数据
     * @return 分析结果
     */
    EvidenceAnalysisResult analyzeEvidence(const std::string& evidence, 
                                     const std::map<std::string, std::string>& metadata = {});

    /**
     * @brief 批量分析证据
     * @param evidences 证据列表
     * @return 分析结果列表
     */
    std::vector<EvidenceAnalysisResult> analyzeBatchEvidence(
        const std::vector<std::pair<std::string, std::map<std::string, std::string>>>& evidences);

    /**
     * @brief 分析对话记录
     * @param conversation 对话内容
     * @param metadata 对话元数据
     * @return 对话分析结果
     */
    ConversationAnalysisResult analyzeConversation(const std::vector<std::map<std::string, std::string>>& conversation,
                                            const std::map<std::string, std::string>& metadata = {});

    /**
     * @brief 生成取证报告
     * @param case_context 案件上下文
     * @param evidence_analyses 证据分析结果
     * @param report_type 报告类型
     * @return 报告生成结果
     */
    ReportGenerationResult generateReport(const nlohmann::json& case_context,
                                      const std::vector<EvidenceAnalysisResult>& evidence_analyses,
                                      const std::string& report_type = "comprehensive");

    /**
     * @brief 检测异常行为
     * @param data 数据内容
     * @param baseline 基线数据（可选）
     * @return 异常检测结果
     */
    std::vector<nlohmann::json> detectAnomalies(const std::string& data,
                                             const std::string& baseline = "");

    /**
     * @brief 识别模式和趋势
     * @param time_series_data 时间序列数据
     * @return 模式识别结果
     */
    nlohmann::json recognizePatterns(const std::vector<std::map<std::string, std::string>>& time_series_data);

    /**
     * @brief 多模态分析
     * @param text 文本内容
     * @param image_path 图像路径
     * @param metadata 额外元数据
     * @return 多模态分析结果
     */
    MultiModalResult performMultiModalAnalysis(const std::string& text,
                                           const std::string& image_path,
                                           const std::map<std::string, std::string>& metadata = {});

    /**
     * @brief 情感分析
     * @param text 文本内容
     * @return 情感分析结果
     */
    std::map<std::string, double> analyzeSentiment(const std::string& text);

    /**
     * @brief 实体识别
     * @param text 文本内容
     * @param entity_types 要识别的实体类型
     * @return 识别出的实体
     */
    std::vector<std::map<std::string, std::string>> recognizeEntities(const std::string& text,
                                                               const std::vector<std::string>& entity_types = {"person", "organization", "location", "date", "phone", "email"});

    /**
     * @brief 证据关联分析
     * @param evidences 证据列表
     * @return 关联关系网络
     */
    nlohmann::json analyzeEvidenceRelations(const std::vector<EvidenceAnalysisResult>& evidences);

    /**
     * @brief 时间线重建
     * @param events 事件列表
     * @return 结构化时间线
     */
    nlohmann::json rebuildTimeline(const std::vector<std::map<std::string, std::string>>& events);

    /**
     * @brief 异步分析证据
     * @param evidence 证据内容
     * @param metadata 证据元数据
     * @param callback 完成回调
     */
    void analyzeEvidenceAsync(const std::string& evidence,
                           const std::map<std::string, std::string>& metadata,
                           std::function<void(const EvidenceAnalysisResult&)> callback);

    /**
     * @brief 获取服务状态
     * @return 服务状态信息
     */
    nlohmann::json getServiceStatus();

    /**
     * @brief 设置分析参数
     * @param parameters 参数映射
     */
    void setAnalysisParameters(const std::map<std::string, std::string>& parameters);

private:
    std::shared_ptr<ModelManager> model_manager_;
    std::map<std::string, std::string> analysis_parameters_;
    std::mutex parameters_mutex_;
    bool is_initialized_;

    // 分析模板
    struct AnalysisTemplate {
        std::string name;
        std::string system_prompt;
        std::vector<std::string> required_fields;
        std::map<std::string, double> default_weights;
    };
    
    std::map<std::string, AnalysisTemplate> analysis_templates_;

    /**
     * @brief 初始化分析模板
     */
    void initializeAnalysisTemplates();

    /**
     * @brief 根据证据类型选择分析模板
     * @param evidence_type 证据类型
     * @return 分析模板
     */
    AnalysisTemplate selectAnalysisTemplate(const std::string& evidence_type);

    /**
     * @brief 构建证据分析任务
     * @param evidence 证据内容
     * @param metadata 证据元数据
     * @param template 分析模板
     * @return AI任务
     */
    AITask buildEvidenceAnalysisTask(const std::string& evidence,
                                   const std::map<std::string, std::string>& metadata,
                                   const AnalysisTemplate& template);

    /**
     * @brief 解析证据分析响应
     * @param response 模型响应
     * @return 分析结果
     */
    EvidenceAnalysisResult parseEvidenceAnalysisResponse(const AIResponse& response);

    /**
     * @brief 构建对话分析任务
     * @param conversation 对话内容
     * @param metadata 对话元数据
     * @return AI任务
     */
    AITask buildConversationAnalysisTask(const std::vector<std::map<std::string, std::string>>& conversation,
                                     const std::map<std::string, std::string>& metadata);

    /**
     * @brief 格式化对话为文本
     * @param conversation 对话记录
     * @return 格式化文本
     */
    std::string formatConversation(const std::vector<std::map<std::string, std::string>>& conversation);

    /**
     * @brief 解析对话分析响应
     * @param response 模型响应
     * @return 对话分析结果
     */
    ConversationAnalysisResult parseConversationAnalysisResponse(const AIResponse& response);

    /**
     * @brief 构建报告生成任务
     * @param case_context 案件上下文
     * @param evidence_analyses 证据分析结果
     * @param report_type 报告类型
     * @return AI任务
     */
    AITask buildReportGenerationTask(const nlohmann::json& case_context,
                                   const std::vector<EvidenceAnalysisResult>& evidence_analyses,
                                   const std::string& report_type);

    /**
     * @brief 准备报告生成数据
     * @param case_context 案件上下文
     * @param evidence_analyses 证据分析结果
     * @return 格式化的报告数据
     */
    std::string prepareReportData(const nlohmann::json& case_context,
                                const std::vector<EvidenceAnalysisResult>& evidence_analyses);

    /**
     * @brief 解析报告生成响应
     * @param response 模型响应
     * @param report_type 报告类型
     * @return 报告生成结果
     */
    ReportGenerationResult parseReportGenerationResponse(const AIResponse& response,
                                                  const std::string& report_type);

    /**
     * @brief 构建异常检测任务
     * @param data 数据内容
     * @param baseline 基线数据
     * @return AI任务
     */
    AITask buildAnomalyDetectionTask(const std::string& data,
                                   const std::string& baseline);

    /**
     * @brief 解析异常检测响应
     * @param response 模型响应
     * @return 异常列表
     */
    std::vector<nlohmann::json> parseAnomalyDetectionResponse(const AIResponse& response);

    /**
     * @brief 构建模式识别任务
     * @param time_series_data 时间序列数据
     * @return AI任务
     */
    AITask buildPatternRecognitionTask(const std::vector<std::map<std::string, std::string>>& time_series_data);

    /**
     * @brief 格式化时间序列数据
     * @param data 时间序列数据
     * @return 格式化文本
     */
    std::string formatTimeSeriesData(const std::vector<std::map<std::string, std::string>>& data);

    /**
     * @brief 解析模式识别响应
     * @param response 模型响应
     * @return 模式识别结果
     */
    nlohmann::json parsePatternRecognitionResponse(const AIResponse& response);

    /**
     * @brief 构建多模态分析任务
     * @param text 文本内容
     * @param image_path 图像路径
     * @param metadata 额外元数据
     * @return AI任务
     */
    AITask buildMultiModalTask(const std::string& text,
                               const std::string& image_path,
                               const std::map<std::string, std::string>& metadata);

    /**
     * @brief 解析多模态分析响应
     * @param response 模型响应
     * @return 多模态分析结果
     */
    MultiModalResult parseMultiModalResponse(const AIResponse& response);

    /**
     * @brief 计算证据相关性分数
     * @param evidence1 证据1
     * @param evidence2 证据2
     * @return 相关性分数 (0-1)
     */
    double calculateEvidenceRelevance(const EvidenceAnalysisResult& evidence1,
                                  const EvidenceAnalysisResult& evidence2);

    /**
     * @brief 构建关联分析任务
     * @param evidences 证据列表
     * @return AI任务
     */
    AITask buildRelationAnalysisTask(const std::vector<EvidenceAnalysisResult>& evidences);

    /**
     * @brief 解析关联分析响应
     * @param response 模型响应
     * @return 关联关系网络
     */
    nlohmann::json parseRelationAnalysisResponse(const AIResponse& response);

    /**
     * @brief 验证分析结果质量
     * @param result 分析结果
     * @return 质量分数 (0-1)
     */
    double validateAnalysisQuality(const EvidenceAnalysisResult& result);

    /**
     * @brief 生成证据ID
     * @return 唯一ID
     */
    std::string generateEvidenceId();

    /**
     * @brief 提取关键词
     * @param text 文本内容
     * @param max_keywords 最大关键词数
     * @return 关键词列表
     */
    std::vector<std::string> extractKeywords(const std::string& text, int max_keywords = 10);

    /**
     * @brief 计算文本相似度
     * @param text1 文本1
     * @param text2 文本2
     * @return 相似度分数 (0-1)
     */
    double calculateTextSimilarity(const std::string& text1, const std::string& text2);

public:
    // 静态工具方法
    static std::string getAnalysisTemplate(const std::string& template_name);
    static std::vector<std::string> getSupportedReportTypes();
    static std::map<std::string, std::string> getDefaultAnalysisParameters();
};

/**
 * @brief 预设分析配置
 * 
 * 提供常见取证场景的预设分析配置
 */
class ForensicAnalysisPresets {
public:
    /**
     * @brief 获取恶意软件分析预设
     * @return 预设配置
     */
    static std::map<std::string, std::string> getMalwareAnalysisPreset();

    /**
     * @brief 获取网络入侵分析预设
     * @return 预设配置
     */
    static std::map<std::string, std::string> getNetworkIntrusionPreset();

    /**
     * @brief 获取数据泄露分析预设
     * @return 预设配置
     */
    static std::map<std::string, std::string> getDataLeakagePreset();

    /**
     * @brief 获取内部威胁分析预设
     * @return 预设配置
     */
    static std::map<std::string, std::string> getInsiderThreatPreset();

    /**
     * @brief 获取数字欺诈分析预设
     * @return 预设配置
     */
    static std::map<std::string, std::string> getDigitalFraudPreset();

private:
    static std::string buildPresetPrompt(const std::string& scenario, 
                                     const std::vector<std::string>& focus_areas);
};

} // namespace llm
} // namespace forensics