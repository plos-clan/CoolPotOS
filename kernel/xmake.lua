target("kernel")
    set_kind("binary")
    set_basename("cpkrnl")
    set_toolchains("clang")

    on_load(function (target)
        local boot = get_config("boot") or "limine"
        local arch = get_config("arch") or os.arch()

        target:add("files", "kernel/src/**/*.c", "kernel/src/**/*.S", { excludes = "src/boot/**" })

        if boot == "limine" then
            target:add("files", "kernel/src/boot/limine/*.c")
            target:add("defines", "CONFIG_BOOT_LIMINE")
        elseif boot == "multiboot2" then
            target:add("files", "src/boot/multiboot2/*.c", "src/boot/multiboot2/*.S")
            target:add("defines", "CONFIG_BOOT_MULTIBOOT2")
        elseif boot == "other" then
            target:add("files", "src/boot/other/*.c", "src/boot/other/*.S")
            target:add("defines", "CONFIG_BOOT_OTHER")
        end

        target:add("includedirs",
            "kernel/src/arch/" .. arch .. "/include",
            "kernel/src/include",
            "kernel/src/boot"
        )

        local linker = "kernel/linker/" .. boot .. "-" .. arch .. ".ld"
        target:add("ldflags", "-T " .. linker, {force = true})
    end)

    add_cflags("-ffreestanding", "-nostdlib", "-fno-builtin",
               "-fno-stack-protector", "-fno-PIC")
    add_ldflags("-nostdlib","-nostdinc", "-static")

    set_targetdir("$(builddir)/kernel")
