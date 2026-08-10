#include "IpcBridge.h"
#include "vipc/ServiceIds.h"

IpcBridge::IpcBridge(QObject* parent) : QObject(parent)
{
}

void IpcBridge::postToUi(uint32_t svc, uint32_t m, uint32_t c,
                         const uint8_t* data, uint32_t len)
{
    auto* e = new PayloadEvent;
    e->service_id = svc;
    e->method_id  = m;
    e->client_id  = c;
    if (len && data) e->payload.assign(data, data + len);
    // postEvent 是线程安全的（Qt内部锁保护事件队列）
    QCoreApplication::postEvent(this, e);
}

void IpcBridge::customEvent(QEvent* ev)
{
    if (ev->type() != PayloadEvent::TYPE) return;
    auto* e = static_cast<PayloadEvent*>(ev);

    QByteArray payload(reinterpret_cast<const char*>(e->payload.data()),
                       (int)e->payload.size());

    emit incomingMessage(e->service_id, e->method_id, e->client_id, payload);

    // 便捷分发（简化MainWindow连接数）
    using namespace vipc;
    if (e->service_id == SVC_STATE_MGR) {
        if (e->method_id == STATE::NOTIFY_STATE_CHANGE)
            emit vehicleStateChanged(payload);
        else if (e->method_id == STATE::NOTIFY_FAULT_CHANGE)
            emit faultChanged(payload);
    }
    // TODO: ErrorCode::SERVICE_UNAVAILABLE → serviceDegraded(...)
}
