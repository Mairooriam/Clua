#pragma once
#include <string>
#include <vector>
struct OpcUAEntry {
    std::string name;
    std::vector<double> value;
    std::vector<double> timestamp_s;
};

struct PlotConfig {
    std::vector<OpcUAEntry> data;
    bool toolTip = true;
};

void PlotOpcUa(PlotConfig* pf);
// void Demo_LinePlots() {
//     IMGUI_DEMO_MARKER("Plots/Line Plots");
//     static float xs1[1001], ys1[1001];
//     for (int i = 0; i < 1001; ++i) {
//         xs1[i] = i * 0.001f;
//         ys1[i] = 0.5f + 0.5f * sinf(50 * (xs1[i] + (float)ImGui::GetTime() / 10));
//     }
//     static double xs2[20], ys2[20];
//     for (int i = 0; i < 20; ++i) {
//         xs2[i] = i * 1/19.0f;
//         ys2[i] = xs2[i] * xs2[i];
//     }
//     if (ImPlot::BeginPlot("Line Plots")) {
//         ImPlot::SetupAxes("x","y");
//         ImPlot::PlotLine("f(x)", xs1, ys1, 1001);
//         ImPlot::PlotLine("g(x)", xs2, ys2, 20,{
//             ImPlotProp_Marker, ImPlotMarker_Circle,
//             ImPlotProp_Flags, ImPlotLineFlags_Segments
//         });
//         ImPlot::EndPlot();
//     }
// }

// void PlotCandlestick(const char* label_id, const double* xs, const double* opens, const double*
// closes, const double* lows, const double* highs, int count, bool tooltip, float width_percent,
// ImVec4 bullCol, ImVec4 bearCol) {
//
//     // get ImGui window DrawList
//     ImDrawList* draw_list = ImPlot::GetPlotDrawList();
//     // calc real value width
//     double half_width = count > 1 ? (xs[1] - xs[0]) * width_percent : width_percent;
//
//     // custom tool
//     if (ImPlot::IsPlotHovered() && tooltip) {
//         ImPlotPoint mouse   = ImPlot::GetPlotMousePos();
//         mouse.x             = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x),
//         ImPlotTimeUnit_Day).ToDouble(); float  tool_l       = ImPlot::PlotToPixels(mouse.x -
//         half_width * 1.5, mouse.y).x; float  tool_r       = ImPlot::PlotToPixels(mouse.x +
//         half_width * 1.5, mouse.y).x; float  tool_t       = ImPlot::GetPlotPos().y; float  tool_b
//         = tool_t + ImPlot::GetPlotSize().y; ImPlot::PushPlotClipRect();
//         draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b),
//         IM_COL32(128,128,128,64)); ImPlot::PopPlotClipRect();
//         // find mouse location index
//         int idx = BinarySearch(xs, 0, count - 1, mouse.x);
//         // render tool tip (won't be affected by plot clip rect)
//         if (idx != -1) {
//             ImGui::BeginTooltip();
//             char buff[32];
//             ImPlot::FormatDate(ImPlotTime::FromDouble(xs[idx]),buff,32,ImPlotDateFmt_DayMoYr,ImPlot::GetStyle().UseISO8601);
//             ImGui::Text("Day:   %s",  buff);
//             ImGui::Text("Open:  $%.2f", opens[idx]);
//             ImGui::Text("Close: $%.2f", closes[idx]);
//             ImGui::Text("Low:   $%.2f", lows[idx]);
//             ImGui::Text("High:  $%.2f", highs[idx]);
//             ImGui::EndTooltip();
//         }
//     }
//
//     // begin plot item
//     if (ImPlot::BeginItem(label_id)) {
//         // override legend icon color
//         ImPlot::GetCurrentItem()->Color = IM_COL32(64,64,64,255);
//         // fit data if requested
//         if (ImPlot::FitThisFrame()) {
//             for (int i = 0; i < count; ++i) {
//                 ImPlot::FitPoint(ImPlotPoint(xs[i], lows[i]));
//                 ImPlot::FitPoint(ImPlotPoint(xs[i], highs[i]));
//             }
//         }
//         // render data
//         for (int i = 0; i < count; ++i) {
//             ImVec2 open_pos  = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
//             ImVec2 close_pos = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);
//             ImVec2 low_pos   = ImPlot::PlotToPixels(xs[i], lows[i]);
//             ImVec2 high_pos  = ImPlot::PlotToPixels(xs[i], highs[i]);
//             ImU32 color      = ImGui::GetColorU32(opens[i] > closes[i] ? bearCol : bullCol);
//             draw_list->AddLine(low_pos, high_pos, color);
//             draw_list->AddRectFilled(open_pos, close_pos, color);
//         }
//
//         // end plot item
//         ImPlot::EndItem();
//     }
// }
