@echo off
setlocal
rem ============================================================
rem  只安装 Arm GNU Toolchain（CMake 与 Ninja 直接复用 Visual Studio 自带的）
rem  在你自己打开的 cmd 窗口里运行
rem  安装位置：%USERPROFILE%\tools\arm-gnu-toolchain
rem  已存在的文件会跳过，可重复执行，支持断点续传
rem ============================================================

set "TOOLS=%USERPROFILE%\tools"
set "DL=%TOOLS%\_dl"
if not exist "%TOOLS%" mkdir "%TOOLS%"
if not exist "%DL%" mkdir "%DL%"

set "GCC_URL=https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-win32-x64.zip"
set "GCC_SIZE=323368593"
set "GCC_DIR=xpack-arm-none-eabi-gcc-14.2.1-1.1"

echo [1/3] 下载 Arm GNU Toolchain（323 MB，唯一需要下载的东西）
call :fetch "%GCC_URL%" "%DL%\armgcc.zip" %GCC_SIZE% || goto :fail

echo [2/3] 解压到 %TOOLS%
if exist "%TOOLS%\arm-gnu-toolchain\bin\arm-none-eabi-gcc.exe" (
    echo     已存在，跳过
) else (
    tar -xf "%DL%\armgcc.zip" -C "%TOOLS%" || goto :fail
    if exist "%TOOLS%\%GCC_DIR%" move "%TOOLS%\%GCC_DIR%" "%TOOLS%\arm-gnu-toolchain" >nul
)

echo [3/3] 写入用户 PATH
powershell -NoProfile -ExecutionPolicy Bypass -Command "$x='%TOOLS%\arm-gnu-toolchain\bin'; $p=[Environment]::GetEnvironmentVariable('Path','User'); if($null -eq $p){$p=''}; if($p -notlike ('*'+$x+'*')){ $p=$p.TrimEnd(';')+';'+$x; [Environment]::SetEnvironmentVariable('Path',$p,'User'); Write-Host '    已追加到用户 PATH' } else { Write-Host '    已在 PATH 中，跳过' }"

echo.
echo ===== 验证 =====
"%TOOLS%\arm-gnu-toolchain\bin\arm-none-eabi-gcc.exe" --version 2>nul | findstr /r "arm-none-eabi" || echo   [失败] 解压后找不到 arm-none-eabi-gcc

echo.
echo 安装完成。新开一个终端，执行 scripts\env.cmd 后即可：
echo   cmake --preset arm
echo   cmake --build --preset arm
goto :eof

:fetch
rem %1=url %2=输出文件 %3=期望字节数
if exist "%~2" (
    for %%A in ("%~2") do if %%~zA GEQ %3 (
        echo     已存在且完整，跳过
        exit /b 0
    )
)
curl -L --retry 5 --retry-delay 3 -C - -o "%~2" "%~1"
if errorlevel 1 exit /b 1
for %%A in ("%~2") do if %%~zA LSS %3 (
    echo     [失败] 下载不完整：%%~zA / %3 字节
    exit /b 1
)
exit /b 0

:fail
echo.
echo 安装中断。重新运行本脚本会从断点继续。
exit /b 1
