# ============================================================
# frontend/CMakeLists.txt
# 将前端编译为可执行文件 (Health_Manager_App)
# 链接后端静态库 health_backend
#
# TODO: 后续将接入 GUI 库，可选择：
#   - ImGui       (轻量级，适合快速原型)
#   - Qt6         (功能全面，适合正式桌面应用)
#
#   GUI 接入示例 (以 ImGui 为例)：
#     FetchContent_Declare(imgui ...)
#     target_link_libraries(Health_Manager_App PRIVATE imgui)
#     target_link_libraries(Health_Manager_App PRIVATE
#         ${OPENGL_LIBRARIES} glfw)
# ============================================================

# 搜集前端源文件
file(GLOB FRONTEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 编译为可执行文件
add_executable(Health_Manager_App ${FRONTEND_SOURCES})

# 链接后端静态库
target_link_libraries(Health_Manager_App PRIVATE health_backend)

# 要求 C++17
target_compile_features(Health_Manager_App PRIVATE cxx_std_17)