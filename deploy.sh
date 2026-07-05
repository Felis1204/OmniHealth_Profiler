#!/bin/bash
# ============================================================
# OmniHealth Profiler - macOS 打包脚本
# 生成可独立运行的 .app + .dmg
# ============================================================
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "=== 1/4 编译 Release ==="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER
cmake --build "$BUILD_DIR" -j$(sysctl -n hw.logicalcpu)

echo ""
echo "=== 2/4 捆绑 Qt 框架 (macdeployqt) ==="
cmake --build "$BUILD_DIR" --target deploy

APP="$BUILD_DIR/frontend/Health_Manager_App.app"

echo ""
echo "=== 3/4 捆绑 SQLite3 动态库 ==="
# Homebrew 安装的 SQLite3 在 /opt/homebrew/lib 或 /usr/local/lib
for SQLITE_LIB in /opt/homebrew/lib/libsqlite3*.dylib /usr/local/lib/libsqlite3*.dylib; do
    if [ -f "$SQLITE_LIB" ]; then
        cp -f "$SQLITE_LIB" "$APP/Contents/Frameworks/"
        # 修复 install_name 使 .app 能内部查找
        install_name_tool -change "$SQLITE_LIB" \
            "@executable_path/../Frameworks/$(basename "$SQLITE_LIB")" \
            "$APP/Contents/MacOS/Health_Manager_App" 2>/dev/null || true
        echo "  ✅ 已捆绑 $(basename "$SQLITE_LIB")"
        break
    fi
done

echo ""
echo "=== 4/5 验证 .app ==="
if [ -d "$APP" ]; then
    SIZE=$(du -sh "$APP" | cut -f1)
    echo "  ✅ $APP ($SIZE)"
else
    echo "  ❌ .app 未生成"
    exit 1
fi

echo ""
echo "=== 5/5 创建 DMG ==="
VERSION="0.2.0"
DMG_NAME="OmniHealth_Profiler-${VERSION}-macOS.dmg"
DMG_PATH="$BUILD_DIR/$DMG_NAME"

# 清除 macOS 扩展属性（避免签名错误）
xattr -cr "$APP" 2>/dev/null || true

rm -f "$DMG_PATH"
hdiutil create -volname "OmniHealth Profiler" -srcfolder "$APP" -ov -format UDZO "$DMG_PATH"

echo ""
echo "============================================"
echo "  ✅ 打包完成!"
echo "  DMG: $DMG_PATH"
echo "  大小: $(du -sh "$DMG_PATH" | cut -f1)"
echo "============================================"
echo ""
echo "直接双击 $DMG_PATH 即可挂载，把 .app 拖到 Applications 即可运行。"
echo "也可以直接用 open 命令运行："
echo "  open $APP"
