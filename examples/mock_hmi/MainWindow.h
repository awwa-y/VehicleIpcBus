#pragma once
// ============================================================================
// 最小HMI主窗口：3个按钮 + 2个标签
//   [连接车辆] [断开] [远光灯开]
//   状态: QLabel
//   日志: QLabel (滚动)
// 纯UI代码，绝不直接操作任何 SocketCAN / Sqlite / 业务解析
// ============================================================================
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <memory>

namespace vipc { class NodeClient; }
class IpcBridge;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& bus_sock, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onLightClicked();
    void onIncoming(uint32_t svc, uint32_t m, uint32_t cid, const QByteArray& p);
    void onVehicleState(const QByteArray& b);
    void onFault(const QByteArray& b);
    void onDegraded(uint32_t svc, const QString& reason);

private:
    void buildUi();
    void appendLog(const QString& s);
    void sendLightCmd(uint8_t bitmap);

    std::unique_ptr<vipc::NodeClient> m_node;
    IpcBridge*  m_bridge = nullptr;   // owned by Qt parent

    QPushButton* m_btnConnect = nullptr;
    QPushButton* m_btnDisconn = nullptr;
    QPushButton* m_btnLight   = nullptr;
    QLabel*      m_labStatus  = nullptr;
    QLabel*      m_labLog     = nullptr;
    QStringList  m_logLines;

    uint8_t      m_lightBitmap = 0;   // bit0远光 bit1近光 bit2工作灯 bit7双闪
    bool         m_connected   = false;
};
