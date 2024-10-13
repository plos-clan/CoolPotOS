import os
import sys

compiler_dir = "/i686_elf_tools/"

cd = os.getcwd()  # 获取当前执行目录 'D:\CrashPowerDOS-main\'
gcc = cd + compiler_dir + 'bin/i686-elf-gcc.exe -w -std=gnu99 -I include/ -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types '
asm = cd + compiler_dir + 'bin/i686-elf-as.exe'
nasm = "nasm -f elf32"
ld = cd + compiler_dir + 'bin/i686-elf-g++.exe'
out = "target"
dir_ = "\\"
src = "src\\"
raw_gcc = cd + compiler_dir + 'bin/i686-elf-gcc.exe'
make_apps = ["wsl make -i -r -C apps clean", "wsl make -r -C apps"]
build_command = "wsl grub-mkrescue -o cpos.iso isodir"

print("Build os: " + sys.platform)
if sys.platform.startswith('linux'):
    compiler_dir = "/i686-linux/"
    dir_ = "/"
    src = "src/"
    gcc = cd + compiler_dir + "bin/i686-elf-gcc -w -std=gnu99 -I include/ -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types "
    asm = cd + compiler_dir + "bin/i686-elf-as"
    ld = cd + compiler_dir + "bin/i686-elf-g++"
    raw_gcc = cd + compiler_dir + "bin/i686-elf-gcc"
    make_apps = ["make -i -r -C apps clean", "make -r -C apps"]
    build_command = "grub-mkrescue -o cpos.iso isodir"

print("GCC Version:")

os.system(raw_gcc + " --version")


def clean():
    print("Clean target folder")
    for file in os.listdir(cd + dir_ + "target"):  # 遍历指定文件夹下所有文件
        os.remove(cd + dir_ + "target" + dir_ + file)
    return 0


def build_boot():  # 构建引导程序
    print("Building boot source code...")
    status = True
    for file in os.listdir(cd + dir_ + src + 'boot'):
        if status and file == 'boot.asm':
            cmd = asm + " " + cd + dir_ + src + "boot" + dir_ + file + " -o " + cd + dir_ + "target" + dir_ + \
                  file.split(".")[0] + ".o"
            status = False
        else:
            cmd = nasm + " " + cd + dir_ + src + "boot" + dir_ + file + " -o " + cd + dir_ + "target" + dir_ + \
                  file.split(".")[0] + ".o"
        e = os.system(cmd)  # os.system 执行命令 e为返回值(非0即不正常退出,可做判断终止构建流程)
        if e != 0:
            return -1
    return 0


def build_driver():  # 构建内置驱动程序
    print("Building driver source code...")
    status_pci = True
    status_ide = True
    for file in os.listdir(cd + dir_ + src + 'driver'):
        if status_pci and (file == 'pci.c'):
            cmd = gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_pci = False
        elif status_ide and (file == 'ide.c'):
            cmd = gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_ide = False
        else:
            cmd = gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_kernel():  # 构建内核本体
    print("Building kernel source code...")
    for file in os.listdir(cd + dir_ + src + 'kernel'):
        cmd = gcc + "-O0 " + src + "kernel" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    for file in os.listdir(cd + dir_ + src + 'kernel' + dir_ + 'memory'):
        cmd = gcc + "-O0 " + src + "kernel" + dir_ + 'memory' + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_data():  # 构建常用工具
    print("Building util source code...")
    for file in os.listdir(cd + dir_ + src + 'util'):
        cmd = gcc + "-O0 " + src + "util" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_sysapp():  # 构建内置系统应用
    print("Building sysapp source code...")
    for file in os.listdir(cd + dir_ + src + 'sysapp'):
        cmd = gcc + "-Og " + src + "sysapp" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_network():  # 构建网络系统
    print("Building network source code...")
    for file in os.listdir(cd + dir_ + src + 'network'):
        cmd = gcc + "-O0 " + src + "network" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_fs():  # 构建文件系统
    print("Building fs source code...")
    for file in os.listdir(cd + dir_ + src + 'fs'):
        cmd = gcc + " " + src + "fs" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0

def syservice():
    print("Building syservice...")
    return os.system("make -r -C syservice")

def linker():  # 交叉编译链接
    print("Linking object files...")
    source_file = ""
    for file in os.listdir(cd + dir_ + 'target'):
        source_file = source_file + " target" + dir_ + file
    return os.system(
        ld + " -T linker.ld -o isodir" + dir_ + "sys" + dir_ + "cposkrnl.elf -ffreestanding -nostdlib " + source_file + " -lgcc")


clean()
a = build_boot()
if a != 0:
    exit(-1)
a = build_driver()
if a != 0:
    exit(-1)
a = build_kernel()
if a != 0:
    exit(-1)
a = build_data()
if a != 0:
    exit(-1)
a = build_sysapp()
if a != 0:
    exit(-1)
a = build_network()
if a != 0:
    exit(-1)
a = build_fs()
if a != 0:
    exit(-1)
a = syservice()
if a != 0:
    exit(-1)
a = linker()
if a != 0:
    exit(-1)

# """
print("Make apps")
for i in make_apps:
    if os.system(i) != 0:
        exit(-1)
    else:
        print("make win!")
# """

print("Building iso...")
if os.system(build_command) != 0:
    exit(-1)

os.system("qemu-system-i386 -cdrom cpos.iso -serial stdio -device sb16 -net nic,model=pcnet -m 4096")

# launch()