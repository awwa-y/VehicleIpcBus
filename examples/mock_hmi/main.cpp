// ============================================================================
// MockHMI 可执行入口
// 用法：./mock_hmi [bus_sock_path]
// ============================================================================
#include "MainWindow.h"
#include <QApplication>
#include <QString>

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    QString sock = "/tmp/cabpad_bus.sock";
    if (argc > 1) sock = QString::fromLocal8Bit(argv[1]);

    MainWindow w(sock);
    w.show();
    return a.exec();
}
