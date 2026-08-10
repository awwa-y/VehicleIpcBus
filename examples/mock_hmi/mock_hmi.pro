# mock_hmi — Qt 最小HMI演示（跨线程通信样板）
QT      += core widgets
CONFIG  += c++17
TARGET   = mock_hmi
TEMPLATE = app

INCLUDEPATH += $$PWD/../../include
LIBS       += -L$$OUT_PWD/../../src -lvipc
PRE_TARGETDEPS += $$OUT_PWD/../../src/libvipc.a

unix: LIBS += -lpthread

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/MainWindow.cpp \
    $$PWD/IpcBridge.cpp

HEADERS += \
    $$PWD/MainWindow.h \
    $$PWD/IpcBridge.h
