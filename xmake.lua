set_project("CoolPotOS")

add_rules("mode.debug", "mode.release")
add_requires("zig", "nasm")

set_languages("c23")
set_optimize("fastest")
set_warnings("all", "extra")
set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

target("kernel")
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
        local xorriso_flags = "-b sys/limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", xorriso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

    on_run(function (target)
        local misc = "-serial stdio -m 4096"
        local speaker = " -audiodev pa,id=speaker -machine pcspk-audiodev=speaker "
        local ahci = "-device ahci,id=ahci -drive file=./disk.qcow2,if=none,id=disk0 -device ide-hd,bus=ahci.0,drive=disk0"
        local kvm = " -enable-kvm"
        local vga = " -vga std -global VGA.vgamem_mb=32 "
        local net = " -net nic,model=pcnet -net user "
        local audio = " -device sb16,audiodev=speaker -device intel-hda -device hda-micro,audiodev=speaker "
        local flags = misc..speaker..vga..net..kvm..audio

        os.exec("qemu-system-i386 -cdrom $(buildir)/CoolPotOS.iso %s", flags)
    end)


