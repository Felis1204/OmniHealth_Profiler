cmake_minimum_required(VERSION 3.20)

# ============================================================
# 项目: Health_Manager — 个人健康数字孪生系统
# 架构: UI (frontend) 与 逻辑 (backend) 分离
# ============================================================
project(Health_Manager
    VERSION 0.1.0
    DESCRIPTION "个人健康数字孪生系统"
    LANGUAGES CXX
)

# ---- C++ 标准 ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- IntelliSense 支持 ----
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 编译器警告 ----
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    # /W4        — 高等级警告（等效 -Wall -Wextra）
    # /permissive- — 标准符合性模式
    add_compile_options(/W4 /permissive-)
endif()

# backend 的 PUBLIC include 目录已通过 target_link_libraries 传播到前端，
# 不再需要全局 include_directories

# ---- 子目录 ----
add_subdirectory(backend)
add_subdirectory(frontend)

# ---- 安装规则 ----
if(APPLE)
    install(TARGETS Health_Manager_App BUNDLE DESTINATION . COMPONENT Runtime)
else()
    install(TARGETS Health_Manager_App RUNTIME DESTINATION . COMPONENT Runtime)
endif()

# ---- 部署目标 (deploy) ----
# 将 Qt 运行时依赖自动复制到可执行文件旁边/内部
if(APPLE)
    get_target_property(QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)
    find_program(MACDEPLOYQT macdeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
    add_custom_target(deploy
        COMMAND ${MACDEPLOYQT} "$<TARGET_BUNDLE_DIR:Health_Manager_App>"
            -verbose=1 -always-overwrite -codesign=-
        COMMENT "Running macdeployqt to bundle Qt frameworks..."
        DEPENDS Health_Manager_App
    )
elseif(WIN32)
    get_target_property(QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
    # 获取 SQLite3 DLL 路径（如果存在）
    if(TARGET SQLite3::SQLite3)
        set(SQLITE3_DLL "$<TARGET_FILE:SQLite3::SQLite3>")
    else()
        # 对 IMPORTED 目标，用 SQLite3_LIBRARY 变量
        set(SQLITE3_DLL "${SQLite3_LIBRARY}")
    endif()

    add_custom_target(deploy
        COMMAND ${WINDEPLOYQT} "$<TARGET_FILE:Health_Manager_App>"
            --verbose 2 --no-translations
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SQLITE3_DLL}"
            "$<TARGET_FILE_DIR:Health_Manager_App>/"
        COMMENT "Running windeployqt to copy Qt DLLs, then copying SQLite3..."
        DEPENDS Health_Manager_App
    )
endif()

# ---- 测试（默认关闭）----
option(BUILD_TESTING "Build the test suite" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
