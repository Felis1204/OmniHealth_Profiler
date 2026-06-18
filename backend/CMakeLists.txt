# ============================================================
# backend/CMakeLists.txt
# 将后端编译为静态库 (health_backend)
#
# 外部依赖：
#   - SQLite3       (本地健康数据持久化存储) —— 跨平台自动查找
#   - cpp-httplib   (连接大模型 API，实现生成式健康报告) —— TODO
#   - nlohmann/json (JSON 解析，用于 API 请求/响应) —— 已集成
# ============================================================

# ---- 外部依赖 ----
# nlohmann/json (JSON 解析，用于 DataAccess 数据序列化与 API 交互)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# SQLite3 —— 跨平台查找策略
# macOS Homebrew (Apple Silicon: /opt/homebrew, Intel: /usr/local)
if(APPLE)
    foreach(HOMEBREW_PREFIX "/opt/homebrew/opt/sqlite" "/usr/local/opt/sqlite")
        if(EXISTS "${HOMEBREW_PREFIX}")
            list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
        endif()
    endforeach()
elseif(WIN32)
    # vcpkg 用户: CMAKE_TOOLCHAIN_FILE 自动处理 SQLite3 查找，无需额外配置
    # MinGW/MSYS2 用户: pacman -S mingw-w64-x86_64-sqlite3 安装后 CMake 自动发现
    # 其他用户: 安装 SQLite3 后通过 -DCMAKE_PREFIX_PATH=<path> 指定
    if(DEFINED ENV{VCPKG_ROOT})
        message(STATUS "vcpkg detected, SQLite3 will be found via vcpkg toolchain")
    else()
        # 自动探测 Qt 自带的 MinGW 工具链中的 SQLite3（常见于 Qt 在线安装器）
        # 例如: Qt 安装路径下的 Tools/mingw1310_64/opt/ 包含 sqlite3.h 和 libsqlite3.a
        foreach(MINGW_OPT_DIR
            "C:/Qt/Tools/mingw1310_64/opt"
            "C:/Qt/Tools/mingw1120_64/opt"
            "F:/Qt/Tools/mingw1310_64/opt"
            "F:/Qt/Tools/mingw1120_64/opt"
        )
            if(EXISTS "${MINGW_OPT_DIR}/include/sqlite3.h")
                message(STATUS "Found SQLite3 in Qt MinGW tools: ${MINGW_OPT_DIR}")
                list(APPEND CMAKE_PREFIX_PATH "${MINGW_OPT_DIR}")
                break()
            endif()
        endforeach()
        message(STATUS "Windows: if SQLite3 not found, install via vcpkg or MSYS2, or set -DCMAKE_PREFIX_PATH=<path>")
    endif()
endif()
find_package(SQLite3 REQUIRED)

# 搜集源文件（排除测试文件 test_compile.cpp）
file(GLOB BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 排除临时测试文件
list(FILTER BACKEND_SOURCES EXCLUDE REGEX ".*test_compile\\.cpp$")

# 编译为静态库
add_library(health_backend STATIC ${BACKEND_SOURCES})

# 指定静态库的头文件搜索路径
target_include_directories(health_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SQLite3_INCLUDE_DIRS}
)

# 链接 SQLite3
# SQLite3 的 CMake 目标名因包管理器而异：
#   - 标准 FindSQLite3 模块 (CMake 3.14+) → SQLite::SQLite3
#   - vcpkg 的 sqlite3 端口                → unofficial::sqlite3::sqlite3
# 这里采用兼容策略：优先使用已知目标，若都不存在则手动创建
if(TARGET SQLite::SQLite3)
    target_link_libraries(health_backend PUBLIC SQLite::SQLite3 nlohmann_json::nlohmann_json)
elseif(TARGET SQLite3::SQLite3)
    target_link_libraries(health_backend PUBLIC SQLite3::SQLite3 nlohmann_json::nlohmann_json)
else()
    # 手动创建 IMPORTED 目标（降级方案）
    add_library(SQLite::SQLite3 UNKNOWN IMPORTED)
    set_target_properties(SQLite::SQLite3 PROPERTIES
        IMPORTED_LOCATION "${SQLite3_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}"
    )
    target_link_libraries(health_backend PUBLIC SQLite::SQLite3 nlohmann_json::nlohmann_json)
endif()

# 要求 C++17
target_compile_features(health_backend PUBLIC cxx_std_17)