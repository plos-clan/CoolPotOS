import os
import sys


def check_os():
    if sys.platform.startswith('darwin'):
        return "Mac OS X"
    elif sys.platform.startswith('win'):
        return "Windows"
    elif sys.platform.startswith('linux'):
        return "Linux"
    else:
        return "Unknown"


if check_os() == "Windows":
    gcc = '/i686_elf_tools/bin/i686-elf-gcc.exe -w -std=gnu99 -I include/ -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types '
    asm = '/i686_elf_tools/bin/i686-elf-as.exe'
    nasm = "nasm -f elf32"
    ld = '/i686_elf_tools/bin/i686-elf-g++.exe'
    cd = os.getcwd()  # 获取当前执行目录 'D:\CrashPowerDOS-main\'
    out = "target"
    dir_ = "\\"
    src = "src\\"
elif check_os() == "Linux":
    gcc = 'gcc.exe -w -std=gnu99 -I include/ -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types '
    asm = 'gcc.exe'
    nasm = "nasm -f elf32"
    ld = 'g++.exe'
    cd = os.getcwd()  # 获取当前执行目录 '\mnt\d\CrashPowerDOS-main\'
    out = "target"
    dir_ = "/"
    src = "./src/"


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
            cmd = cd + asm + " " + cd + dir_ + src + "boot" + dir_ + file + " -o " + cd + dir_ + "target" + dir_ + \
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
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_pci = False
        elif status_ide and (file == 'ide.c'):
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
            status_ide = False
        else:
            cmd = cd + gcc + "-O0 " + src + "driver" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_kernel():  # 构建内核本体
    print("Building kernel source code...")
    for file in os.listdir(cd + dir_ + src + 'kernel'):
        cmd = cd + gcc + "-O0 " + src + "kernel" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_data():  # 构建常用工具
    print("Building util source code...")
    for file in os.listdir(cd + dir_ + src + 'util'):
        cmd = cd + gcc + "-O0 " + src + "util" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_sysapp():  # 构建内置系统应用
    print("Building sysapp source code...")
    for file in os.listdir(cd + dir_ + src + 'sysapp'):
        cmd = cd + gcc + "-O0 " + src + "sysapp" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_network():  # 构建网络系统
    print("Building network source code...")
    for file in os.listdir(cd + dir_ + src + 'network'):
        cmd = cd + gcc + "-O0 " + src + "network" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def build_fs():  # 构建文件系统
    print("Building fs source code...")
    for file in os.listdir(cd + dir_ + src + 'fs'):
        cmd = cd + gcc + " " + src + "fs" + dir_ + file + " -o " + "target" + dir_ + file.split(".")[0] + ".o"
        e = os.system(cmd)
        if e != 0:
            return -1
    return 0


def linker():  # 交叉编译链接
    print("Linking object files...")
    source_file = ""
    for file in os.listdir(cd + dir_ + 'target'):
        source_file = source_file + " target" + dir_ + file
    return os.system(
        cd + "/i686_elf_tools/bin/i686-elf-g++.exe -T linker.ld -o isodir" + dir_ + "sys" + dir_ + "kernel.elf -ffreestanding -nostdlib " + source_file + " -lgcc")


def launch():
    if check_os() == "Windows":
        if len(sys.argv) == 0 or sys.argv[1] == 'vga':
            print("Graphics MODE [VGA]")
            os.system("qemu-system-i386 -net nic,model=pcnet -net user -kernel isodir\\sys\\kernel.elf -hda diskx.img")
        elif sys.argv[1] == 'vbe':
            print("Graphics MODE [VBE]")
            os.system("qemu-system-i386 -vga std -net nic,model=pcnet -net user -kernel isodir\\sys\\kernel.elf -hda diskx.img")
    elif check_os() == "Linux":
        os.system("grub-mkrescue -o cpos.iso isodir")
        os.system("qemu-system-i386 -vga std -cdrom cpos.iso")

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
a = linker()
if a != 0:
    exit(-1)
print("Launching i386 vm...")

# launch()
