// ============================================================================
// MessageBus 可执行程序
// 用法：./vipc_messagebus [sock_path]
//   默认 /tmp/cabpad_bus.sock
// ============================================================================
#include "vipc/MessageBus.h"
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>

static vipc::MessageBus* g_bus = nullptr;
static void on_signal(int) { if (g_bus) g_bus->stop(); }

int main(int argc, char** argv)
{
    std::string path = "/tmp/cabpad_bus.sock";
    if (argc > 1) path = argv[1];

    vipc::MessageBus bus(path);
    g_bus = &bus;
    if (!bus.start()) {
        std::fprintf(stderr, "MessageBus start failed at %s\n", path.c_str());
        return 1;
    }
    std::printf("[MessageBus] listening on %s (Ctrl+C to quit)\n", path.c_str());

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // 主线程 sleep 循环，epoll在子线程
    while (true) {
        ::sleep(1);
        // TODO: Phase3 heartbeat sweep / 统计打印
    }
    return 0;
}
