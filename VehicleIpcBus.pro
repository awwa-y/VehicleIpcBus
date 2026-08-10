# ============================================================================
# VehicleIpcBus - qmake 构建（主，跨Win/Linux稳）
# 核心库：libvipc.a (纯C++17，无Qt依赖)
# 子工程：examples/* 分别用自己的 .pro
# ============================================================================
TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS = \
    src \
    examples/01_hello_rpc/client \
    examples/01_hello_rpc/server \
    examples/messagebus \
    examples/mock_hmi
