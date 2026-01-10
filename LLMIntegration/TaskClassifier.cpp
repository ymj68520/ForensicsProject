#include "TaskClassifier.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace forensics {
namespace llm {

TaskClassifier::TaskClassifier() {
    initializeKeywords();
}

void TaskClassifier::initializeKeywords() {
    // 初始化任务类型关键词
    task_keywords_[TaskType::TEXT_ANALYSIS] = {
        "分析文本", "文本分析", "内容分析", "document", "text", "analyze", "summary"
    };
    
    task_keywords_[TaskType::IMAGE_ANALYSIS] = {
        "图片", "图像", "照片", "image", "photo", "picture", "视觉", "visual"
    };
    
    task_keywords_[TaskType::DOCUMENT_ANALYSIS] = {
        "文档", "报告", "合同", "document", "report", "contract", "pdf", "word"
    };
    
    task_keywords_[TaskType::CONVERSATION_ANALYSIS] = {
        "对话", "聊天", "记录", "conversation", "chat", "dialogue", "message"
    };
    
    task_keywords_[TaskType::EVIDENCE_SUMMARY] = {
        "证据", "摘要", "总结", "evidence", "summary", "conclusion", "finding"
    };
    
    task_keywords_[TaskType::REPORT_GENERATION] = {
        "生成报告", "报告生成", "report generation", "generate report", "create report"
    };
    
    task_keywords_[TaskType::ANOMALY_DETECTION] = {
        "异常", "检测", "可疑", "anomaly", "detection", "suspicious", "abnormal"
    };
    
    task_keywords_[TaskType::PATTERN_RECOGNITION] = {
        "模式", "识别", "规律", "pattern", "recognition", "behavior", "trend"
    };
    
    task_keywords_[TaskType::SENTIMENT_ANALYSIS] = {
        "情感", "情绪", "态度", "sentiment", "emotion", "feeling", "attitude"
    };
    
    task_keywords_[TaskType::ENTITY_RECOGNITION] = {
        "实体", "识别", "人名", "地名", "entity", "person", "location", "organization"
    };
    
    task_keywords_[TaskType::MULTIMODAL_ANALYSIS] = {
        "多模态", "图文", "综合", "multimodal", "image text", "combined"
    };
    
    task_keywords_[TaskType::CODE_ANALYSIS] = {
        "代码", "程序", "脚本", "code", "program", "script", "software"
    };
    
    task_keywords_[TaskType::NETWORK_ANALYSIS] = {
        "网络", "流量", "连接", "network", "traffic", "connection", "protocol"
    };

    // 初始化文件扩展名映射
    extension_mapping_ = {
        {".txt", TaskType::TEXT_ANALYSIS},
        {".doc", TaskType::DOCUMENT_ANALYSIS},
        {".docx", TaskType::DOCUMENT_ANALYSIS},
        {".pdf", TaskType::DOCUMENT_ANALYSIS},
        {".jpg", TaskType::IMAGE_ANALYSIS},
        {".jpeg", TaskType::IMAGE_ANALYSIS},
        {".png", TaskType::IMAGE_ANALYSIS},
        {".bmp", TaskType::IMAGE_ANALYSIS},
        {".gif", TaskType::IMAGE_ANALYSIS},
        {".mp4", TaskType::IMAGE_ANALYSIS},
        {".avi", TaskType::IMAGE_ANALYSIS},
        {".mov", TaskType::IMAGE_ANALYSIS},
        {".cpp", TaskType::CODE_ANALYSIS},
        {".c", TaskType::CODE_ANALYSIS},
        {".py", TaskType::CODE_ANALYSIS},
        {".js", TaskType::CODE_ANALYSIS},
        {".java", TaskType::CODE_ANALYSIS},
        {".html", TaskType::CODE_ANALYSIS},
        {".css", TaskType::CODE_ANALYSIS},
        {".json", TaskType::CODE_ANALYSIS},
        {".xml", TaskType::CODE_ANALYSIS},
        {".log", TaskType::CONVERSATION_ANALYSIS},
        {".csv", TaskType::TEXT_ANALYSIS},
        {".sql", TaskType::CODE_ANALYSIS}
    };

    // 初始化正则表达式模式
    task_patterns_[TaskType::CONVERSATION_ANALYSIS] = {
        std::regex(R"(\b\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\b)"), // 时间戳
        std::regex(R"(^.*?:\s*.*$)"), // 用户名: 消息格式
        std::regex(R"(\[.*?\]\s*.*?:\s*.*)") // [时间] 用户名: 消息
    };
    
    task_patterns_[TaskType::CODE_ANALYSIS] = {
        std::regex(R"(#include\s*<.*?>)"), // C/C++ include
        std::regex(R"(import\s+\w+)"), // Python import
        std::regex(R"(function\s+\w+\s*\()"), // JavaScript function
        std::regex(R"(public\s+class\s+\w+)"), // Java class
        std::regex(R"(\{\s*\n.*\n\s*\})") // 代码块
    };
    
    task_patterns_[TaskType::DOCUMENT_ANALYSIS] = {
        std::regex(R"(第\s*\d+\s*章|第\s*\d+\s*条)"), // 中文文档格式
        std::regex(R"(Article\s+\d+|Section\s+\d+)"), // 英文文档格式
        std::regex(R"(\d+\.\s*[A-Z][^.]*\.)"), // 编号段落
        std::regex(R"(摘要|Abstract|引言|Introduction|结论|Conclusion)")
    };
}

TaskClassification TaskClassifier::classifyTask(const AITask& task) {
    TaskClassification result;
    result.primary_type = TaskType::UNKNOWN;
    result.secondary_type = TaskType::UNKNOWN;
    result.complexity = TaskComplexity::MEDIUM;
    result.confidence = 0.0;
    result.reasoning = "";

    // 收集多种分类结果
    std::vector<TaskType> inferred_types;
    std::vector<double> weights;

    // 1. 基于输入文本分类
    if (!task.input_text.empty()) {
        TaskType text_type = inferTaskType(task.input_text, task.metadata);
        inferred_types.push_back(text_type);
        weights.push_back(0.4); // 文本内容权重较高
    }

    // 2. 基于图像路径分类
    if (!task.input_image_path.empty()) {
        TaskType image_type = inferByExtension(task.input_image_path);
        if (image_type != TaskType::UNKNOWN) {
            inferred_types.push_back(image_type);
            weights.push_back(0.3);
        }
    }

    // 3. 基于元数据分类
    if (!task.metadata.empty()) {
        TaskType metadata_type = inferByMetadata(task.metadata);
        if (metadata_type != TaskType::UNKNOWN) {
            inferred_types.push_back(metadata_type);
            weights.push_back(0.2);
        }
    }

    // 4. 基于系统提示词分类
    if (!task.system_prompt.empty()) {
        TaskType prompt_type = inferTaskType(task.system_prompt);
        if (prompt_type != TaskType::UNKNOWN) {
            inferred_types.push_back(prompt_type);
            weights.push_back(0.1);
        }
    }

    // 合并分类结果
    if (!inferred_types.empty()) {
        result.primary_type = mergeClassificationResults(inferred_types, weights);
        
        // 设置次要类型（如果有的话）
        std::map<TaskType, int> type_counts;
        for (const auto& type : inferred_types) {
            type_counts[type]++;
        }
        
        if (type_counts.size() > 1) {
            auto it = std::max_element(type_counts.begin(), type_counts.end(),
                [](const auto& a, const auto& b) {
                    return a.first == result.primary_type ? false : 
                           b.first == result.primary_type ? true :
                           a.second < b.second;
                });
            if (it != type_counts.end() && it->first != result.primary_type) {
                result.secondary_type = it->first;
            }
        }
    }

    // 评估复杂度
    result.complexity = assessComplexity(task);
    
    // 计算置信度
    result.confidence = calculateConfidence(inferred_types, weights);
    
    // 生成推理说明
    result.reasoning = generateReasoning(task, result);
    
    // 设置可能的类型
    result.possible_types = inferred_types;

    return result;
}

TaskType TaskClassifier::inferTaskType(const std::string& content, 
                                   const std::map<std::string, std::string>& metadata) {
    if (content.empty()) {
        return TaskType::UNKNOWN;
    }

    // 1. 基于关键词匹配
    TaskType keyword_type = inferByKeywords(content);
    
    // 2. 基于模式匹配
    TaskType pattern_type = inferByPatterns(content);
    
    // 3. 基于元数据
    TaskType metadata_type = inferByMetadata(metadata);

    // 权重合并
    std::vector<TaskType> types = {keyword_type, pattern_type, metadata_type};
    std::vector<double> weights = {0.5, 0.3, 0.2};
    
    return mergeClassificationResults(types, weights);
}

TaskComplexity TaskClassifier::assessComplexity(const AITask& task) {
    double complexity_score = 0.0;
    
    // 文本长度复杂度
    if (!task.input_text.empty()) {
        double text_complexity = calculateTextComplexity(task.input_text);
        complexity_score += text_complexity * 0.4;
    }
    
    // 图像复杂度
    if (!task.input_image_path.empty()) {
        complexity_score += 0.3; // 图像处理增加复杂度
        if (requiresMultiModal(task)) {
            complexity_score += 0.2; // 多模态增加更多复杂度
        }
    }
    
    // 元数据复杂度
    if (!task.metadata.empty()) {
        complexity_score += task.metadata.size() * 0.01;
    }
    
    // 自定义max_tokens复杂度
    if (task.max_tokens > 2000) {
        complexity_score += 0.2;
    }
    
    // 系统提示词复杂度
    if (!task.system_prompt.empty()) {
        double prompt_complexity = calculateTextComplexity(task.system_prompt);
        complexity_score += prompt_complexity * 0.1;
    }

    // 转换为复杂度等级
    if (complexity_score < 0.3) {
        return TaskComplexity::LOW;
    } else if (complexity_score < 0.7) {
        return TaskComplexity::MEDIUM;
    } else if (complexity_score < 0.9) {
        return TaskComplexity::HIGH;
    } else {
        return TaskComplexity::CRITICAL;
    }
}

int TaskClassifier::getRecommendedPriority(const AITask& task) {
    int priority = 5; // 默认优先级
    
    // 基于复杂度调整
    switch (task.complexity) {
        case TaskComplexity::CRITICAL:
            priority += 3;
            break;
        case TaskComplexity::HIGH:
            priority += 2;
            break;
        case TaskComplexity::MEDIUM:
            priority += 1;
            break;
        case TaskComplexity::LOW:
            priority -= 1;
            break;
    }
    
    // 基于任务类型调整
    switch (task.type) {
        case TaskType::EVIDENCE_SUMMARY:
        case TaskType::REPORT_GENERATION:
            priority += 2; // 重要任务
            break;
        case TaskType::ANOMALY_DETECTION:
            priority += 1; // 安全相关
            break;
        case TaskType::TEXT_ANALYSIS:
        case TaskType::IMAGE_ANALYSIS:
            priority -= 1; // 常规任务
            break;
    }
    
    // 基于截止时间调整
    if (task.deadline != std::chrono::system_clock::time_point{}) {
        auto now = std::chrono::system_clock::now();
        auto time_left = std::chrono::duration_cast<std::chrono::hours>(task.deadline - now).count();
        
        if (time_left < 1) {
            priority += 3; // 紧急
        } else if (time_left < 24) {
            priority += 2; // 24小时内
        } else if (time_left < 72) {
            priority += 1; // 3天内
        }
    }
    
    // 确保优先级在1-10范围内
    return std::max(1, std::min(10, priority));
}

bool TaskClassifier::requiresMultiModal(const AITask& task) {
    // 如果既有文本又有图像，需要多模态处理
    bool has_text = !task.input_text.empty();
    bool has_image = !task.input_image_path.empty();
    
    if (has_text && has_image) {
        return true;
    }
    
    // 检查元数据是否指示多模态
    auto it = task.metadata.find("modality");
    if (it != task.metadata.end() && it->second == "multimodal") {
        return true;
    }
    
    // 检查任务类型是否需要多模态
    return task.type == TaskType::MULTIMODAL_ANALYSIS;
}

std::string TaskClassifier::generateTaskDescription(const AITask& task) {
    std::ostringstream desc;
    
    desc << "任务ID: " << task.id << "\n";
    desc << "类型: " << taskTypeToString(task.type) << "\n";
    desc << "复杂度: " << complexityToString(task.complexity) << "\n";
    desc << "优先级: " << task.priority << "\n";
    
    if (!task.input_text.empty()) {
        desc << "文本长度: " << task.input_text.length() << " 字符\n";
    }
    
    if (!task.input_image_path.empty()) {
        desc << "图像路径: " << task.input_image_path << "\n";
    }
    
    if (!task.metadata.empty()) {
        desc << "元数据项数: " << task.metadata.size() << "\n";
    }
    
    if (requiresMultiModal(task)) {
        desc << "多模态处理: 是\n";
    }
    
    return desc.str();
}

// 私有方法实现
TaskType TaskClassifier::inferByKeywords(const std::string& content) {
    std::map<TaskType, int> match_counts;
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
    
    for (const auto& [type, keywords] : task_keywords_) {
        int matches = 0;
        for (const auto& keyword : keywords) {
            std::string lower_keyword = keyword;
            std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(), ::tolower);
            
            size_t pos = lower_content.find(lower_keyword);
            while (pos != std::string::npos) {
                matches++;
                pos = lower_content.find(lower_keyword, pos + 1);
            }
        }
        
        if (matches > 0) {
            match_counts[type] = matches;
        }
    }
    
    if (match_counts.empty()) {
        return TaskType::UNKNOWN;
    }
    
    auto max_match = std::max_element(match_counts.begin(), match_counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return max_match->first;
}

TaskType TaskClassifier::inferByExtension(const std::string& file_path) {
    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto it = extension_mapping_.find(extension);
    return (it != extension_mapping_.end()) ? it->second : TaskType::UNKNOWN;
}

TaskType TaskClassifier::inferByPatterns(const std::string& content) {
    std::map<TaskType, int> pattern_matches;
    
    for (const auto& [type, patterns] : task_patterns_) {
        int matches = 0;
        for (const auto& pattern : patterns) {
            if (std::regex_search(content, pattern)) {
                matches++;
            }
        }
        
        if (matches > 0) {
            pattern_matches[type] = matches;
        }
    }
    
    if (pattern_matches.empty()) {
        return TaskType::UNKNOWN;
    }
    
    auto max_match = std::max_element(pattern_matches.begin(), pattern_matches.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return max_match->first;
}

TaskType TaskClassifier::inferByMetadata(const std::map<std::string, std::string>& metadata) {
    auto it = metadata.find("task_type");
    if (it != metadata.end()) {
        return stringToTaskType(it->second);
    }
    
    it = metadata.find("file_type");
    if (it != metadata.end()) {
        std::string file_type = it->second;
        std::transform(file_type.begin(), file_type.end(), file_type.begin(), ::tolower);
        
        if (file_type.find("image") != std::string::npos) {
            return TaskType::IMAGE_ANALYSIS;
        } else if (file_type.find("document") != std::string::npos) {
            return TaskType::DOCUMENT_ANALYSIS;
        } else if (file_type.find("code") != std::string::npos) {
            return TaskType::CODE_ANALYSIS;
        } else if (file_type.find("conversation") != std::string::npos) {
            return TaskType::CONVERSATION_ANALYSIS;
        }
    }
    
    return TaskType::UNKNOWN;
}

double TaskClassifier::calculateTextComplexity(const std::string& text) {
    if (text.empty()) return 0.0;
    
    double complexity = 0.0;
    
    // 1. 文本长度复杂度
    size_t length = text.length();
    if (length > 5000) complexity += 0.3;
    else if (length > 2000) complexity += 0.2;
    else if (length > 500) complexity += 0.1;
    
    // 2. 句子复杂度
    std::regex sentence_regex(R"([.!?]+)");
    auto words_begin = std::sregex_iterator(text.begin(), text.end(), sentence_regex);
    auto words_end = std::sregex_iterator();
    int sentence_count = std::distance(words_begin, words_end);
    
    if (sentence_count > 50) complexity += 0.2;
    else if (sentence_count > 20) complexity += 0.1;
    
    // 3. 专业术语检测
    std::vector<std::string> technical_terms = {
        "algorithm", "encryption", "forensic", "malware", "vulnerability",
        "算法", "加密", "取证", "恶意软件", "漏洞"
    };
    
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    int tech_count = 0;
    for (const auto& term : technical_terms) {
        if (lower_text.find(term) != std::string::npos) {
            tech_count++;
        }
    }
    
    complexity += tech_count * 0.05;
    
    return std::min(1.0, complexity);
}

TaskType TaskClassifier::mergeClassificationResults(const std::vector<TaskType>& types,
                                            const std::vector<double>& weights) {
    std::map<TaskType, double> scores;
    
    for (size_t i = 0; i < types.size() && i < weights.size(); ++i) {
        if (types[i] != TaskType::UNKNOWN) {
            scores[types[i]] += weights[i];
        }
    }
    
    if (scores.empty()) {
        return TaskType::UNKNOWN;
    }
    
    auto max_score = std::max_element(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return max_score->first;
}

double TaskClassifier::calculateConfidence(const std::vector<TaskType>& types,
                                     const std::vector<double>& weights) {
    if (types.empty()) return 0.0;
    
    std::map<TaskType, double> scores;
    double total_weight = 0.0;
    
    for (size_t i = 0; i < types.size() && i < weights.size(); ++i) {
        if (types[i] != TaskType::UNKNOWN) {
            scores[types[i]] += weights[i];
            total_weight += weights[i];
        }
    }
    
    if (scores.empty() || total_weight == 0.0) return 0.0;
    
    // 计算最高分数的占比作为置信度
    auto max_score = std::max_element(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return max_score->second / total_weight;
}

std::string TaskClassifier::generateReasoning(const AITask& task, const TaskClassification& result) {
    std::ostringstream reasoning;
    
    reasoning << "任务分类推理过程:\n";
    reasoning << "1. 主要类型: " << taskTypeToString(result.primary_type);
    reasoning << " (置信度: " << std::fixed << std::setprecision(2) << result.confidence << ")\n";
    
    if (result.secondary_type != TaskType::UNKNOWN) {
        reasoning << "2. 次要类型: " << taskTypeToString(result.secondary_type) << "\n";
    }
    
    reasoning << "3. 复杂度评估: " << complexityToString(result.complexity) << "\n";
    
    if (!task.input_text.empty()) {
        reasoning << "4. 文本分析: 基于关键词和模式匹配\n";
    }
    
    if (!task.input_image_path.empty()) {
        reasoning << "5. 图像分析: 基于文件扩展名 " << std::filesystem::path(task.input_image_path).extension().string() << "\n";
    }
    
    if (requiresMultiModal(task)) {
        reasoning << "6. 多模态检测: 需要图文联合分析\n";
    }
    
    return reasoning.str();
}

// 静态方法实现
std::string TaskClassifier::taskTypeToString(TaskType type) {
    switch (type) {
        case TaskType::TEXT_ANALYSIS: return "文本分析";
        case TaskType::IMAGE_ANALYSIS: return "图像分析";
        case TaskType::DOCUMENT_ANALYSIS: return "文档分析";
        case TaskType::CONVERSATION_ANALYSIS: return "对话分析";
        case TaskType::EVIDENCE_SUMMARY: return "证据摘要";
        case TaskType::REPORT_GENERATION: return "报告生成";
        case TaskType::ANOMALY_DETECTION: return "异常检测";
        case TaskType::PATTERN_RECOGNITION: return "模式识别";
        case TaskType::SENTIMENT_ANALYSIS: return "情感分析";
        case TaskType::ENTITY_RECOGNITION: return "实体识别";
        case TaskType::MULTIMODAL_ANALYSIS: return "多模态分析";
        case TaskType::CODE_ANALYSIS: return "代码分析";
        case TaskType::NETWORK_ANALYSIS: return "网络分析";
        default: return "未知类型";
    }
}

std::string TaskClassifier::complexityToString(TaskComplexity complexity) {
    switch (complexity) {
        case TaskComplexity::LOW: return "低复杂度";
        case TaskComplexity::MEDIUM: return "中等复杂度";
        case TaskComplexity::HIGH: return "高复杂度";
        case TaskComplexity::CRITICAL: return "极高复杂度";
        default: return "未知复杂度";
    }
}

TaskType TaskClassifier::stringToTaskType(const std::string& type_str) {
    std::map<std::string, TaskType> type_map = {
        {"TEXT_ANALYSIS", TaskType::TEXT_ANALYSIS},
        {"IMAGE_ANALYSIS", TaskType::IMAGE_ANALYSIS},
        {"DOCUMENT_ANALYSIS", TaskType::DOCUMENT_ANALYSIS},
        {"CONVERSATION_ANALYSIS", TaskType::CONVERSATION_ANALYSIS},
        {"EVIDENCE_SUMMARY", TaskType::EVIDENCE_SUMMARY},
        {"REPORT_GENERATION", TaskType::REPORT_GENERATION},
        {"ANOMALY_DETECTION", TaskType::ANOMALY_DETECTION},
        {"PATTERN_RECOGNITION", TaskType::PATTERN_RECOGNITION},
        {"SENTIMENT_ANALYSIS", TaskType::SENTIMENT_ANALYSIS},
        {"ENTITY_RECOGNITION", TaskType::ENTITY_RECOGNITION},
        {"MULTIMODAL_ANALYSIS", TaskType::MULTIMODAL_ANALYSIS},
        {"CODE_ANALYSIS", TaskType::CODE_ANALYSIS},
        {"NETWORK_ANALYSIS", TaskType::NETWORK_ANALYSIS}
    };
    
    auto it = type_map.find(type_str);
    return (it != type_map.end()) ? it->second : TaskType::UNKNOWN;
}

TaskComplexity TaskClassifier::stringToComplexity(const std::string& complexity_str) {
    std::map<std::string, TaskComplexity> complexity_map = {
        {"LOW", TaskComplexity::LOW},
        {"MEDIUM", TaskComplexity::MEDIUM},
        {"HIGH", TaskComplexity::HIGH},
        {"CRITICAL", TaskComplexity::CRITICAL}
    };
    
    auto it = complexity_map.find(complexity_str);
    return (it != complexity_map.end()) ? it->second : TaskComplexity::MEDIUM;
}

} // namespace llm
} // namespace forensics