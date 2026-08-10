#pragma once
// ============================================================================
// UdsHeader.h - 类SOME/IP风格二进制协议头（25字节固定）
// 适用：VehicleIpcBus (UDS IPC) + SomeIpGateway (TCP车云) 通用
// ============================================================================
#include <cstdint>

namespace vipc {

static constexpr uint32_t kMagic       = 0xABCDEF01;
static constexpr uint32_t kHeaderSize  = 25;

// MsgType：类SOME/IP TP定义（高4位预留）
enum class MsgType : uint8_t {
    REQUEST     = 0x00,   // 客户端→服务端，期待RESPONSE
    RESPONSE    = 0x02,   // 服务端→客户端，对应REQUEST
    NOTIFY      = 0x04,   // 单向广播/订阅推送，无需响应
    REGISTER    = 0x08,   // Provider → MessageBus 注册服务
    HEARTBEAT   = 0x10,   // 任意节点 → Bus：我还活着
};

// ErrorCode：RESPONSE 头里的结果码
enum class ErrorCode : uint8_t {
    OK               = 0x00,
    NOT_READY        = 0x01,   // 节点启动中
    NOT_FOUND        = 0x02,   // ServiceID / MethodID 未注册
    TIMEOUT          = 0x03,   // 等待Provider响应超时
    INVALID_PAYLOAD  = 0x04,
    SERVICE_UNAVAILABLE = 0x05 // 心跳超时被Bus剔除
};

struct __attribute__((packed)) Header {
    uint32_t  magic;       // 固定 kMagic，收端校验
    uint32_t  service_id;  // 服务ID（见 ServiceIds.h）
    uint32_t  method_id;   // 方法ID
    uint32_t  length;      // payload 字节数（不含此Header）
    uint32_t  client_id;   // 请求流水号：Request→Response 匹配用
    MsgType   msg_type;    // MsgType
    ErrorCode error_code;  // 成功=OK
};
static_assert(sizeof(Header) == kHeaderSize,
              "Header size mismatch: expected 25 bytes");

} // namespace vipc
