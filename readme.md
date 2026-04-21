project/
├── server/
│   ├── main.cpp          # 服务器入口，accept循环
│   ├── client_handler.cpp # 每个客户端线程的处理逻辑
│   ├── database.cpp       # 数据库读写、内存缓存
│   ├── auth.cpp           # 用户认证
│   └── logger.cpp         # 日志
├── client/
│   ├── main.cpp           # 客户端入口，交互式命令行
│   └── protocol.cpp       # 协议封装（发送/接收）
└── data/
    ├── timetable.csv
    └── users.csv


### 开发阶段拆解

**阶段一：基础通信（2天）**

* 建立 Winsock TCP server，`accept` 后为每个连接创建 `std::thread`
* 客户端连接后能收发字符串，验证基本链路

**阶段二：协议 & 数据库（2天）**

* 定义协议格式（参考作业示例），写解析函数
* 实现 CSV 读写 + 内存中的 `std::vector<Course>` 缓存
* 用 `std::mutex` 保护共享数据

**阶段三：功能模块（3天）**

* `LOGIN` → 验证用户角色（student/admin）
* `QUERY CODE xxx` / `QUERY INSTRUCTOR xxx` / `QUERY ALL`
* `ADD` / `UPDATE` / `DELETE`（需 admin 权限）

**阶段四：日志 & 错误处理（1天）**

* 服务端记录连接日志、操作日志到文件
* 非法请求返回 `ERROR <message>`，防崩溃

**阶段五（Bonus）**

* 按时间段搜索：`QUERY TIME Mon-10:00`
* 简单加密：XOR 或 Base64 混淆传输内容
