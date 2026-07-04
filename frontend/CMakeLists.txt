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
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

# 启用 Qt 的自动 MOC / UIC 处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# 告诉 AUTOUIC 到 Forms/ 目录找 .ui 文件（因为源文件在 src/）
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/Forms)

# 显式列出源文件
set(FRONTEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AddData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bpredialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datamanage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/healthdashboarddialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/labtestredialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mhredialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reportdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/trenddialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usermanage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vitalredialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AISettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AIReportDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SeedData.cpp
)

# 显式列出头文件（包含 Q_OBJECT 的头文件必须在此，AUTOMOC 才能处理）
set(FRONTEND_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/AddData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bpredialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/datamanage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/healthdashboarddialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/labtestredialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mhredialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/reportdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/trenddialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/usermanage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vitalredialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/AISettingsDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/AIReportDialog.h
)

# 显式列出 UI 设计文件
set(FRONTEND_FORMS
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/widget.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/AddData.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/bpredialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/datamanage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/healthdashboarddialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/labtestredialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/mhredialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/reportdialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/trenddialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/usermanage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/Forms/vitalredialog.ui
)

# 编译为可执行文件 (macOS Bundle / Windows exe)
if(APPLE)
    add_executable(Health_Manager_App MACOSX_BUNDLE
        ${FRONTEND_SOURCES}
        ${FRONTEND_HEADERS}
        ${FRONTEND_FORMS}
    )
    set_target_properties(Health_Manager_App PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.omnihealth.profiler"
        MACOSX_BUNDLE_BUNDLE_NAME "OmniHealth Profiler"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
else()
    add_executable(Health_Manager_App
        ${FRONTEND_SOURCES}
        ${FRONTEND_HEADERS}
        ${FRONTEND_FORMS}
    )
endif()

# 添加头文件搜索路径
target_include_directories(Health_Manager_App PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接后端静态库和 Qt6 Widgets 模块
target_link_libraries(Health_Manager_App PRIVATE
    health_backend
    Qt6::Widgets
    Qt6::Charts
)

# 要求 C++17
target_compile_features(Health_Manager_App PRIVATE cxx_std_17)