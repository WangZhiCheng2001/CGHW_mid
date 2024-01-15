set_arch("x64")
set_languages("c17", "c++20")

add_rules("mode.debug", "mode.release")
if(is_mode("debug")) then 
    add_defines("NDEBUG")
end

add_requires("imgui", {configs = {sdl2_no_renderer = true, vulkan = true}})
add_requires("vulkansdk")
add_requires("spdlog")
add_requires("glm")

target("rapidobj")
    set_kind("headeronly")
    add_includedirs("./3rdparty/rapidobj/include/rapidobj/", {public = true})

includes("./resources")
includes("./editor")
includes("./engine")