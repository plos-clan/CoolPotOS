# CoolPotOS 一键编译脚本 (Windows / PowerShell)
#
# 自动选择编译环境：
#   1) 有 Docker        -> 使用项目 Dockerfile 在容器内编译（最干净，无需装工具链）
#   2) 没有 Docker但有WSL -> 使用 WSL Ubuntu 编译（利用项目自带 build.sh 同款命令），
#                         自动补装缺失的 clang / cmake / python3-cryptography（需要一次 sudo 口令）
#
# 用法示例：
#   .\build.ps1                     # 默认编译 x86_64 Debug，产出 CoolPotOS.iso
#   .\build.ps1 -BuildType Release  # 编 Release 版
#   .\build.ps1 -Arch riscv64       # 编 riscv64（产出 cpkrnl_rv64.elf）
#   .\build.ps1 -Arch loongarch64   # 编 loongarch64（产出 cpkrnl_la64.elf）
#   .\build.ps1 -Run                # 编译后尝试用 QEMU 运行（需要图形环境）
#   .\build.ps1 -SkipImageBuild     # 仅 Docker 模式有效：复用已构建的 coolpotos 镜像

[CmdletBinding()]
param(
    [ValidateSet("x86_64", "riscv64", "loongarch64")]
    [string]$Arch = "x86_64",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Debug",

    # 目标：iso 仅编译产出镜像；run 还要用 QEMU 启动
    [ValidateSet("iso", "run", "kernel", "all")]
    [string]$Target = "iso",

    [string]$DockerImage = "coolpotos",
    [string]$DockerPlatform = "linux/amd64",
    [switch]$SkipImageBuild,
    [switch]$Run,
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# Windows 路径 -> WSL 挂载路径。盘符转小写（/mnt/f），其余保持原大小写
# 例：F:\Owlbyte\... -> /mnt/f/Owlbyte/...
$rootWsl = "/mnt/" + $root.Substring(0, 1).ToLower() + ($root.Substring(2) -replace '\\', '/')

if ($Jobs -le 0) {
    $ncpu = [Environment]::ProcessorCount
    if ($ncpu -lt 1) { $ncpu = 1 }
    $Jobs = $ncpu
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   CoolPotOS  一键编译" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  架构:        $Arch"
Write-Host "  构建类型:    $BuildType"
Write-Host "  目标:        $Target"
Write-Host "  并行任务:    $Jobs"
Write-Host ""

# ---------- 在 WSL 中执行一段 bash 脚本 ----------
# 直接用 UTF-8 字节流写入 stdin，避免中文路径在命令参数/管道中的编码与 \r 问题。
function Invoke-WslScript {
    param([string]$Script)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "wsl"
    $psi.Arguments = "-e bash"
    $psi.UseShellExecute = $false
    # 只重定向 stdin 用于喂脚本，stdout/stderr 保留给终端 -> 编译日志实时显示，sudo 可在终端输入口令
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

# ---------- 检测可用的编译环境 ----------
$docker = Get-Command docker -ErrorAction SilentlyContinue
$wsl    = Get-Command wsl    -ErrorAction SilentlyContinue

if ($docker) {
    Write-Host "[环境] 检测到 Docker，使用 Docker 容器编译。" -ForegroundColor Green
} elseif ($wsl) {
    Write-Host "[环境] 未检测到 Docker，改用 WSL Ubuntu 编译。" -ForegroundColor Green
} else {
    Write-Host "[错误] 未检测到 Docker，也未检测到 WSL。" -ForegroundColor Red
    Write-Host "   请任选其一：安装 Docker Desktop，或启用 WSL(允许Ubuntu)。" -ForegroundColor Yellow
    exit 1
}

# =================== Docker 模式 ===================
if ($docker) {
    docker version --format "{{.Server.Version}}" 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[错误] Docker 服务未运行，请先启动 Docker Desktop。" -ForegroundColor Red
        exit 1
    }

    if (-not $SkipImageBuild) {
        Write-Host "[1/2] 构建构建环境镜像: $DockerImage ..." -ForegroundColor Green
        docker build --platform $DockerPlatform -t $DockerImage $root
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[错误] 构建 Docker 镜像失败。" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "[1/2] 跳过镜像构建，复用: $DockerImage" -ForegroundColor Green
    }

    if ($Run -or $Target -eq "run") {
        if ($Target -ne "run") { $Target = "run" }
        $ttyArg = "-it"
    } else {
        $ttyArg = "-i"
    }

    # 拼装容器内 bash 命令
    $pipCheck = 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("cryptography") else 1)'
    $bashCmd  = "set -e; " +
                "if ! python3 -c '`"$pipCheck`"'; then " +
                "python3 -m pip install -q --break-system-packages cryptography || " +
                "python3 -m pip install -q cryptography; fi; " +
                "cmake -S . -B build/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON " +
                "-DCMAKE_BUILD_TYPE='$BuildType' -DTARGET_ARCH='$Arch'; " +
                "cmake --build build/ --target '$Target' -j '$Jobs'"

    Write-Host "[2/2] 在容器内编译 $Arch ..." -ForegroundColor Green
    docker run --rm $ttyArg --platform $DockerPlatform `
        -v "$root":/workspace `
        -w /workspace `
        -e BUILD_TYPE=$BuildType `
        -e TARGET_ARCH=$Arch `
        -e TARGET=$Target `
        -e JOBS=$Jobs `
        $DockerImage `
        bash -lc $bashCmd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[错误] 编译失败，请查看上方日志。" -ForegroundColor Red
        exit 1
    }

# =================== WSL 模式 ===================
} elseif ($wsl) {
    Write-Host "[1/4] 检查 WSL 环境、盘符挂载与工具链 ..." -ForegroundColor Green

    # 项目所在盘符（如 F: -> f，用于挂载 /mnt/f）
    $drv       = $root.Substring(0, 1)
    $drvLower  = $drv.ToLower()

    # WSL sudo 密码（设置后所有 sudo 命令自动使用，无需手动输入）
    # 如果你的密码不是 000000，请修改下面这行
    $sudoPwd   = "000000"

    # WSL 内的原生 Linux 工作目录（避免在 Windows 文件系统上编译的兼容性问题）
    $wslBuildDir = "~/CoolPotOS-build"

    # 工具链就绪判断
    $toolReady = "command -v clang >/dev/null 2>&1 && command -v cmake >/dev/null 2>&1 && " +
                 "python3 -c 'import cryptography' >/dev/null 2>&1"

    $wslScript = @()
    $wslScript += "set -e"
    # 若项目所在盘未被 WSL 挂载，则尝试挂载（自动提供密码）
    $wslScript += "if ! mountpoint -q /mnt/$drvLower 2>/dev/null; then " +
                  "echo '[WSL] 未检测到 /mnt/$drvLower 已挂载，正在挂载 ${drv}:'; " +
                  "echo '$sudoPwd' | sudo -S mkdir -p /mnt/$drvLower 2>/dev/null; " +
                  "echo '$sudoPwd' | sudo -S mount -t drvfs '${drv}:' /mnt/$drvLower; " +
                  "fi"
    # 缺失则安装 clang / cmake / python3-cryptography（自动提供密码）
    $wslScript += "if $toolReady; then echo '[WSL] 工具链已就绪'; " +
                  "else echo '[WSL] 正在安装 clang/cmake/python3-cryptography...'; " +
                  "echo '$sudoPwd' | sudo -S apt-get update -qq 2>/dev/null; " +
                  "DEBIAN_FRONTEND=noninteractive echo '$sudoPwd' | sudo -S apt-get install -y -qq clang cmake lld xorriso python3-cryptography git squashfs-tools cpio 2>/dev/null; fi"

    Write-Host "[2/4] 复制项目到 WSL 本地文件系统 ..." -ForegroundColor Green
    # 复制项目到 WSL 本地文件系统（避免 9p 文件系统的 configure_file 限制）
    # 排除 build/ 目录，避免残留的 CMakeCache 导致路径冲突
    $wslScript += "echo '[WSL] 清理旧的构建目录...'; rm -rf $wslBuildDir 2>/dev/null || true"
    $wslScript += "echo '[WSL] 复制项目到 $wslBuildDir (排除 build/)...'; mkdir -p $wslBuildDir"
    $wslScript += "rsync -a --exclude='build/' --exclude='.git/' '$rootWsl/' $wslBuildDir/"
    $wslScript += "cd $wslBuildDir && pwd"

    Write-Host "[3/4] 在 WSL 中配置并编译 $Arch ..." -ForegroundColor Green

    # 自动检测 Windows 代理并配置给 Git（解决 WSL 无法直连 GitHub 的问题）
    $proxyUrl = $null
    # 检测常见代理端口
    @("127.0.0.1:7890", "127.0.0.1:10809", "127.0.0.1:1080", "127.0.0.1:8080") | ForEach-Object {
        try {
            $tcp = New-Object System.Net.Sockets.TcpClient($_.Split(':')[0], [int]$_.Split(':')[1])
            $tcp.Close()
            $proxyUrl = "http://$_"
            Write-Host "[WSL] 检测到代理: $proxyUrl" -ForegroundColor Yellow
        } catch {}
    }
    # 获取 Windows 主机 IP（用于 WSL 访问 Windows 上的代理）
    $winHostIp = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -like '*vEthernet*' -or $_.InterfaceAlias -like '*WSL*' }).IPAddress
    if (-not $winHostIp) {
        # 备选方案：让 WSL 自己查询网关（Windows 主机 IP）
        $gatewayScript = "ip route show default | awk '{print `$3}'"
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "wsl"
        $psi.Arguments = "-e bash -c '$gatewayScript'"
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $p = [System.Diagnostics.Process]::Start($psi)
        $winHostIp = $p.StandardOutput.ReadToEnd().Trim()
        $p.WaitForExit()
    }
    if (-not $winHostIp) { $winHostIp = "172.0.0.1" }  # 兜底

    # 配置 Git 使用 HTTPS + 镜像站（解决 WSL 无法直连 GitHub 的问题）
    # 使用 ghproxy.com 镜像加速 GitHub 访问
    $wslScript += "git config --global url.'https://ghproxy.com/https://github.com/'.insteadOf 'https://github.com/'"
    $wslScript += "git config --global url.'https://ghproxy.com/https://github.com/'.insteadOf 'git@github.com:'"
    if ($proxyUrl) {
        # 把 127.0.0.1 替换成 Windows 主机 IP，因为 WSL 里 127.0.0.1 是 WSL 自身
        $wslProxyUrl = $proxyUrl -replace '127\.0\.0\.1', $winHostIp
        $wslScript += "export http_proxy='$wslProxyUrl'"
        $wslScript += "export https_proxy='$wslProxyUrl'"
        Write-Host "[WSL] 已配置 GitHub 镜像: ghproxy.com (备用代理: $wslProxyUrl)" -ForegroundColor Green
    } else {
        Write-Host "[WSL] 已配置 GitHub 镜像: ghproxy.com" -ForegroundColor Green
    }
    $wslScript += "export GIT_DISCOVERY_ACROSS_FILESYSTEM=1"
    # 直接复用项目自带的 build.sh（其本身已完成 cmake 配置 + 编译）
    $wslScript += "bash ./build.sh -direct --arch '$Arch' --build-type '$BuildType' --target '$Target' -j '$Jobs'"

    Write-Host "[4/4] 复制产物回 Windows ..." -ForegroundColor Green
    # 把 build 目录里的产物复制回 Windows 原始位置
    $wslScript += "echo '[WSL] 复制编译产物回 $rootWsl/build ...'"
    $wslScript += "mkdir -p '$rootWsl/build'"
    $wslScript += "cp -r ./build/* '$rootWsl/build/' 2>/dev/null || true"
    $wslScript += "echo '[WSL] 完成！产物位于: $rootWsl/build/'"

    $scriptText = ($wslScript -join "`n") + "`n"

    Write-Host "(自动挂载盘符、复制项目、编译、回传产物，全程无需手动操作)" -ForegroundColor Yellow
    $exitWsl = Invoke-WslScript -Script $scriptText
    if ($exitWsl -ne 0) {
        Write-Host "[错误] WSL 编译失败，请查看上方日志。" -ForegroundColor Red
        exit 1
    }
}

# =================== 提示产物位置 ===================
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  编译成功！" -ForegroundColor Green
switch ($Arch) {
    "x86_64"     { $artifact = "$root\build\CoolPotOS.iso" }
    "riscv64"    { $artifact = "$root\build\cpkrnl_rv64.elf" }
    "loongarch64"{ $artifact = "$root\build\cpkrnl_la64.elf" }
}
if (Test-Path $artifact) {
    Write-Host "  产物: $artifact" -ForegroundColor Green
} else {
    Write-Host "  产物位置见: $root\build\" -ForegroundColor Green
}

if ($Target -eq "run") {
    Write-Host "  已请求运行，QEMU 窗口需图形环境支持。" -ForegroundColor Yellow
} else {
    Write-Host "  提示: 如需用 QEMU 启动，请执行 .\build.ps1 -Run" -ForegroundColor Yellow
}
Write-Host "============================================" -ForegroundColor Green
exit 0