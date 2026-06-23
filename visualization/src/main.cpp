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

#include "SDL3/SDL_timer.h"
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

struct DataPoint {
    std::int64_t timestamp;
    double value;
};
static int check_sqlite_result(int rc, sqlite3* db, const char* context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        fprintf(stderr, "[sqlite] %s failed: %s\n", context, db ? sqlite3_errmsg(db) : "unknown");
        return 0;
    } else {
        return 1;
    }
}
static void build_plot_vectors(
    const std::vector<DataPoint>& history, std::vector<double>& xs, std::vector<double>& ys) {
    xs.clear();
    ys.clear();

    xs.reserve(history.size());
    ys.reserve(history.size());

    if (history.empty()) {
        return;
    }

    const double base_time = static_cast<double>(history.front().timestamp) / 1000.0;

    for (const DataPoint& point : history) {
        xs.push_back(static_cast<double>(point.timestamp) / 1000.0 - base_time);
        ys.push_back(point.value);
    }
}
std::vector<DataPoint> read_variable_history(sqlite3* db, const char* variable_name) {
    const char* sql = R"sql(
        SELECT
            d.timestamp,
            d.value
        FROM data AS d
        JOIN variable AS v
            ON v.id = d.variable_id
        WHERE v.name = ?1
        ORDER BY d.timestamp ASC;
    )sql";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        check_sqlite_result(rc, db, "prepare history query");
        return {};
    }

    rc = sqlite3_bind_text(stmt, 1, variable_name, -1, SQLITE_TRANSIENT);

    if (rc != SQLITE_OK) {
        check_sqlite_result(rc, db, "bind variable name");
        sqlite3_finalize(stmt);
        return {};
    }

    std::vector<DataPoint> result;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        DataPoint point;

        point.timestamp = sqlite3_column_int64(stmt, 0);
        point.value = sqlite3_column_double(stmt, 1);

        result.push_back(point);
    }

    if (rc != SQLITE_DONE) {
        check_sqlite_result(rc, db, "read history rows");
    }

    sqlite3_finalize(stmt);
    return result;
}

static sqlite3_stmt* insert_data_stmt = nullptr;

static int init_test_data_writer(sqlite3* db) {
    const char* sql = R"sql(
        INSERT INTO data (timestamp, variable_id, value)
        VALUES (?1, ?2, ?3);
    )sql";

    int rc = sqlite3_prepare_v2(db, sql, -1, &insert_data_stmt, nullptr);
    return check_sqlite_result(rc, db, "prepare data insert");
}

static void insert_test_value(
    sqlite3* db, sqlite3_int64 timestamp_ms, int variable_id, double value) {
    sqlite3_reset(insert_data_stmt);
    sqlite3_clear_bindings(insert_data_stmt);

    int rc = sqlite3_bind_int64(insert_data_stmt, 1, timestamp_ms);
    check_sqlite_result(rc, db, "bind timestamp");

    rc = sqlite3_bind_int(insert_data_stmt, 2, variable_id);
    check_sqlite_result(rc, db, "bind variable_id");

    rc = sqlite3_bind_double(insert_data_stmt, 3, value);
    check_sqlite_result(rc, db, "bind value");

    rc = sqlite3_step(insert_data_stmt);

    if (rc != SQLITE_DONE) {
        check_sqlite_result(rc, db, "insert data");
    }
}

static void shutdown_test_data_writer() {
    if (insert_data_stmt != nullptr) {
        sqlite3_finalize(insert_data_stmt);
        insert_data_stmt = nullptr;
    }
}

// Main code
int main(int, char**) {
    printf("hello world\n");
    VulkanContext ctx;
    init_sdl3_vulkan(&ctx);

    // Our state
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImPlot::CreateContext();

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

    const char* dbName = "test.db";
    sqlite3* db = nullptr;
    int sql_rc = sqlite3_open(dbName, &db);
    if (sql_rc != SQLITE_OK) {
        log_fatal("Failed to open %s, error: %s", dbName, db ? sqlite3_errmsg(db) : "unknown");
        if (db) sqlite3_close(db);
        return 1;
    } else {
        log_info("opening test.db was successful");
    }

    if (!init_test_data_writer(db)) {
        printf("failed to initialize sqlite data writer. Exiting program\n");
        shutdown_test_data_writer();
        return 0;
    }

    std::vector<DataPoint> temperature_history;
    std::vector<DataPoint> pressure_history;
    std::vector<DataPoint> humidity_history;
    std::vector<DataPoint> voltage_history;

    std::vector<double> temperature_x;
    std::vector<double> temperature_y;
    std::vector<double> pressure_x;
    std::vector<double> pressure_y;
    std::vector<double> humidity_x;
    std::vector<double> humidity_y;
    std::vector<double> voltage_x;
    std::vector<double> voltage_y;

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
            insert_test_value(db, timestamp_ms, 1, 21.0 + std::sin(frame_counter * 0.01) * 2.0);
        }

        if (frame_counter % 120 == 0) {
            insert_test_value(
                db, timestamp_ms, 2, 101325.0 + std::sin(frame_counter * 0.005) * 100.0);
        }

        if (frame_counter % 300 == 0) {
            insert_test_value(db, timestamp_ms, 3, 50.0 + std::sin(frame_counter * 0.008) * 10.0);
        }

        if (frame_counter % 600 == 0) {
            insert_test_value(db, timestamp_ms, 4, 12.0 + std::sin(frame_counter * 0.02) * 0.2);
        }

        if (frame_counter % 30 == 0) {
            temperature_history = read_variable_history(db, "temperature");
            pressure_history = read_variable_history(db, "pressure");
            humidity_history = read_variable_history(db, "humidity");
            voltage_history = read_variable_history(db, "voltage");

            build_plot_vectors(temperature_history, temperature_x, temperature_y);
            build_plot_vectors(pressure_history, pressure_x, pressure_y);
            build_plot_vectors(humidity_history, humidity_x, humidity_y);
            build_plot_vectors(voltage_history, voltage_x, voltage_y);
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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        bool vsync = ctx.VsyncEnabled;
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("History");

        if (ImPlot::BeginPlot("Temperature", ImVec2(-1, 180))) {
            ImPlot::SetupAxes("Time (s)", "Temperature (C)");
            if (!temperature_x.empty()) {
                ImPlot::PlotLine(
                    "temperature",
                    temperature_x.data(),
                    temperature_y.data(),
                    static_cast<int>(temperature_x.size()));
            }
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Pressure", ImVec2(-1, 180))) {
            ImPlot::SetupAxes("Time (s)", "Pressure (Pa)");
            if (!pressure_x.empty()) {
                ImPlot::PlotLine(
                    "pressure",
                    pressure_x.data(),
                    pressure_y.data(),
                    static_cast<int>(pressure_x.size()));
            }
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Humidity", ImVec2(-1, 180))) {
            ImPlot::SetupAxes("Time (s)", "Humidity (%)");
            if (!humidity_x.empty()) {
                ImPlot::PlotLine(
                    "humidity",
                    humidity_x.data(),
                    humidity_y.data(),
                    static_cast<int>(humidity_x.size()));
            }
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Voltage", ImVec2(-1, 180))) {
            ImPlot::SetupAxes("Time (s)", "Voltage (V)");
            if (!voltage_x.empty()) {
                ImPlot::PlotLine(
                    "voltage",
                    voltage_x.data(),
                    voltage_y.data(),
                    static_cast<int>(voltage_x.size()));
            }
            ImPlot::EndPlot();
        }

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
