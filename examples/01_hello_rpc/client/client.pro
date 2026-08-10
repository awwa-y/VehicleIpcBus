# client 演示：发 REQUEST → 等 Response
TEMPLATE = app
CONFIG  += c++11 console
TARGET   = vipc_demo_client

INCLUDEPATH += $$PWD/../../../include
LIBS       += -L$$OUT_PWD/../../../src -lvipc
PRE_TARGETDEPS += $$OUT_PWD/../../../src/libvipc.a

SOURCES  += $$PWD/main.cpp
