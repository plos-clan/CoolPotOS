target("kernel")
    set_kind("binary")
    set_basename("cpkrnl")
    set_toolchains("clang")

    on_load(function (target)
        import("devel.git")

        local git_hash = "unknown"
        try {
            function ()
                git_hash = git.lastcommit() or "unknown"
            end,
            catch {
                function (err)
                    git_hash = "unknown"
                end
            }
        }

        if git_hash ~= "unknown" then
            git_hash = git_hash:sub(1, 7)
        end

        target:add("defines", "GIT_VERSION=\"" .. git_hash .. "\"")

        local boot = get_config("boot") or "limine"
        local arch = get_config("arch") or os.arch()

        target:add("files", "kernel/src/**/*.c", "kernel/src/**/*.S")
        target:add("files", "kernel/src/main.c")
        target:remove("files", "src/boot/**")
        target:remove("files", "src/arch")
        target:add("files", "src/arch/"..arch.."/*.c")
        target:add("files", "src/arch/"..arch.."/*.S")

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

    add_cflags("-ffreestanding", "-nostdlib", "-fno-builtin", "-fno-stack-protector")
    add_cflags("-mcmodel=kernel", "-fno-pie", "-fno-pic")
    add_ldflags("-nostdlib","-nostdinc", "-static")

    set_targetdir("$(builddir)/kernel")
