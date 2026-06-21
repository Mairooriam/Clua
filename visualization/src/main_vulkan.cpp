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

#include "imgui.h"
#include "imgui_SDL3_Vulkan.h"
#include "implot.h"
// Main code
int main(int, char**) {
    VulkanContext ctx;
    init_sdl3_vulkan(&ctx);
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO& io = ImGui::GetIO();
    ImPlot::CreateContext();
    // Main loop
    bool done = false;
    while (!done) {
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

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You
        // can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a
        // named window.
        {
            static float f = 0.0f;
            static int counter = 0;
            bool vsync = ctx.VsyncEnabled;
            if (ImGui::Checkbox("VSync", &vsync)) {
                ToggleVsyncSwapChain(&ctx, vsync);
            }
            ImGui::Begin(
                "Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");  // Display some text (you can use a format
                                                       // strings too)
            ImGui::Checkbox(
                "Demo Window",
                &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat(
                "float", &f, 0.0f, 1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3(
                "clear color", (float*)&clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return
                                          // true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text(
                "Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / io.Framerate,
                io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin(
                "Another Window",
                &show_another_window);  // Pass a pointer to our bool variable (the window will have
                                        // a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) show_another_window = false;
            ImGui::End();
        }

        ImPlot::ShowDemoWindow();

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
