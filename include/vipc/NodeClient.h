#pragma once
// ============================================================================
// NodeClient - 每个业务进程持有的"节点客户端"
// 职责：
//   1. 连接 MessageBus（Unix Domain Socket）
//   2. 独立 epoll 线程接收消息 → 回调通知上层
//   3. sendRequest() 自动分配 client_id，Response 匹配后调用回调
//   4. registerService() / publish() 辅助封装
// 注意：所有回调运行在 epoll 线程中，UI进程必须用 postEvent/QueuedConnection 跨线程！
// ============================================================================
#include "vipc/UdsHeader.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>

#ifdef __linux__
#  include <sys/epoll.h>
#else
// Windows 兼容占位（实际Windows开发建议用 WSL2 跑此中间件）
struct epoll_event { uint32_t events; int fd; };
#endif

namespace vipc {

class NodeClient {
public:
    // 收到"非Response"（REQUEST / NOTIFY / REGISTER 结果 / HEARTBEAT 应答）回调
    // 注意运行在 epoll 线程
    using IncomingCallback = std::function<void(
        uint32_t service_id,
        uint32_t method_id,
        uint32_t client_id,
        const uint8_t* payload,
        uint32_t length)>;

    // Response 专用回调（用于 sendRequest 配对）
    using ResponseCallback = std::function<void(
        ErrorCode err,
        const uint8_t* payload,
        uint32_t length)>;

    explicit NodeClient(const std::string& bus_sock_path);
    ~NodeClient();

    // 连接 Bus + 启动 epoll 线程（非阻塞）
    bool start();
    // 停线程 + 关 socket
    void stop();

    // ----- 常用接口（均线程安全）------
    // 向 Bus 注册 "本节点提供 service_id 服务"
    bool registerService(uint32_t service_id);

    // 发 REQUEST：Bus 路由 → 对应 Provider → Response 回来触发 on_resp
    // timeout_ms 未实现（可自行加timerfd扩展）
    uint32_t sendRequest(uint32_t service_id,
                         uint32_t method_id,
                         const uint8_t* payload,
                         uint32_t len,
                         ResponseCallback on_resp = nullptr);

    // 发 NOTIFY 广播（Bus转发给所有订阅者）
    bool publish(uint32_t service_id,
                 uint32_t method_id,
                 const uint8_t* payload,
                 uint32_t len);

    // 设置 Incoming 回调（收到非Response消息时）
    void setIncomingHandler(IncomingCallback cb);

private:
    void epollLoop();
    bool sendRaw(const Header& hdr, const uint8_t* payload);
    // 返回 0 表示 EOF/断开；<0 错误；>0 读满
    int  recvFull(uint8_t* buf, size_t len);
    uint32_t nextClientId();

    std::string   m_bus_path;
    int           m_sock_fd = -1;
    int           m_epfd    = -1;

    std::thread         m_thread;
    std::atomic<bool>   m_running{false};

    std::mutex  m_send_mtx;
    std::mutex  m_inc_mtx;
    std::mutex  m_pending_mtx;

    IncomingCallback m_inc_cb;
    std::unordered_map<uint32_t, ResponseCallback> m_pending;
    std::atomic<uint32_t> m_client_seq{1};

    static constexpr int kMaxEvents = 64;
};

} // namespace vipc
