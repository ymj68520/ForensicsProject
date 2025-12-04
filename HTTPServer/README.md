# HTTPServer 模块

位置: `HTTPServer/`

目的
- 提供 RESTful API 管理异步分析任务并查询结果，基于 Crow + Boost.Asio。

主要文件
- `HTTPserver.h`, `HTTPserver.cpp`
- `TaskManager.h`
- `SQLiteHelper.h`
- `Utils.h`

API 端点
- `POST /tasks` - 创建分析任务
- `GET /tasks/{id}` - 查询任务状态
- `GET /tasks/{id}/results` - 获取任务结果（若完成）

运行示例
```bash
./forensic_analyzer --http-server 8080
curl -X POST http://localhost:8080/tasks -H "Content-Type: application/json" -d '{"image_path":"/path/image.e01"}'
```

实现要点
- `HTTPserver` 使用 `crow::App<>` 定义路由并返回 JSON
- `TaskManager` 为任务状态的唯一来源
- 对大结果集使用分页避免内存峰值

安全建议
- 在生产环境前端加 nginx/TLS 并做鉴权
- 将执行任务与 HTTP 服务分离到 worker 后台进程或队列

扩展点
- 对接消息队列（RabbitMQ/Redis）实现分布式任务执行
- 添加认证与访问控制
