# CoolPotOS 一键运行脚本 (Windows / PowerShell)
#
# 通过 QEMU 启动 CoolPotOS.iso。优先使用 Windows 本地 QEMU，
# 未检测到本地 QEMU 时回退到 WSL 中的 QEMU。
#
# 用法示例：
#   .\run.ps1                    # 用 QEMU 启动 x86_64 的 CoolPotOS 镜像
#   .\run.ps1 -Gdb               # 附加 -s -S，等待 gdb 连接调试
#   .\run.ps1 -Arch riscv64      # 运行 riscv64（需要对应 QEMU 与镜像）
#   .\run.ps1 -AutoBuild         # 若发现没有 ISO，先自动调用 build.ps1 编译

[CmdletBinding()]
param(
    [ValidateSet("x86_64", "riscv64", "loongarch64")]
    [string]$Arch = "x86_64",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Debug",

    # 附加 -s -S：开启 gdb 远程调试断点等待。
    [switch]$Gdb,

    # 若未发现 ISO，先自动编译。
    [switch]$AutoBuild,

    # 内存大小(默认 2048M)
    [string]$Memory = "2048M",

    # 本地 QEMU 安装目录。
    [string]$LocalQemuDir = "C:\Program Files\qemu",

    # WSL sudo 密码（本地 QEMU 模式下用不到，仅回退 WSL 时使用）
    [string]$SudoPwd = "000000"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# Windows 路径（用于本地 QEMU 直接访问）
$rootWsl = "/mnt/" + $root.Substring(0, 1).ToLower() + ($root.Substring(2) -replace '\\', '/')
$wslBuildDir = "~/CoolPotOS-build"

$drv      = $root.Substring(0, 1)
$drvLower = $drv.ToLower()

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   CoolPotOS  一键运行 (QEMU)" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  架构:        $Arch"
Write-Host "  内存:        $Memory"
Write-Host "  调试:        $(if ($Gdb) { '是 (等待 gdb: -s -S)' } else { '否' })"
Write-Host ""

# 自动编译开关
if ($AutoBuild) {
    & "$root\build.ps1" -Arch $Arch -BuildType $BuildType
}

# ---------- 找出 ISO 文件 ----------
function Resolve-Iso {
    # 本地 build\ 优先
    if (Test-Path "$root\build\CoolPotOS.iso") { return "$root\build\CoolPotOS.iso" }
    return $null
}
$localIso = Resolve-Iso

# 把路径复制到纯 ASCII 目录（C:\Users\Public），
# 规避 PowerShell 5.1 调用原生 QEMU 时中文路径被按 GBK/UTF-8 误编码的问题。
# 返回复制后的 ASCII 路径。
function Copy-ToAscii {
    param([string]$Path)
    $destDir = "C:\Users\Public\CoolPotOS-run"
    if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
    $name = [System.IO.Path]::GetFileName($Path)
    $dest = Join-Path $destDir $name
    Copy-Item -Path $Path -Destination $dest -Force -ErrorAction SilentlyContinue
    if (Test-Path $dest) { return $dest }
    # 复制失败则回退原路径
    return $Path
}

# ---------- 在 WSL 中执行一段 bash 脚本（UTF-8 字节流）---------- 
function Invoke-WslScript {
    param([string]$Script)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "wsl"
    $psi.Arguments = "-e bash"
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput  = $true
    $psi.RedirectStandardOutput = $false
    $psi.RedirectStandardError  = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $enc = New-Object System.Text.UTF8Encoding($false)
    $bytes = $enc.GetBytes($Script)
    $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    $p.StandardInput.BaseStream.Flush()
    $p.StandardInput.Close()
    $p.WaitForExit()
    return $p.ExitCode
}

# 组装 QEMU 启动参数（仅 CLI args，不含可执行文件路径）
function Get-QemuArgs {
    param([string]$IsoPath)
    if ($Arch -eq "x86_64") {
        # 参考项目 run target 的参数
        $args = @(
            "-M", "q35",
            "-cpu", "Haswell,+x2apic,+avx",
            "-smp", "1",
            "-serial", "stdio",
            "-m", "$Memory",
            "-audiodev", "sdl,id=audio0",
            "-device", "sb16,audiodev=audio0",
            "-netdev", "user,id=net0",
            "-device", "e1000,netdev=net0",
            "-rtc", "base=utc",
            "-vga", "vmware",
            "-cdrom", "$IsoPath"
        )
        # 若有 ovmf 固件则附加（同样复制到 ASCII 目录）
        $ovmf = Join-Path $root "assets\ovmf-code_x64.fd"
        if (Test-Path $ovmf) {
            $ovmfAscii = Copy-ToAscii $ovmf
            $args += @("-drive", "if=pflash,format=raw,file=$ovmfAscii")
        }
        if ($Gdb) { $args += "-s"; $args += "-S" }
        return $args
    }
    elseif ($Arch -eq "riscv64") {
        $args = @(
            "-machine", "virt",
            "-smp", "1",
            "-serial", "stdio",
            "-m", "$Memory",
            "-cdrom", "$IsoPath"
        )
        if ($Gdb) { $args += "-s"; $args += "-S" }
        return $args
    }
    return $null
}

# ---------- 尝试本地 QEMU ----------
$localQemu = $null
$exeName = switch ($Arch) {
    "x86_64"        { "qemu-system-x86_64.exe" }
    "riscv64"       { "qemu-system-riscv64.exe" }
    "loongarch64"   { "qemu-system-loongarch64.exe" }
}
$localQemu = Join-Path $LocalQemuDir $exeName
if (-not (Test-Path $localQemu)) {
    # 也尝试 PATH 里的 qemu
    $inPath = Get-Command $exeName -ErrorAction SilentlyContinue
    if ($inPath) { $localQemu = $inPath.Source } else { $localQemu = $null }
}

if ($localQemu -and $localIso) {
    $isoAscii = Copy-ToAscii $localIso
    Write-Host "[RUN] 使用本地 QEMU: $localQemu" -ForegroundColor Green
    Write-Host "[RUN] 使用镜像:   $localIso" -ForegroundColor Green
    Write-Host "[RUN] QEMU载入:   $isoAscii" -ForegroundColor DarkGray
    $qemuArgs = Get-QemuArgs -IsoPath $isoAscii
    if ($null -eq $qemuArgs) {
        Write-Host "[错误] $Arch 暂不支持本地 QEMU 启动参数。" -ForegroundColor Red
        exit 1
    }
    Write-Host "[RUN] 启动 QEMU ...（关闭 QEMU 窗口后脚本结束）" -ForegroundColor Cyan
    & $localQemu @qemuArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[错误] QEMU 启动失败，代码 $LASTEXITCODE。" -ForegroundColor Red
        exit 1
    }
    Write-Host "[完成] QEMU 已退出。" -ForegroundColor Green
    exit 0
}

# ---------- 回退：WSL 中的 QEMU ----------
Write-Host "[RUN] 未找到本地 QEMU 或本地 ISO，尝试在 WSL 中运行。" -ForegroundColor Yellow

$wslCmd = Get-Command wsl -ErrorAction SilentlyContinue
if (-not $wslCmd) {
    Write-Host "[错误] 本地无 QEMU，也无 WSL。请安装 QEMU（C:\Program Files\qemu）或启用 WSL。" -ForegroundColor Red
    exit 1
}

$s = @()
$s += "set -e"
$s += "if ! mountpoint -q /mnt/$drvLower 2>/dev/null; then " +
      "echo '[RUN] 正在挂载 ${drv}:'; " +
      "echo '$SudoPwd' | sudo -S mkdir -p /mnt/$drvLower 2>/dev/null; " +
      "echo '$SudoPwd' | sudo -S mount -t drvfs '${drv}:' /mnt/$drvLower; fi"

$s += "if ! command -v qemu-system-x86_64 >/dev/null 2>&1 && [ '$Arch' != 'loongarch64' ]; then " +
      "echo '[RUN] 安装 qemu-system-x86 ...'; " +
      "echo '$SudoPwd' | sudo -S apt-get update -qq 2>/dev/null; " +
      "DEBIAN_FRONTEND=noninteractive echo '$SudoPwd' | sudo -S apt-get install -y -qq qemu-system-x86 2>/dev/null; fi"

$s += "if [ -f $wslBuildDir/build/CoolPotOS.iso ]; then ISO=$wslBuildDir/build/CoolPotOS.iso; " +
      "elif [ -f '$rootWsl/build/CoolPotOS.iso' ]; then ISO='$rootWsl/build/CoolPotOS.iso'; " +
      "else echo '[RUN] 未找到 CoolPotOS.iso，请先运行 .\\build.ps1 编译，或用 -AutoBuild'; exit 1; fi"
$s += "echo '[RUN] 使用镜像: ' \$ISO; ls -lh \$ISO"

# gdb 参数(脚本层面预计算，避免嵌套引号)
$gdbArg = if ($Gdb) { "-s -S" } else { "" }

if ($Arch -eq "riscv64") {
    $s += "qemu-system-riscv64 -machine virt -smp 1 -m $Memory -serial stdio -cdrom \$ISO $gdbArg"
} elseif ($Arch -eq "loongarch64") {
    $s += "echo '[RUN] loongarch 尚未内置 QEMU 启动参数，请手动运行带对应机器的 qemu-system-loongarch64'"
} else {
    $s += "cd $wslBuildDir && qemu-system-x86_64 -M q35 -cpu Haswell,+x2apic,+avx -smp 1 " +
          "-serial stdio -m $Memory -audiodev sdl,id=audio0 -device sb16,audiodev=audio0 " +
          "-netdev user,id=net0 -device e1000,netdev=net0 -rtc base=utc -vga vmware " +
          "-cdrom \$ISO $gdbArg"
}

$s += "echo '[RUN] QEMU (WSL) 已启动，关闭 QEMU 窗口后脚本结束。'"
Write-Host "[RUN] 在 WSL 中启动 QEMU ..." -ForegroundColor Green
$scriptText = ($s -join "`n") + "`n"
$exitWsl = Invoke-WslScript -Script $scriptText
if ($exitWsl -ne 0) {
    Write-Host "[错误] 运行失败，请查看上方日志。" -ForegroundColor Red
    exit 1
}
Write-Host "[完成] QEMU 已退出。" -ForegroundColor Green