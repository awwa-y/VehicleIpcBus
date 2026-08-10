#pragma once
// ============================================================================
// ServiceIds.h - ServiceID / MethodID 清单
// 注：此文件为通用服务ID，不绑定具体业务。实际项目可扩展。
// ============================================================================
#include <cstdint>

namespace vipc {

// --- ServiceID（0x0000 预留）------------------------------------------------
enum ServiceID : uint32_t {
    SVC_MESSAGEBUS      = 0x00000000,   // 总线自身
    SVC_CAN_GATEWAY     = 0x00000001,   // CAN硬件接入（车端）
    SVC_STATE_MGR       = 0x00000002,   // 车辆状态机 + 故障诊断
    SVC_DATA_LOGGER     = 0x00000003,   // 异步数据记录
    SVC_HMI_UI          = 0x00000004,   // UI反向控制（预留）
    SVC_CUSTOM_BASE     = 0x00010000,   // 用户自定义服务起始
};

// --- SVC_CAN_GATEWAY MethodID ----------------------------------------------
namespace CAN_GW {
    enum Method : uint32_t {
        SEND_FRAME           = 1,  // REQ: can_id(4B)+data(≤8B)  RSP: ok
        NOTIFY_RECV_FRAME    = 2,  // PUB: can_id(4B)+data(8B)
        SEND_DISCONNECT      = 3,  // REQ: (下大闸/断开车辆)
        SEND_MAP_BUILD       = 4,  // REQ: 建图触发
    };
}

// --- SVC_STATE_MGR MethodID ------------------------------------------------
namespace STATE {
    enum Method : uint32_t {
        CMD_CONNECT_CAR      = 1,  // REQ: car_index(1B)
        CMD_DISCONNECT_CAR   = 2,  // REQ: car_index(1B)
        CMD_SET_MODE         = 3,  // REQ: mode(1B) 0本地1遥操2自动
        CMD_SET_LIGHT        = 4,  // REQ: bitmap(1B) bit0远光 bit1近光 bit2工作灯 bit7双闪
        CMD_PARAM_APPLY      = 5,  // REQ: 车速/安全距离/铲掘/卸料 参数
        GET_VEHICLE_STATUS   = 6,  // REQ: (空) RSP: VehicleStatus二进制
        GET_FAULT_STATUS     = 7,  // REQ: (空) RSP: FaultStatus二进制
        NOTIFY_STATE_CHANGE  = 8,  // PUB: (状态快照)
        NOTIFY_FAULT_CHANGE  = 9,  // PUB: (故障快照)
    };
}

// --- SVC_DATA_LOGGER MethodID ----------------------------------------------
namespace LOGGER {
    enum Method : uint32_t {
        SUBSCRIBE_ALL        = 1,  // REQ: (空) 订阅所有PUB消息
        QUERY_RANGE          = 2,  // REQ: from_ms(8B)+to_ms(8B) RSP: 压缩包
    };
}

} // namespace vipc
