set_project("CoolPotOS")

add_rules("mode.debug", "mode.release")
add_requires("zig", "nasm")

set_arch("i386")
set_languages("c23")
set_optimize("fastest")
set_warnings("all", "extra")
set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)

--add_cflags("-target i686-freestanding")
--add_arflags("-target i686-freestanding")
--add_ldflags("-target i686-freestanding")

add_cflags("-target x86-freestanding")
add_arflags("-target x86-freestanding")
add_ldflags("-target x86-freestanding")
add_cflags("-O2", "-mno-mmx", "-mno-sse", "-mno-sse2")
--add_cflags("-fsanitize=bool -fsanitize=builtin -fsanitize=bounds -fsanitize=enum -fsanitize=float-cast-overflow -fsanitize=float-divide-by-zero -fsanitize=implicit-unsigned-integer-truncation -fsanitize=implicit-integer-sign-change -fsanitize=integer-divide-by-zero -fsanitize=nonnull-attribute -fsanitize=null -fsanitize=nullability -fsanitize=pointer-overflow -fsanitize=return -fsanitize=returns-nonnull-attribute -fsanitize=shift -fsanitize=unsigned-shift-base -fsanitize=signed-integer-overflow -fsanitize=unreachable -fsanitize=unsigned-integer-overflow -fsanitize=vla-bound -fsanitize=implicit-integer-arithmetic-value-change -fsanitize=implicit-bitfield-conversion")
--add_ldflags("-fno-sanitize=undefined","-nostdlib")
--set_toolset("as", "nasm")

target("kernel")
    set_kind("binary")
    set_toolchains("@zig", "nasm")

    set_default(false)

    add_linkdirs("libs")
    add_includedirs("src/include")

    add_links("os_terminal")
    add_links("elf_parse")
    add_links("alloc-i686")
    --add_links("ubscan")
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
        local xorriso_flags = "-b sys/limine-bios-cd.bin -no-emul-boot -boot-info-table"
        os.run("xorriso -as mkisofs %s %s -o %s", xorriso_flags, iso_dir, iso_file)
        print("ISO image created at: " .. iso_file)
    end)

    on_run(function (target)
        local mis = "-serial stdio -m 4096"
        local speaker = " -audiodev pa,id=speaker -machine pcspk-audiodev=speaker "
        local ahci = "-device ahci,id=ahci -drive file=./disk.qcow2,if=none,id=disk0 -device ide-hd,bus=ahci.0,drive=disk0"
        local kvm = " -enable-kvm"
        local vga = " -vga std -global VGA.vgamem_mb=32 "
        local net = " -net nic,model=pcnet -net user "
        local audio = " -device sb16,audiodev=speaker -device intel-hda -device hda-micro,audiodev=speaker "
        local flags = mis..speaker..vga..net..kvm..audio

        os.exec("qemu-system-i386 -cdrom $(buildir)/CoolPotOS.iso %s", flags)
    end)


