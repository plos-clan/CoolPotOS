set_project("CoolPotOS")
set_version("0.1.0")
set_policy("check.auto_ignore_flags", false)
add_rules("mode.debug", "mode.release")

option("boot")
    set_default("limine")
    set_values("limine", "other", "multiboot2")
    set_description("Boot protocol / bootloader")
option("arch")
    set_default("x86_64")
    set_values("x86_64", "aarch64", "riscv64", "loongarch64")
    set_description("OS Arch")
option_end()

if is_config("boot", "limine") then
    add_requires("limine v12.x", {system = false})
end

includes("kernel")
includes("modules")

target("os-pipeline")
    set_kind("phony")
    add_deps("iso")
    set_default(true)

    on_run(function (target)
        import("core.base.option")
        import("utils.progress")

        local arch = get_config("arch") or os.arch()
        local boot = get_config("boot")
        local iso_path = path.join(target:autogendir(), "CoolPotOS.iso")
        local hardware = ""
        local cpu = ""

        if arch == "x86_64" then
            hardware = "q35"
            cpu = "Haswell,+x2apic,+avx"
        elseif arch == "riscv64" then
            hardware = "virt"
            cpu = "rv64"
        elseif arch == "loongarch64" then
            hardware = "virt"
            cpu = "la464"
        elseif arch == "aarch64" then
            hardware = "virt"
            cpu = "cortex-a72"
        else
            raise("Unsupported arch: " .. arch)
        end

        local qemu_cmd = {}
        table.insert(qemu_cmd, "qemu-system-"..arch.."")
        table.insert(qemu_cmd, "-M")
        table.insert(qemu_cmd, hardware)
        table.insert(qemu_cmd, "-cpu")
        table.insert(qemu_cmd, cpu)
        table.insert(qemu_cmd, "-m")
        table.insert(qemu_cmd, "2G")
        table.insert(qemu_cmd, "-serial")
        table.insert(qemu_cmd, "stdio")
        table.insert(qemu_cmd, "-smp")
        table.insert(qemu_cmd, "4")

        if boot == "limine" or boot == "multiboot2" then
            table.insert(qemu_cmd, "-cdrom")
            table.insert(qemu_cmd, "$(builddir)/CoolPotOS.iso")
        end

        table.insert(qemu_cmd, "-S")
        table.insert(qemu_cmd, "-s")

        print("Running: " .. table.concat(qemu_cmd, " "))
        os.exec(table.concat(qemu_cmd, " "))
    end)

target("iso")
    set_kind("phony")
    set_default(false)
    add_deps("kernel", "modules")
    if is_config("boot", "limine") then
       add_packages("limine")
    end

    add_configfiles("assets/limine.conf.in", {
        filename = "iso_dir/limine.conf",
        variables = {
            arch = get_config("arch") or os.arch(),
            kernel = "cpkrnl_"..(get_config("arch") or os.arch())..".elf",
        }
    })

    on_build(function (target)
        import("core.project.project")
        local iso_dir = "$(builddir)/iso_dir"
        local kmod_dir = "$(builddir)/kmod"
        local arch = get_config("arch") or os.arch()

        local kernel = project.target("kernel")
        os.cp(kernel:targetfile(), iso_dir.."/cpkrnl_"..arch..".elf")

        local limine_dir = iso_dir.."/boot"
        local limine_src = target:pkg("limine"):installdir()
        local limine_src = limine_src.."/share/limine"
        os.cp(limine_src.."/limine-uefi-cd.bin", limine_dir.."/limine-uefi-cd.bin")
        os.cp(limine_src.."/limine-bios-cd.bin", limine_dir.."/limine-bios-cd.bin")
        os.cp(limine_src.."/limine-bios.sys", limine_dir.."/limine-bios.sys")

        os.run("xorriso -as mkisofs -R -r -J -b boot/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot boot/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label %s -o %s",
        iso_dir,"$(builddir)/CoolPotOS.iso")
    end)



package("limine")
    set_kind("binary")
    set_urls("https://github.com/Limine-Bootloader/Limine.git")

    on_install(function (package)
        if os.isfile("limine/bootstrap") then
            os.cd("limine")
        end

        os.vrun("./bootstrap")
        os.vrunv("./configure", {
            "--enable-bios",
            "--enable-bios-cd",
            "--enable-uefi-x86-64",
            "--enable-uefi-cd",
            "--prefix=" .. package:installdir()
        })
        import("package.tools.make").make(package)
        import("package.tools.make").make(package, {"install"})
    end)
package_end()
