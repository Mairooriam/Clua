
#include "plot.h"

#include <cmath>
#include <limits>

#include "implot.h"
#include "implot_internal.h"

static int FindNearestIndex(const std::vector<double>& xs, double target_x) {
    if (xs.empty()) {
        return -1;
    }

    int best_index = 0;
    double best_distance = std::abs(xs[0] - target_x);

    for (size_t i = 1; i < xs.size(); ++i) {
        double distance = std::abs(xs[i] - target_x);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = (int)i;
        }
    }

    return best_index;
}

void PlotOpcUa(PlotConfig* pf) {
    // ImVec2 size(-1, ImGui::GetContentRegionAvail().y);

    // if (ImPlot::BeginPlot("OPC UA Simulated", size)) {
    if (ImPlot::BeginPlot("OPC UA Simulated")) {
        ImPlot::SetupAxes("Time", "Value");
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);

        for (size_t i = 0; i < pf->data.size(); ++i) {
            const OpcUAEntry& entry = pf->data[i];
            if (entry.value.empty() || entry.timestamp_s.empty()) {
                continue;
            }
            if (entry.value.size() != entry.timestamp_s.size()) {
                continue;
            }

            ImPlot::PlotLine(
                entry.name.c_str(),
                entry.timestamp_s.data(),
                entry.value.data(),
                (int)entry.timestamp_s.size());
        }

        if (pf->toolTip && ImPlot::IsPlotHovered()) {
            ImPlotPoint mouse = ImPlot::GetPlotMousePos();

            const OpcUAEntry* best_entry = nullptr;
            int best_index = -1;
            double best_distance = std::numeric_limits<double>::max();

            for (size_t i = 0; i < pf->data.size(); ++i) {
                const OpcUAEntry& entry = pf->data[i];
                if (entry.value.empty() || entry.timestamp_s.empty()) {
                    continue;
                }
                if (entry.value.size() != entry.timestamp_s.size()) {
                    continue;
                }

                int idx = FindNearestIndex(entry.timestamp_s, mouse.x);
                if (idx < 0) {
                    continue;
                }

                double distance = std::abs(entry.timestamp_s[idx] - mouse.x);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_entry = &entry;
                    best_index = idx;
                }
            }

            if (best_entry != nullptr && best_index >= 0) {
                ImGui::BeginTooltip();
                ImGui::Text("Series: %s", best_entry->name.c_str());
                ImGui::Text("Time:   %.3f s", best_entry->timestamp_s[best_index]);
                ImGui::Text("Value:  %.3f", best_entry->value[best_index]);
                ImGui::EndTooltip();
            }
        }

        ImPlot::EndPlot();
    }
}
