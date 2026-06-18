# OmniHealth Profiler — 构建指南

## 前置依赖

| 依赖 | 用途 | macOS | Windows |
|------|------|-------|---------|
| CMake ≥ 3.20 | 构建系统 | `brew install cmake` | 从 [cmake.org](https://cmake.org) 下载安装 |
| C++17 编译器 | 编译 | Apple Clang（Xcode 自带） | MinGW-w64 / MSVC 2022 |
| Qt ≥ 6.0 | GUI 前端 | `brew install qt` | 从 [qt.io](https://www.qt.io) 安装（推荐在线安装器） |
| SQLite3 | 数据持久化 | Homebrew 自动处理 | vcpkg / MSYS2 安装 |
| Git | 依赖下载 | 系统自带 | [git-scm.com](https://git-scm.com) |

> nlohmann/json 通过 CMake FetchContent 自动下载，无需手动安装。

---

## macOS 构建

### 1. 安装依赖

```bash
# 安装 CMake 和 Qt6（SQLite3 作为 Qt 依赖自动安装）
brew install cmake qt
```

### 2. 配置

```bash
cd OmniHealth_Profiler
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

CMake 会自动发现 `/opt/homebrew` 下的 Qt6 和 SQLite3，无需手动指定路径。

### 3. 编译

```bash
cmake --build build
```

编译产物：
- 后端静态库：`build/backend/libhealth_backend.a`
- 前端可执行文件：`build/frontend/Health_Manager_App`

### 4. 运行

```bash
./build/frontend/Health_Manager_App
```

### 5. （可选）编译并运行测试

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build --target test_ascvd
./build/tests/test_ascvd
```

---

## Windows 构建

### 1. 安装依赖

**Qt6（通过在线安装器）**
1. 访问 https://www.qt.io/download-open-source
2. 下载 Qt Online Installer
3. 安装时勾选 **Qt 6.x.x → MinGW 64-bit**（队友当前使用 6.11.1 MinGW）
4. 记录安装路径，例如 `C:\Qt\6.11.1\mingw_1310_64`

**SQLite3（任选一种方式）**

方式 A — MSYS2（推荐，配合 MinGW 使用）：
```bash
pacman -S mingw-w64-x86_64-sqlite3
```

方式 B — vcpkg：
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat
vcpkg install sqlite3:x64-windows
```

### 2. 配置

在项目根目录打开终端（PowerShell / Git Bash / MSYS2 终端），运行：

```bash
cmake -B build -G "MinGW Makefiles" ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_1310_64 ^
      -DCMAKE_BUILD_TYPE=Debug
```

> **注意**：`CMAKE_PREFIX_PATH` 必须指向你的 Qt6 实际安装路径（含 mingw 子目录）。
> 如果使用 **MSVC**，将 `-G` 改为 `"Visual Studio 17 2022"`，并指向 msvc 版本的 Qt 路径。

如果 CMake 找不到 SQLite3，额外指定：
```bash
-DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_1310_64;C:/path/to/sqlite3"
```

### 3. 编译

```bash
cmake --build build
```

### 4. 运行

```bash
build\frontend\Health_Manager_App.exe
```

> 如果提示缺少 Qt DLL，将 `C:\Qt\6.11.1\mingw_1310_64\bin` 加入 PATH 环境变量，或使用 Qt Creator 直接运行。

---

## 编译器差异说明

项目使用集中式平台适配层 `backend/include/PlatformCompat.h` 处理编译器差异：

| 函数 | POSIX (macOS / Linux / MinGW) | MSVC |
|------|------|------|
| UTC time 转换 | `gmtime_r` / `timegm` | `gmtime_s` / `_mkgmtime` |

确保 `.cpp` 文件包含 `#include "PlatformCompat.h"` 而非直接使用平台特有函数。

---

## 常见问题

### cmake 找不到 Qt6

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

**解决**：指定 `-DCMAKE_PREFIX_PATH=<Qt安装路径>`，确保路径指向包含 `lib/cmake/Qt6` 的目录。

### cmake 找不到 SQLite3

```
CMake Error: Could NOT find SQLite3
```

**解决**（Windows）：通过 MSYS2 或 vcpkg 安装 SQLite3。或手动指定：
```bash
-DSQLite3_ROOT=C:/path/to/sqlite3
```

### ld: library not found for -lsqlite3

**解决**（macOS Intel）：项目已支持 `/usr/local/opt/sqlite` 路径。如仍有问题，运行：
```bash
brew install sqlite3
```

### 编译时出现 `gmtime_r` 未定义

**解决**：检查是否引入了新的 `.cpp` 文件但忘记 `#include "PlatformCompat.h"`。所有需要时间函数的文件应使用 `health::platform::gmtimeCompat()` 和 `health::platform::timegmCompat()`。

---

## 目录结构速览

```
OmniHealth_Profiler/
├── CMakeLists.txt              # 根构建文件
├── backend/
│   ├── CMakeLists.txt          # 后端静态库
│   └── include/
│       └── PlatformCompat.h    # 跨平台适配层
├── frontend/
│   ├── CMakeLists.txt          # 前端可执行文件
│   ├── Forms/widget.ui         # Qt Designer UI
│   └── src/widget.cpp          # Qt Widget 实现
├── tests/
│   └── test_ascvd.cpp          # ASCVD 端到端测试
└── docs/
    ├── PRD.md                  # 产品需求文档
    ├── SYSTEM_DESIGN.md        # 系统设计文档
    └── BUILD.md                # 本文档
```
