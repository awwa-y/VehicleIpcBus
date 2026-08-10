#include "MainWindow.h"
#include "IpcBridge.h"
#include "vipc/NodeClient.h"
#include "vipc/ServiceIds.h"

#include <QByteArray>
#include <QDateTime>
#include <memory>

MainWindow::MainWindow(const QString& bus_sock, QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();

    // 1. Bridge：接收方（Qt主线程）
    m_bridge = new IpcBridge(this);
    connect(m_bridge, &IpcBridge::incomingMessage,   this, &MainWindow::onIncoming);
    connect(m_bridge, &IpcBridge::vehicleStateChanged, this, &MainWindow::onVehicleState);
    connect(m_bridge, &IpcBridge::faultChanged,       this, &MainWindow::onFault);
    connect(m_bridge, &IpcBridge::serviceDegraded,    this, &MainWindow::onDegraded);

    // 2. NodeClient：发送方 + epoll线程
    m_node = std::make_unique<vipc::NodeClient>(bus_sock.toStdString());
    m_node->setIncomingHandler([this](uint32_t s, uint32_t m, uint32_t c,
                                      const uint8_t* d, uint32_t l) {
        // ✅ 这里运行在 epoll 线程 —— 绝对不能碰 QWidget！
        m_bridge->postToUi(s, m, c, d, l);
    });

    if (m_node->start()) {
        appendLog("连接MessageBus OK：" + bus_sock);
        m_labStatus->setText("就绪（未连接车辆）");
    } else {
        appendLog("⚠ 连接MessageBus失败，请先启动 vipc_messagebus");
        m_labStatus->setText("总线离线");
    }
}

MainWindow::~MainWindow()
{
    if (m_node) m_node->stop();
}

void MainWindow::buildUi()
{
    setWindowTitle("MockHMI — VehicleIpcBus Demo");
    resize(600, 300);

    auto* central = new QWidget(this);
    auto* root    = new QVBoxLayout(central);

    auto* row = new QHBoxLayout();
    m_btnConnect = new QPushButton("连接车辆 0", this);
    m_btnDisconn = new QPushButton("断开", this);
    m_btnLight   = new QPushButton("远光灯 OFF", this);
    m_btnConnect->setMinimumHeight(48);
    m_btnDisconn->setMinimumHeight(48);
    m_btnLight->setMinimumHeight(48);
    row->addWidget(m_btnConnect);
    row->addWidget(m_btnDisconn);
    row->addWidget(m_btnLight);

    m_labStatus = new QLabel("初始化中…", this);
    m_labStatus->setStyleSheet("font-size:18px; color:#2d8cf0; font-weight:bold; padding:8px;");
    m_labLog    = new QLabel("日志:\n", this);
    m_labLog->setWordWrap(true);
    m_labLog->setStyleSheet("background:#2b2b2b;color:#a9b7c6; padding:8px;");

    root->addLayout(row);
    root->addWidget(m_labStatus);
    root->addWidget(m_labLog, 1);
    setCentralWidget(central);

    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_btnLight,   &QPushButton::clicked, this, &MainWindow::onLightClicked);
}

void MainWindow::appendLog(const QString& s)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_logLines.push_front(QString("[%1] %2").arg(ts, s));
    while (m_logLines.size() > 8) m_logLines.pop_back();
    m_labLog->setText("日志:\n" + m_logLines.join("\n"));
}

// ---- 按钮回调（全部走RPC，不直接写硬件）------
void MainWindow::onConnectClicked()
{
    using namespace vipc;
    uint8_t payload[1] = { 0 };  // car_index = 0
    appendLog("→ CMD_CONNECT_CAR(idx=0)");
    m_node->sendRequest(SVC_STATE_MGR, STATE::CMD_CONNECT_CAR, payload, 1,
        [this](ErrorCode ec, const uint8_t*, uint32_t) {
            // 注意：此回调在 epoll 线程！不能直接 setText。
            // 这里简单做法：投递 QMetaObject::invokeMethod 或 postEvent。
            QMetaObject::invokeMethod(m_labStatus, [this, ec] {
                if (ec == ErrorCode::OK) {
                    m_connected = true;
                    m_labStatus->setText("✅ 已连接车辆0");
                } else {
                    m_labStatus->setText("❌ 连接失败 (err=" +
                                         QString::number((int)ec) + ")");
                }
            }, Qt::QueuedConnection);
        });
}

void MainWindow::onDisconnectClicked()
{
    using namespace vipc;
    uint8_t payload[1] = { 0 };
    appendLog("→ CMD_DISCONNECT_CAR(idx=0)");
    m_node->sendRequest(SVC_STATE_MGR, STATE::CMD_DISCONNECT_CAR, payload, 1);
    m_connected = false;
    m_labStatus->setText("未连接车辆");
}

void MainWindow::onLightClicked()
{
    m_lightBitmap ^= 0x01;  // bit0 远光灯 toggle
    m_btnLight->setText(m_lightBitmap & 1 ? "远光灯 ON" : "远光灯 OFF");
    sendLightCmd(m_lightBitmap);
}

void MainWindow::sendLightCmd(uint8_t bitmap)
{
    using namespace vipc;
    uint8_t payload[1] = { bitmap };
    appendLog(QString("→ CMD_SET_LIGHT(bitmap=0x%1)").arg(bitmap, 2, 16, QChar('0')));
    m_node->sendRequest(SVC_STATE_MGR, STATE::CMD_SET_LIGHT, payload, 1);
}

// ---- UI 更新回调（全部是Qt主线程安全）------
void MainWindow::onIncoming(uint32_t svc, uint32_t m, uint32_t, const QByteArray&)
{
    appendLog(QString("← INCOMING svc=0x%1 m=0x%2")
                  .arg(svc, 8, 16, QChar('0'))
                  .arg(m,   8, 16, QChar('0')));
}

void MainWindow::onVehicleState(const QByteArray& b)
{
    appendLog(QString("状态快照 %1 字节").arg(b.size()));
}

void MainWindow::onFault(const QByteArray&)
{
    m_labStatus->setText("⚠ 故障变更，详情见诊断页");
}

void MainWindow::onDegraded(uint32_t svc, const QString& reason)
{
    appendLog(QString("⚠ 服务降级 svc=0x%1 reason=%2")
                  .arg(svc, 8, 16, QChar('0')).arg(reason));
    m_labStatus->setStyleSheet("font-size:18px;color:#ed4014;font-weight:bold;padding:8px;");
}
