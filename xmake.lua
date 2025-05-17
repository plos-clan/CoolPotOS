set_project("CoolPotOS")
add_rules("mode.debug", "mode.release")
set_languages("clatest")
set_warnings("all", "extra")
set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

target("limine")
    set_kind("binary")
    set_default(false)

    add_files("thirdparty/limine/*.c")
    add_includedirs("thirdparty/limine")

target("os-terminal")
    set_kind("phony")
    set_default(false)
    set_policy("build.fence", true)

    on_build(function (target)
        import("core.project.project")
        local src_dir = "thirdparty/libos-terminal"
        local build_dir = "$(buildir)/.build_cache/os-terminal"

        os.setenv("FONT_PATH", "../fonts/SourceCodePro.otf")
        os.cd("thirdparty/libos-terminal")
        os.exec("cargo build --release "..
            "--features embedded-font "..
            "--target-dir %s", build_dir)
        os.exec("cbindgen --output %s/os_terminal.h", build_dir)
    end)

target("pl_readline")
    set_kind("static")
    set_toolchains("clang")

    local base_dir = "thirdparty/pl_readline"
    add_files(base_dir.."/src/*.c")
    add_includedirs(base_dir.."/include", {public = true})

    add_defines("PL_ENABLE_HISTORY_FILE=0")
    add_cflags("-mno-80387", "-mno-mmx", "-DNDEBUG")
    add_cflags("-mno-sse", "-mno-sse2", "-mno-red-zone")
    add_cflags("-nostdlib", "-fno-builtin", "-fno-stack-protector")

target("kernel32")
    set_arch("i386")
    set_kind("binary")
    set_default(false)
    set_toolchains("@zig", "nasm")
    set_toolset("as", "nasm")

    add_cflags("-target x86-freestanding")
    add_arflags("-target x86-freestanding")
    add_ldflags("-target x86-freestanding")

    add_linkdirs("libs/i386")
    add_includedirs("src/i386/include")
    add_files("src/i386/**.asm", "src/i386/**.c")

    add_links("os_terminal")
    add_links("elf_parse")
    add_links("alloc")

    add_asflags("-f", "elf32")
    add_ldflags("-T", "src/i386/linker.ld")
    add_cflags("-mno-mmx", "-mno-sse", "-mno-sse2")

target("kernel64")
    set_arch("x86_64")
    set_kind("binary")
    set_default(false)
    set_toolchains("clang")
    add_deps("pl_readline", "os-terminal")

    add_cflags("-target x86_64-freestanding")
    add_ldflags("-target x86_64-freestanding")

    add_cflags("-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2")
    add_cflags("-mno-red-zone", "-msoft-float", "-flto")
    add_ldflags("-T src/x86_64/linker.ld", "-nostdlib", "-fuse-ld=lld")

    --add_cflags("-fsanitize=undefined")
    --add_cflags("-fsanitize=implicit-unsigned-integer-truncation")
    --add_cflags("-fsanitize=implicit-integer-sign-change")
    --add_cflags("-fsanitize=shift")
    --add_cflags("-fsanitize=implicit-integer-arithmetic-value-change")

    before_build(function (target)
        local hash = try { function()
            local result = os.iorun("git rev-parse --short HEAD")
            return result and result:trim()
        end }
        if hash then target:add("defines", "GIT_VERSION=\""..hash.."\"") end
    end)

    --add_links("ubscan")
    -- add_linkdirs("libs/x86_64")

    add_links("os_terminal")
    local build_dir = "$(buildir)/.build_cache/os-terminal"
    add_includedirs(build_dir)
    add_linkdirs(build_dir.."/x86_64-unknown-none/release/")

    add_files("src/x86_64/**.c")
    add_includedirs("libs/x86_64")
    add_includedirs("src/x86_64/include")
    add_includedirs("src/x86_64/include/types")
    add_includedirs("src/x86_64/include/iic")

target("iso32")
    set_kind("phony")
    add_deps("kernel32")
    set_default(false)

    on_build(function (target)
        import("core.project.project")

        local iso_dir = "$(buildir)/iso_dir"
        os.cp("assets/readme.txt", iso_dir .. "/readme.txt")
        local kernel = project.target("kernel32")
        os.cp(kernel:targetfile(), iso_dir .. "/cpkrnl32.elf")

        local limine_dir = iso_dir .. "/limine"
        os.cp("assets/limine.conf", limine_dir .. "/limine.conf")

        local limine_src = "thirdparty/limine"
        os.cp(limine_src.."/limine-bios.sys", limine_dir.."/limine-bios.sys")
        os.cp(limine_src.."/limine-bios-cd.bin", limine_dir.."/limine-bios-cd.bin")

        local iso_file = "$(buildir)/CoolPotOS.iso"
        local iso_flags = "-b limine/limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", iso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

target("iso64")
    set_kind("phony")
    add_deps("kernel64", "limine")
    set_default(false)

    on_build(function (target)
        import("core.project.project")

        local iso_dir = "$(buildir)/iso_dir"
        os.cp("assets/readme.txt", iso_dir .. "/readme.txt")
        local kernel = project.target("kernel64")
        os.cp(kernel:targetfile(), iso_dir .. "/cpkrnl64.elf")

        local limine_dir = iso_dir .. "/limine"
        os.cp("assets/limine.conf", limine_dir .. "/limine.conf")

        local limine_src = "thirdparty/limine"
        os.cp(limine_src.."/limine-bios.sys", limine_dir.."/limine-bios.sys")
        os.cp(limine_src.."/limine-bios-cd.bin", limine_dir.."/limine-bios-cd.bin")
        os.cp(limine_src.."/limine-uefi-cd.bin", limine_dir.."/limine-uefi-cd.bin")

        local iso_file = "$(buildir)/CoolPotOS.iso"
        os.run("xorriso -as mkisofs "..
            "-R -r -J -b limine/limine-bios-cd.bin "..
            "-no-emul-boot -boot-load-size 4 -boot-info-table "..
            "-hfsplus -apm-block-size 2048 "..
            "--efi-boot limine/limine-uefi-cd.bin "..
            "-efi-boot-part --efi-boot-image --protective-msdos-label "..
            "%s -o %s", iso_dir, iso_file)

        local limine = project.target("limine")
        os.run(limine:targetfile().." bios-install "..iso_file)
        print("ISO image created at: %s", iso_file)
    end)

target("img64")
    set_kind("phony")
    add_deps("kernel64", "limine")
    set_default(false)

    on_build(function (target)
        import("core.project.project")
        local kernel = project.target("kernel64")
        local img_file = "$(buildir)/CoolPotOS.img"

        os.run("oib -f %s:cpkrnl64.elf -f %s:limine.conf "..
            "-f %s:readme.txt -f %s:efi/boot/bootx64.efi -o %s",
            kernel:targetfile(), "assets/limine.conf",
            "assets/readme.txt", "thirdparty/limine/BOOTX64.EFI", img_file)

        print("Disk image created at: %s", img_file)
    end)

target("run32")
    set_kind("phony")
    add_deps("iso32")
    set_default(false)

    on_run(function (target)
        import("core.project.config")
        local flags = {
            "-serial", "stdio", "-m", "4096",
            "-audiodev", "pa,id=speaker", "-vga", "std",
            "-machine", "pcspk-audiodev=speaker",
            "-global", "VGA.vgamem_mb=32",
            "-net", "nic,model=pcnet", "-net", "user",
            -- "-enable-kvm",
            "-device", "sb16,audiodev=speaker",
            "-device", "intel-hda",
            "-device", "hda-micro,audiodev=speaker",
            -- "-device", "ahci,id=ahci",
            -- "-drive", "file=./disk.qcow2,if=none,id=disk0",
            -- "-device", "ide-hd,bus=ahci.0,drive=disk0",
            "-cdrom", config.buildir() .. "/CoolPotOS.iso"
        }
        os.execv("qemu-system-i386", flags)
    end)

target("run64")
    set_kind("phony")
    add_deps("iso64")
    set_default(true)

    on_run(function (target)
        import("core.project.config")
        local disk_template = "if=none,format=raw,id=disk,file="
        local flags = {
            "-M", "q35", "-cpu", "qemu64,+x2apic", "-smp", "4",
            "-serial", "stdio", "-m","1024M", "-no-reboot",
            --"-enable-kvm",
            --"-d", "in_asm",
            --"-d", "in_asm,int",
            --"-S","-s",
            --"-device","nec-usb-xhci,id=xhci",
            --"-device","usb-storage,bus=xhci.0,drive=usbdisk",
            "-audiodev", "sdl,id=audio0",
            "-device", "sb16,audiodev=audio0",
            "-net","nic,model=pcnet","-net","user",
            "-drive", "if=pflash,format=raw,file=assets/ovmf-code.fd",
            "-cdrom", config.buildir().."/CoolPotOS.iso",
            --"-device", "ahci,id=ahci",
            --"-device", "ide-hd,drive=disk,bus=ahci.0",
            --"-drive", disk_template..config.buildir().."/CoolPotOS.img",
            --"-device", "nvme,drive=disk,serial=deadbeef",
            --"-drive", disk_template..config.buildir().."/CoolPotOS.img",
        }
        os.execv("qemu-system-x86_64", flags)
    end)
