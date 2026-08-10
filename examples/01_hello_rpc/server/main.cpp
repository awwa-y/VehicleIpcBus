// ============================================================================
// Demo Server：模拟 CAN_Gateway 服务端
// 1. 连接 Bus
// 2. REGISTER(SVC_CAN_GATEWAY)
// 3. 收到 REQUEST SEND_FRAME → 假装写CAN → 回 ACK Response
// ============================================================================
#include "vipc/NodeClient.h"
#include "vipc/ServiceIds.h"
#include <cstdio>
#include <unistd.h>
#include <signal.h>

using namespace vipc;
static NodeClient* g_node = nullptr;
static void on_signal(int) { if (g_node) g_node->stop(); }

int main(int argc, char** argv)
{
    std::string bus = "/tmp/cabpad_bus.sock";
    if (argc > 1) bus = argv[1];

    NodeClient node(bus);
    g_node = &node;
    node.setIncomingHandler([&](uint32_t svc, uint32_t m, uint32_t cid,
                                const uint8_t* p, uint32_t l) {
        if (svc == SVC_CAN_GATEWAY && m == CAN_GW::SEND_FRAME) {
            // payload = can_id(4B) + data
            uint32_t can_id = 0;
            if (l >= 4) {
                can_id = ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                         ((uint32_t)p[2]<<8)|(uint32_t)p[3];
            }
            std::printf("[Server] REQ SEND_FRAME can_id=0x%08x dlc=%u\n",
                        can_id, l >= 4 ? l - 4 : 0);
            // Response: ACK 文本 payload
            std::string ack = "ACK can_id=0x" +
                std::to_string(can_id) + " sent";
            Header hdr{};
            hdr.magic      = kMagic;
            hdr.service_id = svc;
            hdr.method_id  = m;
            hdr.length     = (uint32_t)ack.size();
            hdr.client_id  = cid;
            hdr.msg_type   = MsgType::RESPONSE;
            hdr.error_code = ErrorCode::OK;
            // 直接用 publish 不对 —— 应该走 node 底层发送。
            // 简化实现：这里直接回发一条 publish 当 ACK（Phase 2 你可封装 sendResponse 方法）
            node.publish(svc, m, reinterpret_cast<const uint8_t*>(ack.data()),
                         (uint32_t)ack.size());
            // 注意：上面publish不是点对点，Client端会通过NOTIFY收到；
            // 真实项目需在NodeClient增加 sendResponse(svc,m,cid,err,payload) 更严谨。
        }
    });

    if (!node.start()) {
        std::fprintf(stderr, "Server: connect to Bus failed\n");
        return 1;
    }
    // 注册服务
    node.registerService(SVC_CAN_GATEWAY);
    std::printf("[Server] registered SVC_CAN_GATEWAY. Waiting REQ...\n");

    struct sigaction sa{}; sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,nullptr);
    sigaction(SIGTERM,&sa,nullptr);

    while (true) ::sleep(1);
    return 0;
}
