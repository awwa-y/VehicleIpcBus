// ============================================================================
// MessageBus.cpp - Phase 2 骨架：REG / REQ / RSP 跑通
// 扩展点（你手搓）：订阅表 NOTIFY 指定客户端；heartbeat sweep；多Provider负载均衡
// ============================================================================
#include "vipc/MessageBus.h"
#include "vipc/ServiceIds.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>
#include <chrono>

namespace vipc {

MessageBus::MessageBus(const std::string& sock_path)
    : m_path(sock_path)
{
}

MessageBus::~MessageBus()
{
    stop();
}

static int setNonBlock(int fd)
{
    int f = fcntl(fd, F_GETFL, 0);
    if (f < 0) return -1;
    return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

static uint64_t nowMs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

bool MessageBus::start()
{
    // 1. 清理遗留 sock 文件
    ::unlink(m_path.c_str());

    m_listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_listen_fd < 0) { std::perror("[Bus] socket"); return false; }
    setNonBlock(m_listen_fd);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("[Bus] bind");
        ::close(m_listen_fd); m_listen_fd = -1; return false;
    }
    if (::listen(m_listen_fd, 32) < 0) {
        std::perror("[Bus] listen");
        ::close(m_listen_fd); m_listen_fd = -1; return false;
    }

    m_epfd = ::epoll_create1(0);
    if (m_epfd < 0) { std::perror("[Bus] epoll_create1"); return false; }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = m_listen_fd;
    ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_listen_fd, &ev);

    m_running = true;
    m_thread = std::thread(&MessageBus::epollLoop, this);
    return true;
}

void MessageBus::stop()
{
    m_running = false;
    if (m_listen_fd >= 0) ::shutdown(m_listen_fd, SHUT_RDWR);
    if (m_thread.joinable()) m_thread.join();
    if (m_epfd >= 0) { ::close(m_epfd); m_epfd = -1; }
    if (m_listen_fd >= 0) { ::close(m_listen_fd); m_listen_fd = -1; ::unlink(m_path.c_str()); }
    std::lock_guard<std::mutex> lk(m_clients_mtx);
    for (int fd : m_clients) ::close(fd);
    m_clients.clear();
}

bool MessageBus::acceptNewClient()
{
    while (true) {
        sockaddr_un cli{};
        socklen_t len = sizeof(cli);
        int cfd = ::accept(m_listen_fd, reinterpret_cast<sockaddr*>(&cli), &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            std::perror("[Bus] accept"); return false;
        }
        setNonBlock(cfd);
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.fd = cfd;
        ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, cfd, &ev);
        {
            std::lock_guard<std::mutex> lk(m_clients_mtx);
            m_clients.push_back(cfd);
            m_last_hb[cfd] = nowMs();
        }
        std::printf("[Bus] client connected: fd=%d (total=%zu)\n", cfd, m_clients.size());
    }
    return true;
}

void MessageBus::removeClient(int fd)
{
    ::epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    {
        std::lock_guard<std::mutex> lk(m_clients_mtx);
        auto it = std::find(m_clients.begin(), m_clients.end(), fd);
        if (it != m_clients.end()) m_clients.erase(it);
        m_last_hb.erase(fd);
    }
    // 从路由表移除该 fd 提供的服务
    {
        std::lock_guard<std::mutex> lk(m_routes_mtx);
        for (auto it = m_routes.begin(); it != m_routes.end(); ) {
            if (it->second == fd) it = m_routes.erase(it);
            else ++it;
        }
    }
    // pending 清理（可选择回复 SERVICE_UNAVAILABLE）
    {
        std::lock_guard<std::mutex> lk(m_pending_mtx);
        for (auto it = m_pending.begin(); it != m_pending.end(); ) {
            if (it->second == fd) it = m_pending.erase(it);
            else ++it;
        }
    }
    std::printf("[Bus] client disconnected: fd=%d\n", fd);
}

static int recvFull(int fd, uint8_t* buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::read(fd, buf + got, len - got);
        if (n > 0) got += (size_t)n;
        else if (n == 0) return 0;
        else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
            return -1;
        }
    }
    return (int)got;
}

void MessageBus::forward(int dst_fd, const Header& hdr, const uint8_t* payload)
{
    // writev 可优化为单次 syscall，这里简单直接
    ::write(dst_fd, &hdr, sizeof(hdr));
    if (hdr.length > 0) ::write(dst_fd, payload, hdr.length);
}

void MessageBus::handleMessage(int fd, const Header& hdr, const uint8_t* payload)
{
    if (hdr.magic != kMagic) return;

    // ---- HEARTBEAT：刷新时间戳
    if (hdr.msg_type == MsgType::HEARTBEAT) {
        std::lock_guard<std::mutex> lk(m_clients_mtx);
        if (m_last_hb.count(fd)) m_last_hb[fd] = nowMs();
        return;
    }

    // ---- REGISTER：记录路由（payload = uint32_t service_id, BE）
    if (hdr.msg_type == MsgType::REGISTER && hdr.length == 4) {
        uint32_t svc =
            ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
            ((uint32_t)payload[2] << 8)  |  (uint32_t)payload[3];
        {
            std::lock_guard<std::mutex> lk(m_routes_mtx);
            m_routes[svc] = fd;
        }
        std::printf("[Bus] REGISTER: fd=%d → svc=0x%08x\n", fd, svc);
        // 可选：回ACK RESPONSE
        Header ack = hdr;
        ack.msg_type   = MsgType::RESPONSE;
        ack.length     = 0;
        ack.error_code = ErrorCode::OK;
        forward(fd, ack, nullptr);
        return;
    }

    // ---- REQUEST：路由 → Provider；记 pending
    if (hdr.msg_type == MsgType::REQUEST) {
        int provider = -1;
        {
            std::lock_guard<std::mutex> lk(m_routes_mtx);
            auto it = m_routes.find(hdr.service_id);
            if (it != m_routes.end()) provider = it->second;
        }
        if (provider < 0) {
            // 服务未注册，直接回 SERVICE_UNAVAILABLE
            Header rsp = hdr;
            rsp.msg_type   = MsgType::RESPONSE;
            rsp.length     = 0;
            rsp.error_code = ErrorCode::NOT_FOUND;
            forward(fd, rsp, nullptr);
            return;
        }
        {
            std::lock_guard<std::mutex> lk(m_pending_mtx);
            m_pending[hdr.client_id] = fd;           // 原始客户端
            m_pending_svc[hdr.client_id] = hdr.service_id;
        }
        forward(provider, hdr, payload);
        return;
    }

    // ---- RESPONSE：查 pending → 回原客户端
    if (hdr.msg_type == MsgType::RESPONSE) {
        int origin = -1;
        {
            std::lock_guard<std::mutex> lk(m_pending_mtx);
            auto it = m_pending.find(hdr.client_id);
            if (it != m_pending.end()) {
                origin = it->second;
                m_pending.erase(it);
                m_pending_svc.erase(hdr.client_id);
            }
        }
        if (origin > 0) forward(origin, hdr, payload);
        return;
    }

    // ---- NOTIFY：简化版广播给所有其他节点（Phase 2 可按订阅表精准推送）
    if (hdr.msg_type == MsgType::NOTIFY) {
        std::lock_guard<std::mutex> lk(m_clients_mtx);
        for (int c : m_clients) {
            if (c == fd) continue;
            forward(c, hdr, payload);
        }
        return;
    }
}

void MessageBus::epollLoop()
{
    epoll_event events[kMaxEvents];
    std::vector<uint8_t> rbuf(kHeaderSize + 65536);

    while (m_running) {
        int n = ::epoll_wait(m_epfd, events, kMaxEvents, 300);
        if (n <= 0) continue;

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == m_listen_fd) {
                if (events[i].events & EPOLLIN) acceptNewClient();
                continue;
            }

            if (events[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                removeClient(fd);
                continue;
            }
            if (!(events[i].events & EPOLLIN)) continue;

            while (true) {
                int rh = recvFull(fd, rbuf.data(), kHeaderSize);
                if (rh == 0) { removeClient(fd); break; }
                if (rh < 0) break;
                auto* h = reinterpret_cast<Header*>(rbuf.data());
                if (h->magic != kMagic) { removeClient(fd); break; }

                std::vector<uint8_t> payload(h->length);
                if (h->length > 0) {
                    int rp = recvFull(fd, payload.data(), h->length);
                    if (rp <= 0) { removeClient(fd); break; }
                }
                handleMessage(fd, *h, payload.data());
            }
        }
    }
}

} // namespace vipc
