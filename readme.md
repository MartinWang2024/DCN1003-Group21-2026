
DCN1003-Group21-2026/
├── Client/
│ ├── [CMakeLists.txt](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
│ ├── include/
│ │ └── client.h
│ └── src/
│ └── client.cpp
├── Driver/
│ ├── [CMakeLists.txt](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
│ ├── include/
│ │ ├── database.h ← 数据访问接口
│ │ └── protocol.h ← 通信协议
│ └── src/
│ ├── database.cpp
│ ├── error.cpp
│ └── protocol.cpp
├── Service/
│ ├── [CMakeLists.txt](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
│ ├── include/
│ │ └── service.h ← 服务端接口（网络/并发相关）
│ └── src/
│ └── service.cpp
├── Tests/ ← 测试
│ ├── [CMakeLists.txt](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
│ ├── test_database.cpp
│ └── test_database_file.cpp
├── data/
│ ├── courses.db
│ └── admins.db
├── extern/
│ └── sqlite3/
│ ├── shell.c
│ ├── sqlite3.c
│ ├── sqlite3.h
│ └── sqlite3ext.h
├── build/ ← CMake 构建输出目录
├── cmake-build-debug/ ← CLion/Debug 构建目录
├── [CMakeLists.txt](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
└── [readme.md](vscode-file://vscode-app/d:/App/Microsoft%20VS%20Code/8b640eef5a/resources/app/out/vs/code/electron-browser/workbench/workbench.html)


### 开发阶段拆解

**阶段一：基础通信**

* 建立 Winsock TCP server，`accept` 后为每个连接创建 `std::thread`
* 客户端连接后能收发字符串，验证基本链路

**阶段二：协议 & 数据库**

* 定义协议格式，写解析函数
* 实现 SQLite读写 + 内存中的 `std::vector<Course>` 缓存？
* 用 `std::mutex` 保护共享数据

**阶段三：功能模块**

* `LOGIN` → 验证用户角色（student/admin）
* 实现基础的增删改查
* `ADD` / `UPDATE` / `DELETE`（需 admin 权限）

**阶段四：日志 & 错误处理**

* 服务端记录连接日志、操作日志到文件
* 非法请求返回 `ERROR <message>`，防崩溃

**阶段五（Bonus）**

* 按时间段搜索：`QUERY TIME Mon-10:00`
* 简单加密：XOR 或 Base64 混淆传输内容
