set_project("CoolPotOS")
add_rules("mode.debug", "mode.release")
set_languages("clatest")
set_warnings("all", "extra")
set_policy("run.autobuild", true)
set_policy("check.auto_ignore_flags", false)
add_requires("limine v9.x-binary", {system = false})

option("arch")
    set_default("x86_64")
    set_showmenu(true)
    set_values("x86_64", "i686")
    set_description("The target architecture of CoolPotOS.")
option_end()

if is_config("arch", "i686") then arch_i686()
elseif is_config("arch", "x86_64") then arch_x86_64() end

function arch_i686()
    add_requires("zig")

    target("kernel")
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
    target_end()

    target("iso")
        set_kind("phony")
        add_deps("kernel")
        add_packages("limine")
        set_default(false)

        on_build(function (target)
            import("core.project.project")

            local iso_dir = "$(buildir)/iso_dir"
            os.cp("assets/readme.txt", iso_dir.."/readme.txt")
            local kernel = project.target("kernel")
            os.cp(kernel:targetfile(), iso_dir.."/cpkrnl32.elf")

            local limine_dir = iso_dir.."/limine"
            os.cp("assets/limine.conf", limine_dir.."/limine.conf")

            local limine_src = target:pkg("limine"):installdir()
            local limine_src = limine_src.."/share/limine"
            os.cp(limine_src.."/limine-bios.sys", limine_dir.."/limine-bios.sys")
            os.cp(limine_src.."/limine-bios-cd.bin", limine_dir.."/limine-bios-cd.bin")

            local iso_file = "$(buildir)/CoolPotOS.iso"
            local iso_flags = "-b limine/limine-bios-cd.bin -no-emul-boot -boot-info-table"
            os.run("xorriso -as mkisofs %s %s -o %s", iso_flags, iso_dir, iso_file)
            print("ISO image created at: "..iso_file)
        end)
    target_end()

    target("run")
        set_kind("phony")
        add_deps("iso")
        set_default(true)

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
                "-cdrom", config.buildir().."/CoolPotOS.iso"
            }
            os.execv("qemu-system-i386", flags)
        end)
    target_end()
end

function arch_x86_64()
    add_requires("pl_readline", {debug = is_mode("debug")})
    add_requires("cp_shell")
    add_requires("os-terminal", {debug = is_mode("debug")})

    target("kernel")
        set_arch("x86_64")
        set_kind("binary")
        set_default(false)
        set_toolchains("clang")
        add_packages("os-terminal", "pl_readline")

        add_cflags("-target x86_64-freestanding")
        add_ldflags("-target x86_64-freestanding")

        add_cflags("-mno-80387", "-mno-mmx", "-mno-sse", "-mno-sse2")
        add_cflags("-mno-red-zone", "-msoft-float", "-flto")
        add_ldflags("-T src/x86_64/linker.ld", "-nostdlib", "-fuse-ld=lld")
        --add_cflags("-Wno-unused-parameter","-Wno-unused-variable","-Wno-unused-value")

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

        add_files("src/x86_64/**.c")
        add_includedirs("libs/x86_64")
        add_includedirs("src/x86_64/include")
        add_includedirs("src/x86_64/include/types")
        add_includedirs("src/x86_64/include/iic")
    target_end()

    target("iso")
        set_kind("phony")
        add_deps("kernel")
        add_packages("limine", "cp_shell")
        set_default(false)

        on_build(function (target)
            import("core.project.project")

            local iso_dir = "$(buildir)/iso_dir"
            os.cp("assets/readme.txt", iso_dir.."/readme.txt")
            local kernel = project.target("kernel")
            os.cp(kernel:targetfile(), iso_dir.."/cpkrnl64.elf")

            local limine_dir = iso_dir.."/limine"
            os.cp("assets/limine.conf", limine_dir.."/limine.conf")

            local limine_src = target:pkg("limine"):installdir()
            local limine_src = limine_src.."/share/limine"
            os.cp(limine_src.."/limine-bios.sys", limine_dir.."/limine-bios.sys")
            os.cp(limine_src.."/limine-bios-cd.bin", limine_dir.."/limine-bios-cd.bin")
            os.cp(limine_src.."/limine-uefi-cd.bin", limine_dir.."/limine-uefi-cd.bin")
            os.cp("assets/libc.so", iso_dir.."/libc.so")

            local shell = target:pkg("cp_shell")
            os.cp(shell:installdir().."/bin/shell", iso_dir.."/shell.elf")
            --os.cp("assets/shell.elf", iso_dir.."/shell.elf")

            local iso_file = "$(buildir)/CoolPotOS.iso"
            os.run("xorriso -as mkisofs "..
                "-R -r -J -b limine/limine-bios-cd.bin "..
                "-no-emul-boot -boot-load-size 4 -boot-info-table "..
                "-hfsplus -apm-block-size 2048 "..
                "--efi-boot limine/limine-uefi-cd.bin "..
                "-efi-boot-part --efi-boot-image --protective-msdos-label "..
                "%s -o %s", iso_dir, iso_file)

            os.run("limine bios-install "..iso_file)
            print("ISO image created at: %s", iso_file)
        end)
    target_end()

    target("img")
        set_kind("phony")
        add_deps("kernel")
        add_packages("limine", "cp_shell")
        set_default(false)

        on_build(function (target)
            import("core.project.project")
            local kernel = project.target("kernel")
            local img_file = "$(buildir)/CoolPotOS.img"

            local limine_src = target:pkg("limine"):installdir()
            local limine_src = limine_src.."/share/limine"

            local shell = target:pkg("cp_shell")
            local shell_bin = shell:installdir().."/bin/shell"

            os.run("oib -f %s:cpkrnl64.elf -f %s:limine.conf -f %s:shell.elf "..
                "-f %s:readme.txt -f %s:efi/boot/bootx64.efi -o %s",
                kernel:targetfile(), "assets/limine.conf", shell_bin,
                "assets/readme.txt", limine_src.."/BOOTX64.EFI", img_file)

            print("Disk image created at: %s", img_file)
        end)
    target_end()

    target("run")
        set_kind("phony")
        add_deps("iso")
        set_default(true)

        on_run(function (target)
            import("core.project.config")
            local disk_template = "if=none,format=raw,id=disk,file="
            local flags = {
                "-M", "q35", "-cpu", "Haswell,+x2apic,+avx", "-smp", "4",
                "-serial", "stdio", "-m","2048M", --"-no-reboot",
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
                -- "-cdrom", "/mnt/local/CoolPotOS.iso",
                --"-device", "ahci,id=ahci",
                --"-device", "ide-hd,drive=disk,bus=ahci.0",
                --"-drive", disk_template..config.buildir().."/CoolPotOS.img",
                --"-device", "nvme,drive=disk,serial=deadbeef",
                --"-drive", disk_template..config.buildir().."/CoolPotOS.img",
            }
            os.execv("qemu-system-x86_64", flags)
        end)
    target_end()
end

--- CoolPotOS Shell
package("cp_shell")
    set_urls("https://github.com/plos-clan/cp_shell.git")

    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)

--- Thirdparty Definitions ---
package("limine")
    set_kind("binary")
    set_urls("https://github.com/limine-bootloader/limine.git")
    
    on_install(function (package)
        local prefix = "PREFIX="..package:installdir()
        import("package.tools.make").make(package)
        import("package.tools.make").make(package, {"install", prefix})
    end)
package_end()

package("pl_readline")
    set_urls("https://github.com/plos-clan/pl_readline.git")

    on_install(function (package)
        io.writefile("xmake.lua", [[
            add_rules("mode.release", "mode.debug")
            target("pl_readline")
                set_kind("static")
                set_toolchains("clang")

                add_files("src/*.c")
                add_includedirs("include")

                add_defines("PL_ENABLE_HISTORY_FILE=0")
                add_cflags("-mno-80387", "-mno-mmx", "-DNDEBUG")
                add_cflags("-mno-sse", "-mno-sse2", "-mno-red-zone")
                add_cflags("-nostdlib", "-fno-builtin", "-fno-stack-protector")
            ]])
        import("package.tools.xmake").install(package)
        os.cp("include/*.h", package:installdir("include"))
    end)
package_end()

package("os-terminal")
    set_urls("https://github.com/plos-clan/libos-terminal.git")

    add_configs("font", {
        description = "Set which font should be embedded.",
        default = "en",
        values = {"none", "en", "en-zh"}
    })

    add_configs("arch", {
        description = "Set which architecture to build for.",
        default = "x86_64",
        values = {"x86_64", "i686"}
    })
    
    on_install(function (package)
        local font_config = package:config("font")

        if font_config == "en" then
            os.setenv("FONT_PATH", "../fonts/SourceCodePro.otf")
        elseif font_config == "en-zh" then
            os.setenv("FONT_PATH", "../fonts/FiraCodeNotoSans.ttf")
        end

        os.run(("cargo build %s %s"):format(
            package:debug() and "" or "--release",
            font_config ~= "none" and "--features embedded-font" or ""
        ))

        os.cp(("target/%s-unknown-none/%s/libos_terminal.a"):format(
            package:config("arch"),
            package:debug() and "debug" or "release"),
            package:installdir("lib"))

        os.run("cbindgen --output %s/os_terminal.h", package:installdir("include"))
    end)
package_end()
