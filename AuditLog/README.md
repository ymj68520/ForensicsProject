# AuditLog 模块

## 概述

AuditLog 是一个独立的审计日志模块，为 ForensicsProject 提供高效、持久化的操作日志记录功能。该模块专为小内存设备优化，使用文件持久化存储和智能缓存策略，确保在资源受限环境下的高性能运行。

## 设计特性

### 核心功能
- **SQLite 持久化存储**：所有日志条目存储在本地 SQLite 数据库中
- **内存缓存优化**：
  - 写缓冲：批量写入减少磁盘 I/O
  - 读缓存：LRU 策略缓存最近查询的日志
- **异步后台刷写**：可选的后台线程定期刷写缓存
- **线程安全**：完全线程安全，支持多线程并发访问
- **日志管理**：支持日志轮转、自动清理和导出

### 性能优化
- 批量插入（默认 50 条/批）
- 预编译 SQL 语句
- SQLite WAL 模式（提升并发性能）
- 可配置的缓存大小（默认 100 条）
- 索引优化（task_id, timestamp, action）

### 内存占用
- 默认配置下内存占用 < 1MB
- 写缓冲：~50 条 × ~200 字节 = ~10KB
- 读缓存：~100 条 × ~200 字节 = ~20KB
- 其他开销：< 1MB（数据库连接、线程栈等）

## 架构设计

### 数据结构

#### AuditLogEntry
```cpp
struct AuditLogEntry {
    int64_t id;                                          // 数据库主键
    std::string task_id;                                 // 关联的任务 ID
    std::chrono::system_clock::time_point timestamp;     // 时间戳
    std::string action;                                  // 操作类型
    std::string details;                                 // 详细信息
    std::string user_id;                                 // 用户标识
};
```

#### AuditLogConfig
```cpp
struct AuditLogConfig {
    std::string db_path = "forensics_audit.db";          // 数据库文件路径
    size_t cache_size = 100;                             // 读缓存条目数
    size_t batch_size = 1;                               // 批量写入阈值（1=立即写入，确保数据安全）
    int flush_interval_seconds = 3;                      // 自动刷写间隔（仅异步模式）
    bool async_write = false;                            // 默认禁用异步写入，确保数据安全
    size_t max_db_size_mb = 100;                         // 数据库大小限制
    int retention_days = 30;                             // 日志保留天数
    bool enable_wal = true;                              // 启用 WAL 模式
};
```

### 数据库 Schema
```sql
CREATE TABLE audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id TEXT NOT NULL,
    timestamp INTEGER NOT NULL,           -- Unix 时间戳（毫秒）
    action TEXT NOT NULL,
    details TEXT,
    user_id TEXT,
    created_at INTEGER DEFAULT (strftime('%s', 'now') * 1000)
);

CREATE INDEX idx_task_id ON audit_logs(task_id);
CREATE INDEX idx_timestamp ON audit_logs(timestamp);
CREATE INDEX idx_action ON audit_logs(action);
```

## API 参考

### 初始化

```cpp
// 使用默认配置初始化（首次调用时）
AuditLog::instance();

// 使用自定义配置初始化
AuditLogConfig config;
config.db_path = "/var/log/forensics/audit.db";
config.cache_size = 50;                    // 小内存设备减小缓存
config.retention_days = 7;                 // 只保留 7 天日志
config.async_write = true;                 // 启用异步写入
AuditLog::instance(config);
```

### 写入日志

```cpp
// 记录审计日志
AuditLog::instance().log(task_id, "CREATED", "Task created with priority 1");
AuditLog::instance().log(task_id, "STATUS_CHANGE", "Status changed to RUNNING");
AuditLog::instance().log(task_id, "ERROR", "Analysis failed: file not found", "user123");

// 强制刷写缓存到磁盘（关键操作后）
AuditLog::instance().flush();
```

### 查询日志

```cpp
// 获取特定任务的所有日志
auto logs = AuditLog::instance().getTaskLogs(task_id);

// 分页查询（前 20 条）
auto recent_logs = AuditLog::instance().getTaskLogs(task_id, 20, 0);

// 时间范围查询
auto start = std::chrono::system_clock::now() - std::chrono::hours(24);
auto end = std::chrono::system_clock::now();
auto logs_24h = AuditLog::instance().getLogsByTimeRange(start, end);

// 按操作类型查询
auto error_logs = AuditLog::instance().getLogsByAction("ERROR");

// 获取日志总数
int64_t total = AuditLog::instance().getLogCount();
int64_t task_count = AuditLog::instance().getLogCount(task_id);
```

### 统计信息

```cpp
// 获取统计信息（JSON 格式）
nlohmann::json stats = AuditLog::instance().getStatistics();
// 返回内容：
// {
//   "total_logs": 12345,
//   "by_action": {
//     "CREATED": 1000,
//     "STATUS_CHANGE": 5000,
//     "ERROR": 100
//   },
//   "db_size_mb": 15,
//   "cache_size": 85,
//   "cache_limit": 100,
//   "pending_writes": 12
// }
```

### 管理操作

```cpp
// 清理过期日志（使用配置的保留天数）
AuditLog::instance().cleanup();

// 清理指定天数之前的日志
AuditLog::instance().cleanup(7);  // 删除 7 天前的日志

// 日志轮转（数据库过大时自动备份并创建新数据库）
AuditLog::instance().rotate();

// 导出日志
AuditLog::instance().exportToFile("/tmp/audit_export.json", "json");
AuditLog::instance().exportToFile("/tmp/audit_export.csv", "csv");
```

## TaskManager 集成

### 使用方式

TaskManager 已集成 AuditLog 模块，无需修改现有代码即可使用：

```cpp
// 记录日志（向后兼容）
TaskManager::instance().add_audit_log(task_id, "CREATED", "Task created");

// 获取任务的审计日志
auto logs = TaskManager::instance().get_audit_logs(task_id);
auto recent_logs = TaskManager::instance().get_audit_logs(task_id, 50);  // 最近 50 条
```

### 迁移说明

从内存存储迁移到持久化存储的变化：

**之前**：
- `AnalysisTask` 结构包含 `std::vector<AuditLogEntry> audit_log`
- 日志存储在内存中，进程重启后丢失
- 大量任务时内存占用高

**之后**：
- `AnalysisTask` 不再包含 `audit_log` 成员
- 日志持久化到数据库，进程重启后保留
- 内存占用极小（< 1MB），适合小内存设备
- 通过 `get_audit_logs()` 动态查询日志

## 模块集成

AuditLog 模块已集成到 ForensicsProject 的所有核心模块中。以下是各模块的审计日志操作类型：

### ImageAnalyzer

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `IMAGE_OPEN` | 镜像打开成功 | 成功打开磁盘镜像文件 |
| `FS_OPEN` | 文件系统检测 | 成功识别文件系统类型 |
| `EXTRACTION_COMPLETE` | 提取完成 | 文件系统遍历完成 |
| `XFS_EXTRACTION_COMPLETE` | XFS 提取完成 | XFS 文件系统提取完成 |
| `NATIVE_EXTRACTION_COMPLETE` | 原生挂载提取完成 | Linux 原生挂载提取完成 |

### DatabaseManager

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `DB_INIT` | 数据库初始化成功 | 成功打开和初始化数据库 |
| `DB_INIT_FAILED` | 数据库初始化失败 | 无法打开数据库文件 |

### EventExtractor

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `EVENT_EXTRACTION_START` | 事件提取开始 | 开始从源数据库提取事件 |
| `EVENT_EXTRACTION_COMPLETE` | 事件提取完成 | 事件提取成功完成 |

### FileClassifier

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `CLASSIFICATION_START` | 文件分类开始 | 开始对文件进行分类 |
| `CLASSIFICATION_COMPLETE` | 文件分类完成 | 文件分类成功完成 |

### FileExtractor

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `EXTRACTOR_INIT` | 提取器初始化成功 | 文件提取器初始化完成 |
| `EXTRACTOR_INIT_FAILED` | 提取器初始化失败 | 无法初始化文件提取器 |

### AndroidAnalyzer

| Action | 说明 | 触发时机 |
|--------|------|----------|
| `ANDROID_INIT` | Android 分析器初始化成功 | Android 分析器初始化完成 |
| `ANDROID_INIT_FAILED` | Android 分析器初始化失败 | 无法初始化 Android 分析器 |
| `ANDROID_ANALYSIS_START` | Android 数据分析开始 | 开始分析 Android 数据 |
| `ANDROID_ANALYSIS_COMPLETE` | Android 数据分析完成 | Android 数据分析成功完成 |

### 在自定义模块中使用

```cpp
#include "AuditLog/AuditLog.h"

void myCustomFunction() {
    // 记录操作开始
    AuditLog::instance().log("SYSTEM", "CUSTOM_OP_START", "Starting custom operation");
    
    // 执行操作...
    
    // 记录操作完成
    AuditLog::instance().log("SYSTEM", "CUSTOM_OP_COMPLETE", "Custom operation completed successfully");
}
```

## 使用示例

### 示例 1：基本使用

```cpp
#include "AuditLog/AuditLog.h"

int main() {
    // 初始化（程序启动时）
    AuditLogConfig config;
    config.db_path = "forensics_audit.db";
    config.cache_size = 100;
    AuditLog::instance(config);
    
    // 记录日志
    std::string task_id = "task-12345";
    AuditLog::instance().log(task_id, "CREATED", "Task created");
    AuditLog::instance().log(task_id, "RUNNING", "Analysis started");
    
    // 查询日志
    auto logs = AuditLog::instance().getTaskLogs(task_id);
    for (const auto& log : logs) {
        std::cout << log.action << ": " << log.details << std::endl;
    }
    
    // 程序退出时自动刷写缓存
    return 0;
}
```

### 示例 2：定期清理

```cpp
#include <thread>
#include <chrono>

// 后台清理线程
void cleanupThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
        
        // 每天清理一次过期日志
        AuditLog::instance().cleanup();
        
        // 检查数据库大小，必要时轮转
        auto stats = AuditLog::instance().getStatistics();
        if (stats["db_size_mb"] > 90) {  // 接近 100MB 限制
            AuditLog::instance().rotate();
        }
    }
}
```

### 示例 3：导出审计报告

```cpp
// 导出最近 7 天的日志
auto start = std::chrono::system_clock::now() - std::chrono::hours(7 * 24);
auto end = std::chrono::system_clock::now();
auto logs = AuditLog::instance().getLogsByTimeRange(start, end);

// 生成 JSON 报告
nlohmann::json report = {
    {"generated_at", std::chrono::system_clock::to_time_t(end)},
    {"period", "Last 7 days"},
    {"total_entries", logs.size()},
    {"entries", logs}
};

std::ofstream ofs("audit_report.json");
ofs << report.dump(2);
ofs.close();
```

### 示例 4：查询模块操作日志

```cpp
// 查询所有镜像打开操作
auto image_opens = AuditLog::instance().getLogsByAction("IMAGE_OPEN");
std::cout << "Total images opened: " << image_opens.size() << std::endl;

// 查询最近的 Android 分析
auto android_logs = AuditLog::instance().getLogsByAction("ANDROID_ANALYSIS_COMPLETE", 10, 0);
for (const auto& log : android_logs) {
    std::cout << "Analysis completed: " << log.details << std::endl;
}
```

## 性能基准

### 写入性能
- **单条写入**：~0.1ms（写入缓冲）
- **批量写入**（50 条）：~5ms（刷写到数据库）
- **异步模式**：写入操作几乎不阻塞

### 查询性能
- **按 task_id 查询**（有索引）：~1ms（100 条）
- **时间范围查询**（有索引）：~2ms（1000 条）
- **缓存命中**：~0.01ms（内存读取）

### 内存占用
- **默认配置**：< 1MB
- **小内存配置**（cache_size=20, batch_size=10）：< 200KB

## 故障排查

### 常见问题

**问题 1：数据库锁定错误**
```
Error: database is locked
```
**解决方案**：
- 确保启用了 WAL 模式（`config.enable_wal = true`）
- 避免在多个进程同时写入同一数据库
- 检查文件权限

**问题 2：内存占用过高**
```
Cache growing too large
```
**解决方案**：
- 减小 `cache_size`（例如从 100 降到 50）
- 减小 `batch_size`（例如从 50 降到 20）
- 增加 `flush_interval_seconds` 频率

**问题 3：数据库文件过大**
```
Database file exceeds limit
```
**解决方案**：
- 调用 `cleanup()` 删除过期日志
- 调用 `rotate()` 轮转数据库
- 减小 `retention_days`

## 配置建议

### 高性能服务器
```cpp
AuditLogConfig config;
config.cache_size = 500;
config.batch_size = 100;
config.flush_interval_seconds = 10;
config.async_write = true;
config.retention_days = 90;
```

### 小内存设备（如嵌入式系统）
```cpp
AuditLogConfig config;
config.cache_size = 20;
config.batch_size = 10;
config.flush_interval_seconds = 3;
config.async_write = true;
config.retention_days = 7;
config.max_db_size_mb = 10;
```

### 高可靠性（关键系统）
```cpp
AuditLogConfig config;
config.cache_size = 50;
config.batch_size = 20;
config.flush_interval_seconds = 1;    // 频繁刷写
config.async_write = false;           // 同步写入
config.enable_wal = true;
config.retention_days = 365;
```

## 线程安全

所有 AuditLog 方法都是线程安全的：
- `log()` - 可从多个线程并发调用
- `getTaskLogs()` - 可与 `log()` 并发
- `flush()` - 可安全调用
- `cleanup()` 和 `rotate()` - 建议在低负载时调用

## 依赖项

- **SQLite3**：数据库存储
- **nlohmann/json**：JSON 序列化
- **C++20**：标准库功能（filesystem, chrono, thread）

## 未来扩展

- [ ] 支持远程日志发送（syslog, HTTP）
- [ ] 实时日志流（WebSocket）
- [ ] 日志加密存储
- [ ] 多数据库后端支持（PostgreSQL, MongoDB）
- [ ] 日志分析和异常检测
- [ ] 压缩归档（自动压缩旧日志）

## 许可证

与 ForensicsProject 主项目相同。

## 作者

ForensicsProject 开发团队

## 更新日志

### v1.1.1 (2025-12-19) - 关键修复
- **修复：程序终止时数据丢失问题**
  - 修改默认配置：`batch_size=1`（立即写入）、`async_write=false`（同步模式）
  - 添加信号处理器（SIGINT、SIGTERM、SIGHUP）确保优雅关闭
  - 添加 `atexit` 处理器确保正常退出时刷新缓存
- 现在每条日志都立即写入数据库，彻底防止数据丢失
- 高性能场景可启用 `async_write=true` 并增大 `batch_size`

### v1.1.0 (2025-12-19)
- 扩展 AuditLog 到所有核心模块
  - ImageAnalyzer：镜像打开、文件系统检测、提取完成
  - DatabaseManager：数据库初始化
  - EventExtractor：事件提取开始/完成
  - FileClassifier：文件分类开始/完成
  - FileExtractor：提取器初始化
  - AndroidAnalyzer：Android 分析器初始化、分析开始/完成
- 新增模块集成文档
- 新增示例：查询模块操作日志

### v1.0.0 (2025-12-18)
- 初始版本发布
- SQLite 持久化存储
- 写缓冲 + LRU 读缓存
- 异步后台刷写
- 日志轮转和清理
- JSON/CSV 导出

