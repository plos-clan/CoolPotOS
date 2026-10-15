@echo off
rem ============================================================
rem  CoolPotOS one-click build entry (double click to run)
rem  Default calls build.ps1 to build x86_64 Debug -> build\CoolPotOS.iso
rem
rem  pass-through args (or edit %* below / from command line):
rem    -Arch riscv64 / -Arch loongarch64   switch target arch
rem    -BuildType Release                  build Release
rem    -Run                                build and try to run QEMU
rem    -SkipImageBuild                     (docker) reuse built image
rem  examples:  build.bat -BuildType Release -Run
rem ============================================================
setlocal

rem locate script dir so double-click works from any CWD
set "ROOT=%~dp0"

if not exist "%ROOT%build.ps1" (
    echo [ERROR] build.ps1 not found next to build.bat.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%build.ps1" %*

if errorlevel 1 (
    echo.
    echo [FAILED] Build failed, see log above.
) else (
    echo.
    echo [DONE] Build finished.
)

echo.
pause
endlocal