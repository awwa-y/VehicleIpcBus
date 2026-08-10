# 核心静态库 libvipc.a — 纯 C++17，不依赖 Qt
TEMPLATE = lib
CONFIG  += staticlib c++17
TARGET   = vipc

INCLUDEPATH += $$PWD/../include

HEADERS += \
    $$PWD/../include/vipc/UdsHeader.h \
    $$PWD/../include/vipc/ServiceIds.h \
    $$PWD/../include/vipc/NodeClient.h \
    $$PWD/../include/vipc/MessageBus.h

SOURCES += \
    $$PWD/NodeClient.cpp \
    $$PWD/MessageBus.cpp

unix: LIBS += -lpthread
