# MessageBus 可执行入口
TEMPLATE = app
CONFIG  += c++11 console
TARGET   = vipc_messagebus

INCLUDEPATH += $$PWD/../../include
LIBS       += -L$$OUT_PWD/../../src -lvipc
PRE_TARGETDEPS += $$OUT_PWD/../../src/libvipc.a

SOURCES  += $$PWD/main.cpp
