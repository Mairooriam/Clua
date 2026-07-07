#include "db_render.h"

#include <imgui.h>
#include <implot.h>

#include <cassert>
#include <cmath>
#include <limits>

#include "db_access.h"
void db_render_measurements(Measurements* meas) {
    if (meas == nullptr) {
        ImGui::TextUnformatted("No measurements (null).");
        return;
    }

    assert(
        (meas->timestamp.size() == meas->value.size()) &&
        "timestamp and value must have the same length");

    if (meas->timestamp.empty()) {
        ImGui::TextUnformatted("No measurements.");
        return;
    }

    if (ImGui::BeginTable(
            "MeasurementsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Timestamp");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < meas->timestamp.size(); ++i) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%lld", (long long)meas->timestamp[i]);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.6f", meas->value[i]);
        }

        ImGui::EndTable();
    }
}
void db_render_measurementRecord(MeasurementRecord* record) {
    if (record == nullptr) {
        ImGui::TextUnformatted("No record (null).");
        return;
    }

    if (record->empty()) {
        ImGui::TextUnformatted("No variables in record.");
        return;
    }

    for (const auto& [name, meas] : *record) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (!meas.timestamp.empty()) {
            if (ImGui::CollapsingHeader(name.c_str(), flags)) {
                ImGui::PushID(name.c_str());
                db_render_measurements(const_cast<Measurements*>(&meas));
                ImGui::PopID();
            }
        }
    }
}
static int FindNearestIndexMs(const std::vector<int64_t>& xs_ms, double target_s) {
    if (xs_ms.empty()) {
        return -1;
    }

    int best_index = 0;
    double best_distance = std::abs((double)xs_ms[0] / 1000.0 - target_s);

    for (size_t i = 1; i < xs_ms.size(); ++i) {
        double distance = std::abs((double)xs_ms[i] / 1000.0 - target_s);
        if (distance < best_distance) {
            best_distance = distance;
            best_index = (int)i;
        }
    }

    return best_index;
}

// TODO:
// make drag and drop variable to plot. Plot needs to have state that tracks whats added to it
// also need to create multiple pltos

void db_render_plot_measurementRecord(MeasurementRecord* record) {
    static bool tooltip = true;
    if (record == nullptr || record->empty()) {
        ImGui::TextUnformatted("No data to plot.");
        return;
    }

    if (ImPlot::BeginPlot("OPC UA Simulated")) {
        ImPlot::SetupAxes("Time", "Value");
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);

        for (const auto& ht : *record) {
            const auto& ts = ht.second.timestamp;
            const auto& vals = ht.second.value;

            if (ts.empty() || vals.empty() || ts.size() != vals.size()) {
                continue;
            }

            std::vector<double> ts_double(ts.size());
            for (size_t i = 0; i < ts.size(); ++i) {
                ts_double[i] = static_cast<double>(ts[i]) / 1000.0;
            }

            ImPlot::PlotLine(
                ht.first.c_str(), ts_double.data(), vals.data(), static_cast<int>(vals.size()));
        }

        if (tooltip && ImPlot::IsPlotHovered()) {
            ImPlotPoint mouse = ImPlot::GetPlotMousePos();

            const std::string* best_name = nullptr;
            const Measurements* best_meas = nullptr;
            int best_index = -1;
            double best_distance = std::numeric_limits<double>::max();

            for (const auto& [name, meas] : *record) {
                if (meas.value.empty() || meas.timestamp.empty()) {
                    continue;
                }
                if (meas.value.size() != meas.timestamp.size()) {
                    continue;
                }

                int idx = FindNearestIndexMs(meas.timestamp, mouse.x);
                if (idx < 0) {
                    continue;
                }

                double t_s = (double)meas.timestamp[idx] / 1000.0;
                double distance = std::abs(t_s - mouse.x);

                if (distance < best_distance) {
                    best_distance = distance;
                    best_name = &name;
                    best_meas = &meas;
                    best_index = idx;
                }
            }

            if (best_name != nullptr && best_meas != nullptr && best_index >= 0) {
                ImGui::BeginTooltip();
                ImGui::Text("Series: %s", best_name->c_str());
                ImGui::Text("Time:   %.3f s", (double)best_meas->timestamp[best_index] / 1000.0);
                ImGui::Text("Value:  %.6f", best_meas->value[best_index]);
                ImGui::EndTooltip();
            }
        }

        ImPlot::EndPlot();
    }
}
