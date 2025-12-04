# AndroidAnalyzer 模块

位置: `AndroidAnalyzer/`

目的
- 解析 Android 镜像/备份中应用数据（如 SMS、联系人、通话记录、应用数据等）。

主要文件
- `AndroidAnalyzer.h`, `AndroidAnalyzer.cpp`

实现要点
- 重点处理 `/data/data/*` 下的 SQLite 数据库和应用私有文件
- 提供基于文件路径和已知数据库结构的解析器

测试建议
- 使用脱敏或合规的测试备份验证解析结果字段

扩展点
- 添加对常见应用（WhatsApp, WeChat 等）数据库的专用解析器
- 支持 APK 签名与证书分析
