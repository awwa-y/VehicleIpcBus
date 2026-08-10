# 01_hello_rpc server 演示：注册 SVC_CAN_GATEWAY 服务，收到 SEND_FRAME 回 ACK
TEMPLATE = app
CONFIG  += c++11 console
TARGET   = vipc_demo_server

INCLUDEPATH += $$PWD/../../../include
LIBS       += -L$$OUT_PWD/../../../src -lvipc
PRE_TARGETDEPS += $$OUT_PWD/../../../src/libvipc.a

SOURCES  += $$PWD/main.cpp
