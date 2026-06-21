// Dear ImGui: standalone example application for SDL3 + Vulkan

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own
// engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with
// imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp)
// and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own
//   engine/app code.
// Read comments in imgui_impl_vulkan.h.

#include <math.h>

#include <cstdint>

#include "SDL3/SDL_timer.h"
#include "imgui.h"
#include "imgui_SDL3_Vulkan.h"
#include "implot.h"
#include "plot.h"
// Main code
int main(int, char**) {
    VulkanContext ctx;
    init_sdl3_vulkan(&ctx);
    // Our state
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    // ImGuiIO& io = ImGui::GetIO();
    ImPlot::CreateContext();
    // Main loop
    bool done = false;
    uint64_t lastTime = 0;

    PlotConfig pf;

    OpcUAEntry var1;
    var1.name = "var1";
    var1.timestamp_s = {1718971200.0, 1718971201.0, 1718971202.0, 1718971203.0};
    var1.value = {0.42, 0.55, 0.51, 0.63};

    OpcUAEntry var2;
    var2.name = "var2";
    var2.timestamp_s = {1718971200.0, 1718971201.0, 1718971202.0, 1718971203.0};
    var2.value = {0.30, 0.35, 0.50, 0.47};

    pf.data.push_back(var1);
    pf.data.push_back(var2);

    while (!done) {
        uint64_t deltaTime = SDL_GetTicks() - lastTime;
        lastTime = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imguiHandleInput(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(ctx.window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate()
        // function]
        if (SDL_GetWindowFlags(ctx.window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Resize swap chain?
        int fb_width, fb_height;
        SDL_GetWindowSize(ctx.window, &fb_width, &fb_height);
        ResizeSwapChain(&ctx, fb_width, fb_height);

        startImguiFrame();

        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        bool vsync = ctx.VsyncEnabled;
        if (ImGui::Checkbox("VSync", &vsync)) {
            ToggleVsyncSwapChain(&ctx, vsync);
        }
        ImGui::Text("%zu", deltaTime);

        ImPlot::ShowDemoWindow();
        PlotOpcUa(&pf);

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized =
            (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            ctx.wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            ctx.wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            ctx.wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            ctx.wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(&ctx, ctx.wd, draw_data);
            FramePresent(&ctx, ctx.wd);
        }
    }

    cleanup(&ctx);
    return 0;
}
