// ============================================================================
// NodeClient.cpp - 实现骨架（Phase 1 可扩展点已标注 TODO）
// 提示：你手搓时，可先实现"无MessageBus直接connect"，再加Bus路由
// ============================================================================
#include "vipc/NodeClient.h"
#include "vipc/ServiceIds.h"

#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <cerrno>

namespace vipc {

NodeClient::NodeClient(const std::string& bus_sock_path)
    : m_bus_path(bus_sock_path)
{
}

NodeClient::~NodeClient()
{
    stop();
}

static int setNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool NodeClient::start()
{
    if (m_running) return true;

    // 1. 创建 UDS socket
    m_sock_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_sock_fd < 0) {
        std::perror("[NodeClient] socket");
        return false;
    }
    setNonBlock(m_sock_fd);

    // 2. connect 到 MessageBus
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_bus_path.c_str(), sizeof(addr.sun_path) - 1);
    int rc = ::connect(m_sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        std::perror("[NodeClient] connect");
        ::close(m_sock_fd);
        m_sock_fd = -1;
        return false;
    }

    // 3. epoll_create + 监听 EPOLLIN
    m_epfd = ::epoll_create1(0);
    if (m_epfd < 0) {
        std::perror("[NodeClient] epoll_create1");
        ::close(m_sock_fd);
        m_sock_fd = -1;
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = m_sock_fd;
    ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_sock_fd, &ev);

    // 4. 启动 epoll 线程
    m_running = true;
    m_thread = std::thread(&NodeClient::epollLoop, this);
    return true;
}

void NodeClient::stop()
{
    m_running = false;
    if (m_epfd >= 0) {
        // 让 epoll_wait 立即返回（shutdown 更优雅）
        ::shutdown(m_sock_fd, SHUT_RDWR);
    }
    if (m_thread.joinable()) m_thread.join();
    if (m_epfd    >= 0) { ::close(m_epfd);    m_epfd = -1; }
    if (m_sock_fd >= 0) { ::close(m_sock_fd); m_sock_fd = -1; }
    std::lock_guard<std::mutex> lk(m_pending_mtx);
    m_pending.clear();
}

// ---- 发送封装 -------------------------------------------------------------
bool NodeClient::sendRaw(const Header& hdr, const uint8_t* payload)
{
    if (m_sock_fd < 0) return false;
    std::lock_guard<std::mutex> lk(m_send_mtx);
    // 连续 write。真实实现可用 writev 避免两次syscall。
    ssize_t w1 = ::write(m_sock_fd, &hdr, sizeof(hdr));
    if (w1 != (ssize_t)sizeof(hdr)) return false;
    if (hdr.length > 0) {
        ssize_t w2 = ::write(m_sock_fd, payload, hdr.length);
        if (w2 != (ssize_t)hdr.length) return false;
    }
    return true;
}

bool NodeClient::registerService(uint32_t service_id)
{
    Header hdr{};
    hdr.magic      = kMagic;
    hdr.service_id = SVC_MESSAGEBUS;
    hdr.method_id  = 0;
    hdr.length     = sizeof(uint32_t);
    hdr.client_id  = nextClientId();
    hdr.msg_type   = MsgType::REGISTER;
    hdr.error_code = ErrorCode::OK;

    uint8_t payload[4];
    payload[0] = (uint8_t)(service_id >> 24);
    payload[1] = (uint8_t)(service_id >> 16);
    payload[2] = (uint8_t)(service_id >> 8);
    payload[3] = (uint8_t)(service_id);
    return sendRaw(hdr, payload);
}

uint32_t NodeClient::sendRequest(uint32_t service_id, uint32_t method_id,
                                 const uint8_t* payload, uint32_t len,
                                 ResponseCallback on_resp)
{
    Header hdr{};
    hdr.magic      = kMagic;
    hdr.service_id = service_id;
    hdr.method_id  = method_id;
    hdr.length     = len;
    hdr.client_id  = nextClientId();
    hdr.msg_type   = MsgType::REQUEST;
    hdr.error_code = ErrorCode::OK;

    if (on_resp) {
        std::lock_guard<std::mutex> lk(m_pending_mtx);
        m_pending[hdr.client_id] = std::move(on_resp);
    }
    if (!sendRaw(hdr, payload)) {
        if (on_resp) {
            std::lock_guard<std::mutex> lk(m_pending_mtx);
            m_pending.erase(hdr.client_id);
        }
        return 0;
    }
    return hdr.client_id;
}

bool NodeClient::publish(uint32_t service_id, uint32_t method_id,
                         const uint8_t* payload, uint32_t len)
{
    Header hdr{};
    hdr.magic      = kMagic;
    hdr.service_id = service_id;
    hdr.method_id  = method_id;
    hdr.length     = len;
    hdr.client_id  = 0;  // NOTIFY 无配对
    hdr.msg_type   = MsgType::NOTIFY;
    hdr.error_code = ErrorCode::OK;
    return sendRaw(hdr, payload);
}

void NodeClient::setIncomingHandler(IncomingCallback cb)
{
    std::lock_guard<std::mutex> lk(m_inc_mtx);
    m_inc_cb = std::move(cb);
}

// ---- 接收循环（epoll线程）------------------------------------------------
int NodeClient::recvFull(uint8_t* buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        ssize_t n = ::read(m_sock_fd, buf + got, len - got);
        if (n > 0) {
            got += (size_t)n;
        } else if (n == 0) {
            return 0; // EOF
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -1; // 稍后再试
            return -1;
        }
    }
    return (int)got;
}

void NodeClient::epollLoop()
{
    epoll_event events[kMaxEvents];
    // 接收缓冲区：Header + payload。实际实现可改用环形缓冲/IOVec
    std::vector<uint8_t> rbuf(kHeaderSize + 65536);

    while (m_running) {
        int n = ::epoll_wait(m_epfd, events, kMaxEvents, 200);
        if (n <= 0) continue;

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd != m_sock_fd) continue;
            if (events[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                // Bus 断开 — TODO: 重连机制（Phase 3 实现）
                m_running = false;
                break;
            }
            if (!(events[i].events & EPOLLIN)) continue;

            while (true) {
                // 1. 读 Header
                int rh = recvFull(rbuf.data(), kHeaderSize);
                if (rh <= 0) break;
                auto* h = reinterpret_cast<Header*>(rbuf.data());
                if (h->magic != kMagic) {
                    // 丢包，重新同步（简单实现直接break）
                    break;
                }
                // 2. 读 Payload
                std::vector<uint8_t> payload(h->length);
                if (h->length > 0) {
                    int rp = recvFull(payload.data(), h->length);
                    if (rp <= 0) break;
                }

                // 3. 分发
                if (h->msg_type == MsgType::RESPONSE) {
                    ResponseCallback cb;
                    {
                        std::lock_guard<std::mutex> lk(m_pending_mtx);
                        auto it = m_pending.find(h->client_id);
                        if (it != m_pending.end()) {
                            cb = std::move(it->second);
                            m_pending.erase(it);
                        }
                    }
                    if (cb) cb(h->error_code, payload.data(), (uint32_t)payload.size());
                } else {
                    IncomingCallback cb;
                    {
                        std::lock_guard<std::mutex> lk(m_inc_mtx);
                        cb = m_inc_cb;
                    }
                    if (cb) cb(h->service_id, h->method_id, h->client_id,
                               payload.data(), (uint32_t)payload.size());
                }
            }
        }
    }
}

uint32_t NodeClient::nextClientId()
{
    uint32_t x = m_client_seq.fetch_add(1, std::memory_order_relaxed);
    return x != 0 ? x : m_client_seq.fetch_add(1, std::memory_order_relaxed);
}

} // namespace vipc
