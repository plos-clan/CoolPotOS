FROM ubuntu:24.04

# 避免 apt 交互式提示（比如时区选择）
ENV DEBIAN_FRONTEND=noninteractive

# 安装所有必要工具
RUN apt-get update && \
    apt-get install -y \
        clang \
        lld \
        xorriso \
        qemu-system-x86 \
        qemu-system-riscv64 \
        cmake \
        python3 \
        python3-cryptography \
        python3-pip \
        python3-venv \
        git \
        build-essential \
        && \
    rm -rf /var/lib/apt/lists/*

# 设置默认工作目录（可以按需修改）
WORKDIR /workspace

# 可选：设置默认命令（比如启动 shell）
CMD ["/bin/bash"]
