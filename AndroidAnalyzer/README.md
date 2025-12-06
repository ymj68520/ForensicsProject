# AndroidAnalyzer 模块

位置: `AndroidAnalyzer/`

目的
- 解析 Android 镜像/备份中应用数据（如 SMS、联系人、通话记录、应用数据等）。
- 分析 Android 系统目录（/system/）中的系统应用、配置文件和框架文件。
- 将分析结果存储在专用数据库中以便后续查询和分析。

主要文件
- `AndroidAnalyzer.h`, `AndroidAnalyzer.cpp`
- `AndroidAnalysisDatabase` - 存储Android分析结果（含系统和用户数据）的专用数据库类

实现要点
- 重点处理 `/data/data/*` 下的 SQLite 数据库和应用私有文件
- 分析 `/system/app/` 和 `/system/priv-app/` 下的系统APK文件
- 解析 `/system/build.prop` 配置文件
- 扫描 `/system/framework/` 目录中的框架文件
- 将所有分析结果存储在 `*_android.db` 数据库中
- 提供基于文件路径和已知数据库结构的解析器

功能列表
1. **用户数据分析**:
   - SMS/MMS (`mmssms.db`)
   - 联系人 (`contacts2.db`)
   - 通话记录 (`calllog.db`)
   - WhatsApp 消息 (`msgstore.db`)
   - Chrome 浏览历史 (`History`)
   - WiFi 配置信息 (`WifiConfigStore.xml` / `wpa_supplicant.conf`)
   - 已安装应用列表 (`packages.xml`)

2. **系统数据分析**:
   - Build.prop 属性
   - 系统应用 (System Apps)
   - 框架文件 (Framework Files)

数据库表结构
- `sms_messages` - 短信记录
- `contacts` - 联系人信息
- `call_logs` - 通话记录
- `whatsapp_messages` - WhatsApp 聊天记录
- `wifi_networks` - WiFi 网络配置
- `chrome_history` - 浏览器历史
- `installed_packages` - 已安装应用信息
- `system_build_properties` - 系统构建属性
- `system_apps` - 系统应用信息
- `framework_files` - 框架文件信息

扩展点
- 添加更多第三方应用（WeChat, Telegram 等）的专用解析器
- 支持 APK 签名与证书分析
- 增强 Timeline 分析