# CMake 子工程：messagebus 可执行
add_executable(vipc_messagebus main.cpp)
target_link_libraries(vipc_messagebus PRIVATE vipc)
