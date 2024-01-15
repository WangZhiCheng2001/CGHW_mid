target("ShaderBuilder")
    set_kind("object")
    add_rules("utils.glsl2spv", {outputdir = "resources/shaders/compiled"})
    add_files("./shaders/*.vert", "./shaders/*.frag", "./shaders/*.comp")
target_end()

target("BuiltinResources")
    set_kind("static")
    -- just to make it can be compiled as a library
    -- so that external projects can get paths
    add_files("./empty.cpp")
    add_includedirs("./shaders/compiled", {public = true})
    add_includedirs("./models", {public = true})
    add_deps("ShaderBuilder")

    after_build(function (target)
        if os.exists(target:targetdir() .. "/resources") then 
            os.rmdir(target:targetdir() .. "/resources")
        end
        os.cp("$(projectdir)/resources", target:targetdir() .. "/resources")
    end)
target_end()