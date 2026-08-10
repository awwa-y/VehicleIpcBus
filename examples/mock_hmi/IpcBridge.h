#pragma once
// ============================================================================
// IpcBridge：epoll 线程 → Qt UI 主线程安全桥
//
// 原理：
//   epoll 线程（NodeClient回调）→ postEvent 投递自定义QEvent到主线程事件循环
//   → 主线程 customEvent 分发 → emit signal → MainWindow slot 更新控件
//
// 为什么不用 QueuedConnection 直接信号槽？
//   NodeClient 本身不是 QObject，且 epoll 线程不跑 Qt event dispatcher；
//   postEvent 是 Qt 官方推荐的线程安全跨线程投递方式。
// ============================================================================
#include <QObject>
#include <QEvent>
#include <QCoreApplication>
#include <cstdint>
#include <vector>

class IpcBridge : public QObject {
    Q_OBJECT
public:
    struct PayloadEvent : public QEvent {
        static const Type TYPE = static_cast<Type>(QEvent::User + 0xB1);
        uint32_t service_id;
        uint32_t method_id;
        uint32_t client_id;
        std::vector<uint8_t> payload;
        PayloadEvent() : QEvent(TYPE) {}
    };

    explicit IpcBridge(QObject* parent = nullptr);

    // 【epoll 线程安全调用】投递到主线程
    void postToUi(uint32_t svc, uint32_t m, uint32_t c,
                  const uint8_t* data, uint32_t len);

signals:
    // （主线程）收到 REQUEST/NOTIFY 类消息
    void incomingMessage(uint32_t service_id, uint32_t method_id,
                         uint32_t client_id, const QByteArray& payload);
    // 便捷信号：UI只关心状态更新可接这个
    void vehicleStateChanged(const QByteArray& statusBinary);
    void faultChanged(const QByteArray& faultBinary);
    void serviceDegraded(uint32_t service_id, const QString& reason);

protected:
    void customEvent(QEvent* ev) override;
};
