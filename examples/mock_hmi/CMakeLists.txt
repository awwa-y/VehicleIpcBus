# CMake 子工程：mock_hmi Qt 例子（需 find_package(Qt6)）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_executable(mock_hmi
    main.cpp
    MainWindow.h   MainWindow.cpp
    IpcBridge.h    IpcBridge.cpp
)
target_link_libraries(mock_hmi PRIVATE vipc Qt6::Core Qt6::Widgets)
target_compile_features(mock_hmi PRIVATE cxx_std_17)
