@echo off
REM ============================================================
REM OmniHealth Profiler - Windows 打包脚本
REM 生成可独立运行的 .exe + 所有 DLL (ZIP)
REM
REM 前提条件:
REM   1. 已安装 Qt 6.x (如 C:\Qt\6.11.1\mingw_1310_64)
REM   2. 已安装 CMake ≥ 3.20
REM   3. MinGW 或 MSVC 编译器在 PATH 中
REM
REM 如果使用 MSVC:
REM   先在开始菜单打开 "x64 Native Tools Command Prompt"
REM   然后运行此脚本
REM ============================================================
setlocal enabledelayedexpansion

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

REM ---- 检测 Qt 路径 ----
if not defined Qt6_DIR (
    echo [提示] Qt6_DIR 未设置，尝试自动检测...
    for /d %%i in (C:\Qt\6.*) do (
        for /d %%j in (%%i\mingw_* %%i\msvc*_64) do (
            set Qt6_DIR=%%j\lib\cmake\Qt6
            goto :qt_found
        )
    )
    :qt_found
)
if not defined Qt6_DIR (
    echo [错误] 找不到 Qt6。请设置 Qt6_DIR 环境变量，例如:
    echo   set Qt6_DIR=C:\Qt\6.11.1\mingw_1310_64\lib\cmake\Qt6
    exit /b 1
)
echo Qt6_DIR=%Qt6_DIR%

REM ---- 编译 ----
echo.
echo === 1/4 编译 Release ===
cmake -B "%BUILD_DIR%" -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER ^
    -DCMAKE_PREFIX_PATH="%Qt6_DIR%\..\..\..\.."
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build "%BUILD_DIR%" -j
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM ---- 部署 Qt DLL ----
echo.
echo === 2/4 部署 Qt DLL (windeployqt) ===
cmake --build "%BUILD_DIR%" --target deploy
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM ---- 打包 ZIP ----
echo.
echo === 3/4 打包 ZIP ===
set EXE_DIR=%BUILD_DIR%\frontend
for /f "tokens=*" %%v in ('cmake --version ^| findstr /r "[0-9]"') do echo %%v
set VERSION=0.2.0
set ZIP_NAME=OmniHealth_Profiler-%VERSION%-Windows-x64.zip
set ZIP_PATH=%BUILD_DIR%\%ZIP_NAME%

del "%ZIP_PATH%" 2>nul
powershell -command "Compress-Archive -Path '%EXE_DIR%\*' -DestinationPath '%ZIP_PATH%'"

echo.
echo === 4/4 完成 ===
echo ZIP: %ZIP_PATH%
echo.
echo 将 ZIP 发给老师，解压后双击 Health_Manager_App.exe 即可运行。
echo 注意: 如果缺少 VC++ 运行时，需安装:
echo   https://aka.ms/vs/17/release/vc_redist.x64.exe

endlocal
