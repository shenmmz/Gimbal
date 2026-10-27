@echo off
setlocal
rem ============================================================
rem  Install ONLY the Arm GNU Toolchain.
rem  CMake and Ninja are reused from Visual Studio, nothing to install.
rem  Run this in your own cmd window.
rem  Target: %USERPROFILE%\tools\arm-gnu-toolchain
rem  Existing files are skipped, safe to re-run, download resumes.
rem
rem  NOTE - ASCII ONLY, DO NOT ADD CHINESE TEXT HERE.
rem  cmd.exe parses batch files in the OEM codepage (936/GBK on a Chinese
rem  Windows). UTF-8 Chinese comments can swallow the trailing CR, merging
rem  two lines into one; see the header of env.cmd for the full story.
rem ============================================================

set "TOOLS=%USERPROFILE%\tools"
set "DL=%TOOLS%\_dl"
if not exist "%TOOLS%" mkdir "%TOOLS%"
if not exist "%DL%" mkdir "%DL%"

set "GCC_URL=https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-win32-x64.zip"
set "GCC_SIZE=323368593"
set "GCC_DIR=xpack-arm-none-eabi-gcc-14.2.1-1.1"

echo [1/3] Downloading Arm GNU Toolchain (323 MB, the only download)
call :fetch "%GCC_URL%" "%DL%\armgcc.zip" %GCC_SIZE% || goto :fail

echo [2/3] Extracting to %TOOLS%
if exist "%TOOLS%\arm-gnu-toolchain\bin\arm-none-eabi-gcc.exe" (
    echo     already present, skipped
) else (
    tar -xf "%DL%\armgcc.zip" -C "%TOOLS%" || goto :fail
    if exist "%TOOLS%\%GCC_DIR%" move "%TOOLS%\%GCC_DIR%" "%TOOLS%\arm-gnu-toolchain" >nul
)

echo [3/3] Appending to user PATH
powershell -NoProfile -ExecutionPolicy Bypass -Command "$x='%TOOLS%\arm-gnu-toolchain\bin'; $p=[Environment]::GetEnvironmentVariable('Path','User'); if($null -eq $p){$p=''}; if($p -notlike ('*'+$x+'*')){ $p=$p.TrimEnd(';')+';'+$x; [Environment]::SetEnvironmentVariable('Path',$p,'User'); Write-Host '    appended to user PATH' } else { Write-Host '    already in PATH, skipped' }"

echo.
echo ===== verify =====
"%TOOLS%\arm-gnu-toolchain\bin\arm-none-eabi-gcc.exe" --version 2>nul | findstr /r "arm-none-eabi" || echo   [FAIL] arm-none-eabi-gcc not found after extraction

echo.
echo Done. Open a NEW terminal, run scripts\env.cmd, then:
echo   cmake --preset arm
echo   cmake --build --preset arm
goto :eof

:fetch
rem %1=url %2=output file %3=expected byte count
if exist "%~2" (
    for %%A in ("%~2") do if %%~zA GEQ %3 (
        echo     already present and complete, skipped
        exit /b 0
    )
)
curl -L --retry 5 --retry-delay 3 -C - -o "%~2" "%~1"
if errorlevel 1 exit /b 1
for %%A in ("%~2") do if %%~zA LSS %3 (
    echo     [FAIL] incomplete download: %%~zA / %3 bytes
    exit /b 1
)
exit /b 0

:fail
echo.
echo Interrupted. Re-running this script resumes the download.
exit /b 1
