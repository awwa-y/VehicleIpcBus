# VehicleIpcBus — 车载微服务通用通信中间件

面向汽车电子场景的轻量级进程间通信（IPC）框架，对标 ROS 2 / AUTOSAR Adaptive 的核心理念，
用于将单体车载HMI拆分为 **HMI_UI / CAN_Gateway / State_Manager / Data_Logger** 四个独立节点，
通过 **Unix Domain Socket + epoll** 实现高可靠、低延迟的进程间RPC与事件通知。

> 本仓库为通用开源中间件，不包含任何具体业务代码；其设计来源于对某工程机械车载HMI（CabPad类项目）
> 架构升级实践，详细设计文档见 [CabPad-Refactor-Doc](https://github.com/awwa-y/CabPad-Refactor-Doc)。

---

## ✨ 特性

- **零拷贝二进制协议**：25字节固定Header类SOME/IP格式，魔数+校验防错
- **UDS + epoll 异步事件循环**：不走网络协议栈，本地回环延迟 < 10μs
- **MessageBus 服务注册与发现**：节点只需知道 ServiceID，无需关心对端 socket 地址
- **Request-Response 自动匹配**：ClientID 机制，支持超时回调
- **心跳保活 + 故障隔离**：Provider 超时自动从路由表剔除，下游节点收到降级通知
- **Qt 跨线程安全桥接**：`IpcBridge + postEvent`，epoll 线程永不阻塞UI
- **双构建系统**：qmake（主，适配Qt老项目） + CMake（可选）

---

## 🏗 架构拓扑

```
 ┌──────────────┐   Request/Notify    ┌────────────────┐  Request/Notify  ┌────────────────┐
 │   HMI_UI     │◄──────────────────►│                │◄────────────────►│ CAN_Gateway    │
 │  (Qt UI)     │   Unix Domain Sock  │  MessageBus    │                  │ (RawCan/epoll) │
 └──────────────┘                    │  (Master,      │                  └────────────────┘
                                     │   路由+心跳)    │
 ┌──────────────┐   Subscribe/Notify │                │  Subscribe/Notify ┌────────────────┐
 │ State_Mgr    │◄──────────────────►│                │◄────────────────►│ Data_Logger    │
 │ (状态机/故障)│                    └────────────────┘                  │ (异步落盘)     │
 └──────────────┘                                                          └────────────────┘
         ▲
         └─ 跨机扩展：SomeIpGateway (TCP ↔ 车云平台)
```

---

## 📦 编译与运行

### 方式一：qmake（推荐，简单稳）
```bash
mkdir build && cd build
qmake ../VehicleIpcBus.pro
make -j$(nproc)
# 产物：
#   libvipc.a
#   examples/01_hello_rpc/vipc_demo_client
#   examples/01_hello_rpc/vipc_demo_server
#   examples/messagebus/vipc_messagebus
#   examples/mock_hmi/mock_hmi   （Qt存在时）
```

### 方式二：CMake（可选）
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## 🚀 30秒跑通 Demo

```bash
# 终端1：启动 MessageBus
./examples/messagebus/vipc_messagebus /tmp/cabpad_bus.sock &

# 终端2：启动服务端（模拟CAN_Gateway）
./examples/01_hello_rpc/vipc_demo_server /tmp/cabpad_bus.sock &

# 终端3：启动客户端（模拟HMI发请求）
./examples/01_hello_rpc/vipc_demo_client /tmp/cabpad_bus.sock
# 期望输出:
#   [Client] Sending REQUEST: "SEND_FRAME 0x17FE0001"
#   [Client] OK Response(seq=1): "ACK can_id=0x17FE0001 sent"
```

---

## 📊 压测报告（SomeIpGateway 车云网关）

压测环境：Intel i7-12700 / 32GB / Ubuntu 22.04

| 指标 | 数值 |
|---|---|
| 长连接并发数 | 5,000 |
| 平均 RPC 延迟 | < 1 ms |
| 峰值 QPS | 120,000 req/s |
| 进程内存占用（稳态） | < 60 MB |

> 详细压测脚本见 `SomeIpGateway` 仓库。

---

## 📁 目录结构

```
VehicleIpcBus/
├── include/vipc/        公共头（对外SDK）
│   ├── UdsHeader.h      25字节二进制Header定义
│   ├── ServiceIds.h     ServiceID / MethodID 枚举
│   ├── NodeClient.h     节点客户端（业务进程使用）
│   └── MessageBus.h     总线Master（服务注册/路由/心跳）
├── src/                 核心实现
│   ├── NodeClient.cpp
│   └── MessageBus.cpp
├── examples/
│   ├── 01_hello_rpc/    HelloWorld RPC
│   ├── 02_pub_sub/      发布订阅（TODO，自行实现参考文档）
│   ├── 03_fault_isolation/ 心跳故障隔离（TODO）
│   ├── messagebus/      MessageBus 可执行程序入口
│   └── mock_hmi/        Qt 最小HMI演示（IpcBridge跨线程）
├── tests/               单元测试（TODO）
├── docs/                协议设计文档
├── VehicleIpcBus.pro    qmake 根构建
└── CMakeLists.txt       CMake 根构建
```

---

## 🔗 相关仓库

| 仓库 | 说明 |
|---|---|
| [CabPad-Refactor-Doc](https://github.com/awwa-y/CabPad-Refactor-Doc) | 架构升级设计文档（6篇） |
| [SomeIpGateway](https://github.com/awwa-y/SomeIpGateway) | SOME/IP风格车云通信epoll网关 |
