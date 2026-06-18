# ============================================================
# frontend/CMakeLists.txt
# 将前端编译为可执行文件 (Health_Manager_App)
# 链接后端静态库 health_backend + Qt6::Widgets
# ============================================================

# 查找 Qt6 组件
# Windows 用户: 如果 Qt6 未自动发现，请指定 CMAKE_PREFIX_PATH
#   例如: cmake -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_1310_64
# macOS 用户 (Homebrew): CMake 会自动发现 /opt/homebrew 下的 Qt6
if(WIN32)
    message(STATUS "Windows: if Qt6 is not found, pass "
                    "-DCMAKE_PREFIX_PATH=<Qt-install-path> to CMake "
                    "(e.g. C:/Qt/6.11.1/mingw_1310_64)")
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 启用 Qt 的自动 MOC / UIC 处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# 告诉 AUTOUIC 到 Forms/ 目录找 .ui 文件（因为源文件在 src/）
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/Forms)

# 显式列出源文件
set(FRONTEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widget.cpp
)

# 显式列出头文件
set(FRONTEND_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widget.h
)

# 显式列出 UI 设计文件
set(FRONTEND_FORMS
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/widget.ui
)

# 编译为可执行文件
add_executable(Health_Manager_App
    ${FRONTEND_SOURCES}
    ${FRONTEND_HEADERS}
    ${FRONTEND_FORMS}
)

# 添加头文件搜索路径
target_include_directories(Health_Manager_App PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接后端静态库和 Qt6 Widgets 模块
target_link_libraries(Health_Manager_App PRIVATE
    health_backend
    Qt6::Widgets
)

# 要求 C++17
target_compile_features(Health_Manager_App PRIVATE cxx_std_17)