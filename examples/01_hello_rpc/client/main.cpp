// ============================================================================
// Demo Client：模拟 HMI_UI 发起 REQUEST
// 1. 连 Bus
// 2. sendRequest(SVC_CAN_GATEWAY, SEND_FRAME, can_id=0x17FE0001)
// 3. 等待 Response 或 NOTIFY（上面Server演示用NOTIFY回的ACK）
// ============================================================================
#include "vipc/NodeClient.h"
#include "vipc/ServiceIds.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <unistd.h>

using namespace vipc;
static std::atomic<bool> g_done{false};

int main(int argc, char** argv)
{
    std::string bus = "/tmp/cabpad_bus.sock";
    if (argc > 1) bus = argv[1];

    NodeClient node(bus);
    // 同时监听 Response 和 NOTIFY（Server演示用NOTIFY回ACK）
    node.setIncomingHandler([](uint32_t, uint32_t, uint32_t,
                               const uint8_t* p, uint32_t l) {
        std::string s(reinterpret_cast<const char*>(p), l);
        std::printf("[Client] NOTIFY/response-like payload: %s\n", s.c_str());
        g_done = true;
    });

    if (!node.start()) {
        std::fprintf(stderr, "Client: connect Bus failed\n");
        return 1;
    }
    // 等 Bus 路由生效（你实现中可做重试）
    ::usleep(200 * 1000);

    // payload = can_id(4B BE) + "DATA"
    uint8_t payload[8] = {
        0x17, 0xFE, 0x00, 0x01,   // 0x17FE0001
        'D','A','T','A'
    };
    std::printf("[Client] send REQUEST: SEND_FRAME 0x17FE0001 ...\n");
    node.sendRequest(SVC_CAN_GATEWAY, CAN_GW::SEND_FRAME, payload, 8,
        [](ErrorCode ec, const uint8_t* p, uint32_t l) {
            std::string s(reinterpret_cast<const char*>(p), l);
            std::printf("[Client] RESPONSE err=%d payload='%s'\n",
                        (int)ec, s.c_str());
            g_done = true;
        });

    // 5s 超时
    int wait = 0;
    while (!g_done && wait < 50) { ::usleep(100 * 1000); ++wait; }
    if (!g_done) std::printf("[Client] timeout\n");
    return 0;
}
