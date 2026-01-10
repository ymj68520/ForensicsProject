# 数字取证工具 - 大模型集成模块

本模块为数字取证工具提供了完整的大语言模型（LLM）集成能力，通过LM Studio支持本地模型部署，实现智能化的证据分析、报告生成和模式识别功能。

## 🚀 主要特性

### 智能任务分类
- **自动任务识别**：基于内容和元数据自动识别任务类型
- **复杂度评估**：智能评估任务复杂度，选择合适的模型
- **多级分类器**：结合规则和机器学习的混合分类方法
- **置信度评分**：为每个分类结果提供置信度评估

### 智能模型管理
- **多模型支持**：同时管理多个LLM模型
- **自动模型选择**：根据任务类型和复杂度自动选择最佳模型
- **负载均衡**：智能分配任务到不同模型，优化资源利用
- **故障转移**：模型失败时自动切换到备用模型
- **性能监控**：实时监控模型性能指标

### 高级分析服务
- **证据分析**：深度分析数字证据，提取关键信息
- **对话分析**：分析聊天记录，识别行为模式和情感倾向
- **报告生成**：自动生成专业的取证报告
- **异常检测**：识别异常行为和可疑模式
- **模式识别**：发现数据中的隐藏模式和趋势
- **情感分析**：分析文本情感倾向
- **实体识别**：识别人名、地名、组织等实体
- **多模态分析**：联合分析文本和图像内容

### RESTful API
- **完整API接口**：提供RESTful API供外部系统调用
- **异步处理**：支持异步任务处理，提高响应速度
- **批量操作**：支持批量证据分析和处理
- **认证授权**：支持API认证和权限控制
- **CORS支持**：支持跨域请求

## 📋 系统要求

### 硬件要求
- **CPU**：Intel i5 8代以上或AMD Ryzen 5以上
- **内存**：16GB以上（推荐32GB）
- **存储**：10GB可用空间
- **GPU**：NVIDIA RTX 3060以上（推荐，可选）

### 软件要求
- **操作系统**：Windows 10/11, Ubuntu 20.04+, macOS 12+
- **C++编译器**：GCC 9+, Clang 10+, MSVC 2019+
- **CMake**：3.16+
- **LM Studio**：最新版本
- **Python**：3.8+（用于某些依赖）

### 依赖库
- **libcurl**：HTTP客户端
- **OpenSSL**：加密支持
- **nlohmann/json**：JSON处理
- **The Sleuth Kit**：取证核心库

## 🛠️ 安装和配置

### 1. 安装LM Studio
```bash
# 从官网下载并安装LM Studio
# https://lmstudio.ai/

# 启动LM Studio并下载模型
# 推荐模型：
# - llama3.1-8b-instruct (轻量级分类)
# - qwen2-7b-instruct (通用分析)
# - llava-v1.6-34b (多模态)
```

### 2. 编译LLM集成模块
```bash
cd LLMIntegration
mkdir build && cd build
cmake ..
make -j4
```

### 3. 配置文件
编辑 `llm_config.json` 文件，配置：
- LM Studio连接地址
- 模型参数
- HTTP服务端口
- 分析参数

### 4. 启动服务
```bash
# 运行示例程序
./llm_example

# 或者集成到主程序
# 在main.cpp中包含LLM集成模块
```

## 📖 API使用示例

### 证据分析
```bash
curl -X POST http://localhost:8080/api/llm/analyze/evidence \
  -H "Content-Type: application/json" \
  -d '{
    "evidence": "系统日志显示异常登录行为...",
    "metadata": {
      "type": "log",
      "source": "system_log"
    }
  }'
```

### 情感分析
```bash
curl -X POST http://localhost:8080/api/llm/analyze/sentiment \
  -H "Content-Type: application/json" \
  -d '{
    "text": "用户对此事件感到愤怒和担忧"
  }'
```

### 批量分析
```bash
curl -X POST http://localhost:8080/api/llm/analyze/batch \
  -H "Content-Type: application/json" \
  -d '{
    "evidences": [
      {
        "content": "证据1内容",
        "metadata": {"type": "text"}
      },
      {
        "content": "证据2内容",
        "metadata": {"type": "log"}
      }
    ]
  }'
```

## 🔧 C++编程接口

### 基本使用
```cpp
#include "LLMIntegration/ModelManager.h"
#include "LLMIntegration/AIAnalysisService.h"

// 初始化模型管理器
auto model_manager = std::make_shared<forensics::llm::ModelManager>("http://localhost:1234");
model_manager->initialize();

// 创建分析服务
auto analysis_service = std::make_shared<forensics::llm::AIAnalysisService>(model_manager);
analysis_service->initialize();

// 分析证据
forensics::llm::EvidenceAnalysisResult result = analysis_service->analyzeEvidence(
    "系统日志显示可疑活动...",
    {{"type", "log"}}
);

std::cout << "分析结果: " << result.content_summary << std::endl;
std::cout << "可疑度: " << result.suspicious_score << std::endl;
```

### 异步处理
```cpp
// 异步分析
analysis_service->analyzeEvidenceAsync(evidence, metadata, 
    [](const forensics::llm::EvidenceAnalysisResult& result) {
        std::cout << "异步分析完成: " << result.evidence_id << std::endl;
    });
```

### 任务分类
```cpp
#include "LLMIntegration/TaskClassifier.h"

forensics::llm::TaskClassifier classifier;

forensics::llm::AITask task;
task.input_text = "请分析这段聊天记录";

forensics::llm::TaskClassification classification = classifier.classifyTask(task);
std::cout << "任务类型: " << forensics::llm::TaskClassifier::taskTypeToString(classification.primary_type) << std::endl;
```

## 🎯 应用场景

### 1. 恶意软件分析
- 自动分析恶意行为模式
- 识别攻击技术TTPs
- 生成分析报告

### 2. 网络入侵检测
- 分析网络流量异常
- 识别入侵行为
- 关联攻击事件

### 3. 数据泄露调查
- 分析数据访问模式
- 识别敏感信息泄露
- 追踪数据流向

### 4. 内部威胁检测
- 分析员工行为异常
- 识别权限滥用
- 生成风险评估报告

### 5. 数字证据分析
- 自动提取关键信息
- 关联多个证据
- 生成时间线分析

## 📊 性能优化

### 模型选择策略
- **性能优先**：选择响应时间最快的模型
- **质量优先**：选择分析质量最高的模型
- **成本优化**：选择资源消耗最少的模型
- **负载均衡**：均匀分配任务到各模型

### 缓存机制
- 响应结果缓存
- 模型状态缓存
- 智能缓存失效

### 并发控制
- 任务队列管理
- 资源限制控制
- 优先级调度

## 🔍 监控和日志

### 性能指标
- 模型响应时间
- 任务成功率
- 资源使用率
- 错误统计

### 日志记录
- 详细的操作日志
- 错误和警告信息
- 性能指标记录
- 审计追踪

## 🛡️ 安全考虑

### 数据保护
- 本地模型部署，数据不出网
- 加密通信支持
- 访问权限控制
- 敏感信息脱敏

### 输入验证
- SQL注入防护
- XSS攻击防护
- 路径遍历防护
- 输入大小限制

## 🧪 测试

### 单元测试
```bash
cd build
ctest --verbose
```

### 集成测试
```bash
./test_integration
```

### 性能测试
```bash
./benchmark_models
```

## 📚 文档

- [API参考文档](docs/api_reference.md)
- [配置指南](docs/configuration.md)
- [部署指南](docs/deployment.md)
- [故障排除](docs/troubleshooting.md)

## 🤝 贡献

欢迎提交问题报告和功能请求！

### 开发环境设置
```bash
git clone <repository>
cd ForensicsProject/LLMIntegration
git submodule update --init --recursive
```

### 代码规范
- 遵循Google C++代码规范
- 使用clang-format格式化代码
- 添加适当的单元测试
- 更新相关文档

## 📄 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 🙏 致谢

- [LM Studio](https://lmstudio.ai/) - 本地LLM部署平台
- [The Sleuth Kit](https://www.sleuthkit.org/) - 数字取证核心库
- [nlohmann/json](https://github.com/nlohmann/json) - JSON处理库

## 📞 支持

如有问题或建议，请通过以下方式联系：
- 提交GitHub Issue
- 发送邮件至：support@forensicsproject.com
- 查看在线文档：https://docs.forensicsproject.com/llm-integration

---

**注意**：本模块需要在本地运行LM Studio，请确保有足够的计算资源运行选择的模型。对于生产环境，建议使用专用的GPU服务器。