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
endif()

# 将 backend/include 加入全局头文件搜索路径，
# 使 frontend 也能通过 #include "HealthManager.h" 找到契约头文件
include_directories(${CMAKE_SOURCE_DIR}/backend/include)

# ---- 子目录 ----
add_subdirectory(backend)
add_subdirectory(frontend)
