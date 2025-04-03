set_project("CoolPotOS")

add_rules("mode.debug", "mode.release")
add_requires("zig","nasm")

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
        local xorriso_flags = "-b sys/limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", xorriso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

target("kernel64")
    set_kind("binary")
    set_toolchains("clang")
    set_default(false)

    add_cflags("-target x86_64-freestanding")
    add_ldflags("-target x86_64-freestanding")

    add_cflags("-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2", "-msoft-float","-nostdinc")
    add_cflags("-mcmodel=kernel","-mno-red-zone")
    add_cflags("-Wno-unused-parameter")
    add_ldflags("-static","-nostdlib")

    add_files("src/x86_64/**/*.c")
    add_files("src/x86_64/**/*.S")
    add_linkdirs("libs/x86_64")
    add_includedirs("libs/x86_64")
    add_includedirs("src/x86_64/include")
    add_includedirs("src/x86_64/include/types")
    add_includedirs("src/x86_64/include/iic")
    add_ldflags("-T src/x86_64/linker.ld", "-e kmain")

    add_links("alloc")
    add_links("os_terminal")


target("iso64")
    set_kind("phony")
    add_deps("kernel64")
    set_default(false)

    on_build(function (target)
        import("core.project.project")

        local iso_dir = "$(buildir)/iso_dir"
        os.cp("assets/*", iso_dir .. "/")

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

target("default_build")
    set_kind("phony")
    add_deps("iso64")
    --add_deps("iso32")
    set_default(true)

    on_run(function (target)
        import("core.project.config")
        -- x86_64 xmake run
        local flags = {
            "-M", "q35",
            "-cpu", "qemu64,+x2apic",
            "-smp", "4",
            "-serial", "stdio",
            "-m","1024M",
            --"-no-reboot",
            --"-enable-kvm",
            -- "-d", "in_asm,int",
            -- "-d", "int",
            "-S","-s",
            --"-drive","file=./disk.qcow2,format=raw,id=usbdisk,if=none",
            --"-device","nec-usb-xhci,id=xhci",
            --"-device","usb-storage,bus=xhci.0,drive=usbdisk",
            --"-device","ahci,id=ahci","-drive","file=./disk.qcow2,if=none,id=disk0","-device","ide-    bus=ahci.0,drive=disk0",
            --"-drive","file=nvme.raw,if=none,id=D22","-device","nvme,drive=D22,serial=1234",
            --"-audiodev","pa,id=snd","-machine","pcspk-audiodev=snd",
            "-net","nic,model=pcnet","-net","user",
            "-drive", "if=pflash,format=raw,file=assets/ovmf-code.fd",
            "-cdrom", config.buildir() .. "/CoolPotOS.iso",
        }
        os.execv("qemu-system-x86_64 " , flags)
       -- os.execv("echo " , flags)

        -- i386 xmake run
        -- local misc = "-serial stdio -m 4096"
        -- local speaker = " -audiodev pa,id=speaker -machine pcspk-audiodev=speaker "
        -- local ahci = "-device ahci,id=ahci -drive file=./disk.qcow2,if=none,id=disk0 -device ide-hd,bus=ahci.0,drive=disk0"
        -- local kvm = " -enable-kvm"
        -- local vga = " -vga std -global VGA.vgamem_mb=32 "
        -- local net = " -net nic,model=pcnet -net user "
        -- local audio = " -device sb16,audiodev=speaker -device intel-hda -device hda-micro,audiodev=speaker "
        -- local flags = misc..speaker..vga..net..kvm..audio

        -- os.exec("qemu-system-i386 -cdrom $(buildir)/CoolPotOS.iso %s", flags)
    end)
