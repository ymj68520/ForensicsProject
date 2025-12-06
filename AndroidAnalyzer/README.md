# AndroidAnalyzer 模块

位置: `AndroidAnalyzer/`

目的
- 解析 Android 镜像/备份中应用数据（如 SMS、联系人、通话记录、应用数据等）。
- 分析 Android 系统目录（/system/）中的系统应用、配置文件和框架文件。
- 将分析结果存储在专用数据库中以便后续查询和分析。

主要文件
- `AndroidAnalyzer.h`, `AndroidAnalyzer.cpp`
- `SystemAnalysisDatabase` - 存储系统分析结果的专用数据库类

实现要点
- 重点处理 `/data/data/*` 下的 SQLite 数据库和应用私有文件
- 分析 `/system/app/` 和 `/system/priv-app/` 下的系统APK文件
- 解析 `/system/build.prop` 配置文件
- 扫描 `/system/framework/` 目录中的框架文件
- 将所有分析结果存储在 `*_system_analysis.db` 数据库中
- 提供基于文件路径和已知数据库结构的解析器

数据库表结构
- `system_build_properties` - 系统构建属性 (key, value)
- `system_apps` - 系统应用信息 (package_name, apk_path, version_info, privileges)
- `framework_files` - 框架文件信息 (file_name, file_path, file_type, file_size)

测试建议
- 使用脱敏或合规的测试备份验证解析结果字段
- 验证数据库中的数据完整性和准确性

扩展点
- 添加对常见应用（WhatsApp, WeChat 等）数据库的专用解析器
- 支持 APK 签名与证书分析
- 增强系统目录分析功能
