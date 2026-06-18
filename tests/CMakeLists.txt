# ============================================================
# tests/CMakeLists.txt
# 通过 -DBUILD_TESTING=ON 启用测试构建
# ============================================================

# ASCVD 端到端验证测试
add_executable(test_ascvd test_ascvd.cpp)
target_link_libraries(test_ascvd PRIVATE health_backend)
target_compile_features(test_ascvd PRIVATE cxx_std_17)
add_test(NAME ascvd_verification COMMAND test_ascvd)
