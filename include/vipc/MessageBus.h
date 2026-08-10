#pragma once
// ============================================================================
// MessageBus - 总线 Master 单进程（可执行程序入口见 examples/messagebus/main.cpp）
// 职责：
//   1. 绑定固定 UDS 路径，接受所有节点 connect
//   2. REGISTER → 维护路由表 m_routes[service_id] = provider_fd
//   3. REQUEST  → 查路由表转发给 Provider；记录 pending[client_id] = original_client_fd
//   4. RESPONSE → 按 pending 回传给原客户端
//   5. NOTIFY   → 广播所有订阅（Phase 2 TODO: 订阅表），当前简化广播所有节点
//   6. HEARTBEAT + 超时清理（Phase 3 TODO）
// ============================================================================
#include "vipc/UdsHeader.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <list>
#include <cstdint>

namespace vipc {

class MessageBus {
public:
    explicit MessageBus(const std::string& sock_path);
    ~MessageBus();

    // bind/listen + 启动 epoll 线程（非阻塞）
    bool start();
    void stop();

private:
    void epollLoop();
    bool acceptNewClient();
    void handleMessage(int fd, const Header& hdr, const uint8_t* payload);
    void forward(int dst_fd, const Header& hdr, const uint8_t* payload);
    void removeClient(int fd);

    std::string m_path;
    int m_listen_fd = -1;
    int m_epfd      = -1;

    std::thread         m_thread;
    std::atomic<bool>   m_running{false};

    // 所有已连接的客户端 fd（可用于 NOTIFY 广播）
    std::mutex              m_clients_mtx;
    std::vector<int>        m_clients;

    // ServiceID → Provider fd（简化：一服务一Provider，可扩展为 list+负载均衡）
    std::mutex                                    m_routes_mtx;
    std::unordered_map<uint32_t, int>             m_routes;

    // client_id → original_client_fd（REQUEST原客户端，Response原路返回）
    std::mutex                                    m_pending_mtx;
    std::unordered_map<uint32_t, int>             m_pending;
    // client_id → original_request_service_id（调试用）
    std::unordered_map<uint32_t, uint32_t>        m_pending_svc;

    // fd → 最后心跳 ms（Phase 3 TODO：定期sweep）
    std::unordered_map<int, uint64_t>             m_last_hb;

    static constexpr int kMaxEvents = 128;
};

} // namespace vipc
