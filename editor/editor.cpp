#include <iostream>
#include <fstream>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "applicationBase.hpp"

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::level_enum::info);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("imgui_SDL2_Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

    ApplicationBase app{};
    app.initSurface(window);
    app.initSwapchain();
    app.initImGui(window);
    app.createRenderer();

    while (!app.shouldClose())
    {
        app.processEvent(window);

        app.newFrame(window);

        app.displayGui(window);

        if (app.shouldRender())
        {
            app.updateRenderData();
            app.renderFrame();
            app.presentFrame();
        }
    }

    app.destroy();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}