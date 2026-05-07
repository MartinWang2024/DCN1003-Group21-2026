DCN1003-Group21-2026/
├── Common/
│   ├── include/
    │   ├── protocol.h        ← 通信协议（模块6）
    │   │
    │   └──
│   └── src
├── Client/
│   └── main.cpp          ← 查询模块（模块2）+ 用户登录交互（模块3）
├── Driver/
│   ├── protocol.h        ← 通信协议（模块6）
│   └── protocol.cpp
├── Service/
│   ├── main.cpp          ← 网络/并发（模块5）
│   ├── course_store.h    ← 数据库模块（模块1）
│   ├── course_store.cpp  ← 增删改查（模块4）
│   └── auth.h/cpp        ← 用户认证（模块3 服务端部分）
└── Data/
    └── timetable.db

### 开发阶段拆解

**阶段一：基础通信**

* 建立 Winsock TCP server，`accept` 后为每个连接创建 `std::thread`
* 客户端连接后能收发字符串，验证基本链路

**阶段二：协议 & 数据库**

* 定义协议格式（参考作业示例），写解析函数
* 实现 CSV 读写 + 内存中的 `std::vector<Course>` 缓存
* 用 `std::mutex` 保护共享数据

**阶段三：功能模块**

* `LOGIN` → 验证用户角色（student/admin）
* `QUERY CODE xxx` / `QUERY INSTRUCTOR xxx` / `QUERY ALL`
* `ADD` / `UPDATE` / `DELETE`（需 admin 权限）

**阶段四：日志 & 错误处理**

* 服务端记录连接日志、操作日志到文件
* 非法请求返回 `ERROR <message>`，防崩溃

**阶段五（Bonus）**

* 按时间段搜索：`QUERY TIME Mon-10:00`
* 简单加密：XOR 或 Base64 混淆传输内容
