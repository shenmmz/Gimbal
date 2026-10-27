@echo off
rem 把本项目的构建环境加入当前命令行（只影响本窗口）
rem 依赖：Visual Studio（提供 MSVC + CMake + Ninja，无需单独下载）
rem       Arm GNU Toolchain 装在 %GIMBAL_TOOLS%（默认 %USERPROFILE%\tools）

if "%GIMBAL_TOOLS%"=="" set "GIMBAL_TOOLS=%USERPROFILE%\tools"

rem 1) MSVC 环境：vcvars64 会一并配好 INCLUDE / LIB / rc.exe / mt.exe
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
)
if defined VSPATH (
    if exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

rem 2) 复用 VS 自带的 CMake 与 Ninja
if defined VSPATH (
    if exist "%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" (
        set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    )
    if exist "%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja" (
        set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
    )
)

rem 3) Arm GNU Toolchain + OpenOCD（需要单独安装的两样）
set "PATH=%GIMBAL_TOOLS%\arm-gnu-toolchain\bin;%GIMBAL_TOOLS%\openocd\bin;%GIMBAL_TOOLS%\ninja;%PATH%"

echo [gimbal] 环境就绪
where cmake >nul 2>nul || echo   [缺少] cmake
where ninja >nul 2>nul || echo   [缺少] ninja
where cl    >nul 2>nul || echo   [缺少] MSVC (cl.exe)
where arm-none-eabi-gcc >nul 2>nul || echo   [缺少] arm-none-eabi-gcc  --^> 运行 scripts\setup-toolchain.cmd
where openocd >nul 2>nul || echo   [缺少] openocd
