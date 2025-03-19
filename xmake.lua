set_project("CoolPotOS")

add_rules("mode.debug", "mode.release")
add_requires("zig")

set_optimize("fastest")
set_languages("c23")
--set_warnings("all", "extra", "pedantic", "error")

set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

add_cflags("-target x86_64-freestanding")
add_ldflags("-target x86_64-freestanding")

add_cflags("-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2", "-msoft-float","-nostdinc")
add_cflags("-mcmodel=kernel","-mno-red-zone")
add_ldflags("-static","-nostdlib")

target("kernel")
    set_kind("binary")
    set_toolchains("clang")
    set_default(false)

    add_files("src/**/*.c")
    add_files("src/**/*.S")
    add_linkdirs("libs")
    add_includedirs("libs")
    add_includedirs("src/include")
    add_includedirs("src/include/types")
    add_includedirs("src/include/iic")
    add_ldflags("-T src/linker.ld", "-e kmain")

    add_links("alloc")
    add_links("os_terminal")


target("iso")
    set_kind("phony")
    add_deps("kernel")
    set_default(true)

    on_build(function (target)
        import("core.project.project")

        local iso_dir = "$(buildir)/iso_dir"
        os.cp("assets/*", iso_dir .. "/")

        local target = project.target("kernel")
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

    on_run(function (target)
        import("core.project.config")

        local flags = {
            "-M", "q35",
            "-cpu", "qemu64,+x2apic",
            "-smp", "4",
            "-serial", "stdio",
            "-m","1024M",
            --"-no-reboot",
            --"-enable-kvm",
            --"-d", "in_asm,int",
            --"-S","-s",
            --"-drive","file=./disk.qcow2,format=raw,id=usbdisk,if=none",
            --"-device","nec-usb-xhci,id=xhci",
            --"-device","usb-storage,bus=xhci.0,drive=usbdisk",
            --"-device","ahci,id=ahci","-drive","file=./disk.qcow2,if=none,id=disk0","-device","ide-hd,bus=ahci.0,drive=disk0",
            --"-drive","file=nvme.raw,if=none,id=D22","-device","nvme,drive=D22,serial=1234",
            --"-audiodev","pa,id=snd","-machine","pcspk-audiodev=snd",
            "-drive", "if=pflash,format=raw,file=assets/ovmf-code.fd",
            "-cdrom", config.buildir() .. "/CoolPotOS.iso",
            "-hda", "hda.img",
        }
        
        os.execv("qemu-system-x86_64 " , flags)
    end)
