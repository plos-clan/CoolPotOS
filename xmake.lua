set_project("CoolPotOS")

add_rules("mode.debug", "mode.release")
add_requires("zig", "nasm")

set_arch("i386")
set_languages("c23")
set_optimize("fastest")
set_warnings("all", "extra")
set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

add_cflags("-target x86-freestanding")
add_arflags("-target x86-freestanding")
add_ldflags("-target x86-freestanding")
add_cflags("-mno-sse", "-mno-sse2")

target("kernel")
    set_kind("binary")
    set_toolchains("@zig", "nasm")
    set_default(false)

    add_linkdirs("libs")
    add_includedirs("src/include")

    add_links("os_terminal")
    add_links("elf_parse")
    add_files("src/**/*.asm", "src/**/*.c")

    add_asflags("-f", "elf32")
    add_ldflags("-T", "linker.ld")

target("iso")
    set_kind("phony")
    add_deps("kernel")
    set_default(true)

    on_build(function (target)
        import("core.project.project")
        local iso_dir = "$(buildir)/iso_dir"

        if os.exists(iso_dir) then os.rmdir(iso_dir) end
        os.cp("assets", iso_dir)

        local kernel = project.target("kernel")
        os.cp(kernel:targetfile(), iso_dir .. "/sys/cpkrnl.elf")

        local iso_file = "$(buildir)/CoolPotOS.iso"
        local xorriso_flags = "-b limine/limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", xorriso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

    on_run(function (target)
        local flags = "-serial stdio -m 4096"
        local audio_flags = " -audiodev pa,id=speaker -machine pcspk-audiodev=speaker "
        local ahci_flags = "-device ahci,id=ahci -drive file=./disk.qcow2,if=none,id=disk0 -device ide-hd,bus=ahci.0,drive=disk0"

        local vga = " -vga std -global VGA.vgamem_mb=32 "
        local net = " -net nic,model=pcnet -net user "
        local audio = " -device sb16,audiodev=speaker -device intel-hda -device hda-micro,audiodev=speaker "
        local fa = flags..audio_flags..vga..net..audio

        os.exec("qemu-system-i386 -cdrom $(buildir)/CoolPotOS.iso %s", fa)
    end)
