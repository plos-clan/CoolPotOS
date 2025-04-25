set_project("CoolPotOS")
add_rules("mode.debug", "mode.release")
set_optimize("fastest")
set_languages("c23")
set_warnings("all", "extra")

set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

target("kernel32")
    set_arch("i386")
    set_kind("binary")
    set_toolchains("@zig", "nasm")
    set_toolset("as", "nasm")

    add_cflags("-target x86-freestanding")
    add_arflags("-target x86-freestanding")
    add_ldflags("-target x86-freestanding")
    add_cflags("-mno-mmx", "-mno-sse", "-mno-sse2")

    set_default(false)

    add_linkdirs("libs/i386")
    add_includedirs("src/i386/include")
    add_files("src/i386/**/*.asm", "src/i386/**/*.c")

    add_links("os_terminal")
    add_links("elf_parse")
    add_links("alloc")

    add_asflags("-f", "elf32")
    add_ldflags("-T", "src/i386/linker.ld")

target("iso32")
    set_kind("phony")
    add_deps("kernel32")
    set_default(false)

    on_build(function (target)
        import("core.project.project")
        local iso_dir = "$(buildir)/iso_dir"

        if os.exists(iso_dir) then os.rmdir(iso_dir) end
        os.cp("assets", iso_dir)

        local kernel = project.target("kernel32")
        os.cp(kernel:targetfile(), iso_dir .. "/sys/cpkrnl.elf")

        local iso_file = "$(buildir)/CoolPotOS.iso"
        local xorriso_flags = "-b limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", xorriso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

target("kernel64")
    set_kind("binary")
    set_toolchains("clang")
    set_default(false)

    add_cflags("-target x86_64-freestanding")
    add_ldflags("-target x86_64-freestanding")

    add_cflags("-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2", "-msoft-float")
    add_cflags("-mcmodel=kernel", "-mno-red-zone", "-nostdinc", "-flto", "-fpie")
    add_cflags("-Wno-unused-parameter","-Wno-unused-function")
    add_ldflags("-nostdlib", "-flto", "-fuse-ld=lld", "-static")

    add_cflags("-fsanitize=undefined")
    --add_cflags("-fsanitize=implicit-unsigned-integer-truncation")
    --add_cflags("-fsanitize=implicit-integer-sign-change")
    --add_cflags("-fsanitize=shift")
    --add_cflags("-fsanitize=implicit-integer-arithmetic-value-change")
    add_ldflags("-Wl,--defsym=_real_malloc=malloc")
    add_ldflags("-Wl,--defsym=_real_free=free")
    add_ldflags("-Wl,--defsym=_real_realloc=realloc")
    add_cflags("-Dmalloc=_fuck_malloc", "-Dfree=_fuck_free", "-Drealloc=_fuck_realloc")

    add_files("src/x86_64/**/*.c")
    add_files("src/x86_64/**/*.S")
    add_files("src/x86_64/fuck-alloc.c")

    before_build(function (target)
      local hash = try { function() return os.iorun("git rev-parse --short HEAD") end }
        if hash then
          hash = hash:trim()
          target:add("defines", "GIT_VERSION=\"" .. hash .. "\"")
        end
    end)

    add_linkdirs("libs/x86_64")
    add_links("alloc")
    add_links("os_terminal")
    add_links("ubscan")

    add_includedirs("libs/x86_64")
    add_includedirs("src/x86_64/include")
    add_includedirs("src/x86_64/include/types")
    add_includedirs("src/x86_64/include/iic")
    add_ldflags("-T src/x86_64/linker.ld")

target("iso64")
    set_kind("phony")
    add_deps("kernel64")
    set_default(false)

    on_build(function (target)
        import("core.project.project")

        local iso_dir = "$(buildir)/iso_dir"
        os.cp("assets/sys", iso_dir .. "/sys")
        os.cp("assets/limine.conf", iso_dir .. "/limine.conf")
        os.cp("assets/limine-uefi-cd.bin", iso_dir .. "/limine-uefi-cd.bin")

        local target = project.target("kernel64")
        os.cp(target:targetfile(), iso_dir .. "/sys/cpkrnl.elf")

        local iso_file = "$(buildir)/CoolPotOS.iso"
        os.run("xorriso -as mkisofs -efi-boot-part --efi-boot-image --protective-msdos-label " ..
        "-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus "..
        "-R -r -J -apm-block-size 2048 "..
        "-exclude ovmf-code.fd "..
        "--efi-boot limine-uefi-cd.bin "..
        "%s -o %s", iso_dir, iso_file)
        print("ISO image created at: %s", iso_file)
    end)

target("run32")
    set_kind("phony")
    add_deps("iso32")
    set_default(false)

    on_run(function (target)
        import("core.project.config")
        local flags = {
            "-serial", "stdio",
            "-m", "4096",
            "-audiodev", "pa,id=speaker",
            "-machine", "pcspk-audiodev=speaker",
            "-vga", "std",
            "-global", "VGA.vgamem_mb=32",
            "-net", "nic,model=pcnet",
            "-net", "user",
            "-enable-kvm",
            "-device", "sb16,audiodev=speaker",
            "-device", "intel-hda",
            "-device", "hda-micro,audiodev=speaker",
            "-device", "ahci,id=ahci",
            "-drive", "file=./disk.qcow2,if=none,id=disk0",
            "-device", "ide-hd,bus=ahci.0,drive=disk0",
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
        local flags = {
            "-M", "q35",
            "-cpu", "qemu64,+x2apic",
            "-smp", "4",
            "-serial", "stdio",
            "-m","1024M",
            "-no-reboot",
            --"-enable-kvm",
            --"-d", "int",
            --"-d", "in_asm,int",
            --"-S","-s",
            --"-drive","file=./disk.qcow2,format=raw,id=usbdisk,if=none",
            --"-device","nec-usb-xhci,id=xhci",
            --"-device","usb-storage,bus=xhci.0,drive=usbdisk",
            --"-device","ahci,id=ahci","-drive","file=./hda.img,if=none,id=disk0","-device","ide-hd,bus=ahci.0,drive=disk0",
            --"-drive","file=nvme.raw,if=none,id=D22","-device","nvme,drive=D22,serial=1234",
            --"-audiodev","pa,id=snd","-machine","pcspk-audiodev=snd",
            "-net","nic,model=pcnet","-net","user",
            "-drive", "if=pflash,format=raw,file=assets/ovmf-code.fd",
            "-cdrom", config.buildir() .. "/CoolPotOS.iso",
            "-audiodev", "sdl,id=audio0", "-device", "sb16,audiodev=audio0",
        }
        os.execv("qemu-system-x86_64 " , flags)
    end)
