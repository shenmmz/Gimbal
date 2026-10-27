@echo off
rem ============================================================
rem  Put the build environment into the CURRENT console window.
rem  Deps: Visual Studio (gives MSVC + CMake + Ninja, no extra download)
rem        Arm GNU Toolchain installed under %GIMBAL_TOOLS%
rem        (default %USERPROFILE%\tools)
rem
rem  TWO RULES FOR THIS FILE, both learned the hard way:
rem
rem  1) ASCII ONLY. cmd.exe parses batch files in the OEM codepage (936/GBK
rem     on a Chinese Windows). A UTF-8 Chinese comment is read as a
rem     misaligned double-byte sequence; when a lead byte lands next to CR,
rem     that CR is eaten as the trail byte, the line does not terminate, it
rem     merges with the next one and cmd runs the comment as a command.
rem     Symptom: random "xxx is not recognized as an internal or external
rem     command". git diff shows nothing.
rem
rem  2) NO PARENTHESIZED BLOCKS around anything that expands a path.
rem     %ProgramFiles(x86)% expands to "C:\Program Files (x86)" and cmd
rem     counts that ')' as the end of the block, then chokes on the rest.
rem     That is why the logic below uses chained "if ... if ..." and a temp
rem     file instead of if-blocks and for-loops.
rem ============================================================

if "%GIMBAL_TOOLS%"=="" set "GIMBAL_TOOLS=%USERPROFILE%\tools"

rem 1) Locate Visual Studio. vswhere prints the path; a temp file carries it
rem    back, because "for /f ... in (`...`)" breaks on the (x86) parens.
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSTMP=%TEMP%\gimbal_vspath.txt"
if exist "%VSWHERE%" "%VSWHERE%" -latest -property installationPath > "%VSTMP%" 2>nul
if exist "%VSTMP%" set /p VSPATH=<"%VSTMP%"
if exist "%VSTMP%" del "%VSTMP%" >nul 2>nul

rem 2) MSVC environment: vcvars64 also sets INCLUDE / LIB / rc.exe / mt.exe
if defined VSPATH if exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul

rem 3) Reuse the CMake and Ninja that ship with Visual Studio
if defined VSPATH if exist "%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
if defined VSPATH if exist "%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja" set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

rem 4) Arm GNU Toolchain + OpenOCD (the only two things installed separately)
set "PATH=%GIMBAL_TOOLS%\arm-gnu-toolchain\bin;%GIMBAL_TOOLS%\openocd\bin;%GIMBAL_TOOLS%\ninja;%PATH%"

echo [gimbal] environment ready
where cmake >nul 2>nul || echo   [missing] cmake
where ninja >nul 2>nul || echo   [missing] ninja
where cl    >nul 2>nul || echo   [missing] MSVC cl.exe
where arm-none-eabi-gcc >nul 2>nul || echo   [missing] arm-none-eabi-gcc  -- run scripts\setup-toolchain.cmd
where openocd >nul 2>nul || echo   [missing] openocd
