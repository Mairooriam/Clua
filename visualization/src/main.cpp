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

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <math.h>
#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // abort

#include <cstdint>
#include <iostream>

#include "SDL3/SDL_timer.h"
#include "db_access.h"
#include "db_render.h"
#include "imgui.h"
#include "imgui_SDL3_Vulkan.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "log.h"
#include "plot.h"
#include "sqlite3.h"

// This example doesn't compile with Emscripten yet! Awaiting SDL3 support.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

#include <sqlite3.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

// Main code
int main(int, char**) {
    VulkanContext ctx;
    init_sdl3_vulkan(&ctx);

    // Our state
    static bool done = false;
    uint64_t lastTime = 0;
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
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

    ImPlot::CreateContext();

    DbContext dbCtx;
    dbCtx.dbName = "data.db";
    dbCtx.dbSchemaFilename = "sql.txt";
    int res = db_connect(&dbCtx);

    if (!init_test_data_writer(dbCtx.db)) {
        printf("failed to initialize sqlite data writer. Exiting program\n");
        shutdown_test_data_writer();
        return 0;
    }

    MeasurementRecord record;
    while (!done) {
        uint64_t deltaTime = SDL_GetTicks() - lastTime;
        lastTime = SDL_GetTicks();
        static int frame_counter = 0;
        frame_counter++;
        const sqlite3_int64 timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            imguiHandleInput(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(ctx.window)) {
                done = true;
            }
        }
        if (frame_counter % 60 == 0) {
            insert_test_value(
                dbCtx.db, timestamp_ms, "test1", 21.0 + std::sin(frame_counter * 0.01) * 2.0);
        }

        if (frame_counter % 120 == 0) {
            insert_test_value(
                dbCtx.db,
                timestamp_ms,
                "test2",
                101325.0 + std::sin(frame_counter * 0.005) * 100.0);
        }

        if (frame_counter % 300 == 0) {
            insert_test_value(
                dbCtx.db, timestamp_ms, "test3", 50.0 + std::sin(frame_counter * 0.008) * 10.0);
        }

        if (frame_counter % 600 == 0) {
            insert_test_value(
                dbCtx.db, timestamp_ms, "test4", 12.0 + std::sin(frame_counter * 0.02) * 0.2);
        }

        if (frame_counter % 30 == 0) {
            int res = -1;
            // res = read_variable_history(dbCtx.db, "temperature", &record);
            // res = read_variable_history(dbCtx.db, "tatu", &record);
            // res = read_variable_history(dbCtx.db, "teemu", &record);
            // res = read_variable_history(dbCtx.db, "voltage", &record);
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
        bool vsync = ctx.VsyncEnabled;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // kysy muutujat sqlit
        // valitse muuttujat jota logataan ( jos yksikin valittu loggaa )
        static bool autoUpdate = true;
        static bool trig = false;
        static int trigThreshold = 600;
        if (frame_counter % trigThreshold == 0) {
            trig = true;
        } else {
            trig = false;
        }

        static std::vector<db_schema_variable> variables;
        if (autoUpdate && trig) {
            variables.clear();
            db_query_variable_info(dbCtx.db, variables);
            for (auto variable : variables) {
                printf("id:%zu name:%s\n", variable.id, variable.name.c_str());
            }
        }

        ImGui::Begin("History");
        static std::vector<bool> selected{0};
        static bool plotFetch = true;
        static bool plotFetchTrig = false;
        static int plotFetchPollRate = 600;
        if (selected.size() < variables.size()) {
            selected.resize(variables.size(), false);
        }
        if (variables.size() >= 1) {
            for (size_t i = 0; i < variables.size(); ++i) {
                auto& variable = variables[i];

                bool checked = selected[i];  // copy out from proxy
                std::string label = std::format("id:{}-name:{}", variable.id, variable.name);

                if (ImGui::Checkbox(label.c_str(), &checked)) {
                    selected[i] = checked;  // write back
                }
                if (checked) {
                    plotFetchTrig = true;
                }
            }
        }
        // TODO: move away from rendering for now here.
        if (frame_counter % plotFetchPollRate == 0) {
            plotFetchTrig = true;
        }
        // stuff to run more frequent. possibly combine with the other one. sicne few variables.
        // iterating is fast...
        // like max couple thousand. not actually iterating over the variable data
        for (size_t i = 0; i < variables.size(); i++) {
            auto variable = variables[i];

            if (selected[i] == false) {
                auto it = record.find(variable.name);
                if (it != record.end()) {
                    it->second.timestamp.clear();
                    it->second.value.clear();
                }
            }
        }

        // Possibly heavy stuff here only run now and then
        if (plotFetch && plotFetchTrig) {
            for (size_t i = 0; i < variables.size(); i++) {
                auto variable = variables[i];
                if (selected[i] == true) {
                    read_variable_history(dbCtx.db, variable.name.c_str(), &record);
                }
            }
            plotFetchTrig = false;
        }

        // db_render_measurementRecord(&record);
        ImGui::End();

        ImGui::Begin("plotting");
        db_render_plot_measurementRecord(&record);
        ImGui::End();

        if (ImGui::Checkbox("VSync", &vsync)) {
            ToggleVsyncSwapChain(&ctx, vsync);
        }
        ImGui::Text("%zu", deltaTime);

        ImPlot::ShowDemoWindow();
        PlotOpcUa(&pf);

        // 1. Show the big demo window (Most of the sample code is in
        // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
        // ImGui!).
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

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
