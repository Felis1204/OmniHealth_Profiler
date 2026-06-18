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

# ---- 测试（默认关闭）----
option(BUILD_TESTING "Build the test suite" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
