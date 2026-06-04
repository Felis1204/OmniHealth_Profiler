# ============================================================
# backend/CMakeLists.txt
# 将后端编译为静态库 (health_backend)
#
# TODO: 后续将通过 FetchContent 引入外部依赖：
#   - SQLite3       (本地健康数据持久化存储)
#   - cpp-httplib   (连接大模型 API，实现生成式健康报告)
#   - nlohmann/json (JSON 解析，用于 API 请求/响应)
#
#   FetchContent 使用示例：
#     include(FetchContent)
#     FetchContent_Declare(sqlite3 ...)
#     FetchContent_MakeAvailable(sqlite3)
# ============================================================

# 搜集源文件
file(GLOB BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 编译为静态库
add_library(health_backend STATIC ${BACKEND_SOURCES})

# 指定静态库的头文件搜索路径
target_include_directories(health_backend
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 要求 C++17
target_compile_features(health_backend PUBLIC cxx_std_17)