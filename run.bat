@echo off
rem ============================================================
rem  CoolPotOS one-click RUN entry (double click to run)
rem  Boots CoolPotOS.iso inside WSL via QEMU.
rem  Prerequisite: run build.ps1 / build.bat first (or use -AutoBuild).
rem
rem  pass-through args:
rem    -Gdb                                wait for gdb (-s -S)
rem    -AutoBuild                          build first if ISO missing
rem    -Arch riscv64 / -Arch loongarch64
rem    -Memory 4096M
rem  examples:  run.bat -AutoBuild
rem ============================================================
setlocal

set "ROOT=%~dp0"

if not exist "%ROOT%run.ps1" (
    echo [ERROR] run.ps1 not found next to run.bat.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%run.ps1" %*

if errorlevel 1 (
    echo.
    echo [FAILED] Run failed, see log above.
) else (
    echo.
    echo [DONE] QEMU exited.
)

echo.
pause
endlocal