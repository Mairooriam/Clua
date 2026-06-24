#include "db_render.h"

#include <imgui.h>

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
        if (ImGui::CollapsingHeader(name.c_str(), flags)) {
            ImGui::PushID(name.c_str());
            db_render_measurements(const_cast<Measurements*>(&meas));
            ImGui::PopID();
        }
    }
}
