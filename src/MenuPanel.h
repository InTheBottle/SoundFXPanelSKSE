#pragma once

#include "settings.h"
#include "SoundCategoryManager.h"

#include <SKSEMenuFramework.h>

namespace MenuPanel
{
    namespace detail
    {
        // Tracks unsaved changes for the "(unsaved)" label.
        inline bool g_dirty = false;

        // The render callback invoked by the SKSE Menu Framework every frame
        // our section is visible.  Uses the ImGuiMCP:: wrappers exposed by
        // the framework header.
        inline void __stdcall RenderPanel()
        {
            auto& settings   = Settings::GetSingleton();
            auto& manager    = SoundCategoryManager::GetSingleton();
            auto& categories = manager.GetCategories();

            ImGuiMCP::TextDisabled("Adjust volumes for all sound categories (vanilla + mods).");
            ImGuiMCP::TextDisabled("Changes take effect immediately. Click 'Save to INI' to persist.");
            ImGuiMCP::Separator();

            bool prevWasMod = false;

            for (auto& entry : categories) {
                // Divider between vanilla and mod-added categories
                if (entry.fromMod && !prevWasMod) {
                    ImGuiMCP::Separator();
                    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
                        "  Mod-added categories");
                    ImGuiMCP::Separator();
                }
                prevWasMod = entry.fromMod;

                // Volume slider
                float vol = entry.volume;
                const std::string sliderLabel = entry.displayName + "##" + entry.editorID;
                ImGuiMCP::SetNextItemWidth(200.f);
                if (ImGuiMCP::SliderFloat(sliderLabel.c_str(), &vol, 0.0f, 1.0f, "%.2f")) {
                    manager.SetVolume(entry, vol);
                    g_dirty = true;
                }

                // Tooltip with technical info
                if (ImGuiMCP::IsItemHovered()) {
                    ImGuiMCP::SetTooltip("EditorID: %s\nFormID: %08X",
                        entry.editorID.c_str(), entry.form->GetFormID());
                }

                // Per-row "Reset" button
                ImGuiMCP::SameLine();
                const std::string resetID = "Reset##" + entry.editorID;
                if (ImGuiMCP::SmallButton(resetID.c_str())) {
                    // Default: 1.0 for master, 0.7 for everything else
                    const bool isMaster = (entry.editorID.find("Master") != std::string::npos ||
                                           entry.editorID.find("master") != std::string::npos ||
                                           entry.editorID.find("MASTER") != std::string::npos);
                    const float def = isMaster ? 1.0f : 0.7f;
                    manager.SetVolume(entry, def);
                    g_dirty = true;
                }
                if (ImGuiMCP::IsItemHovered()) {
                    const bool isMaster = (entry.editorID.find("Master") != std::string::npos ||
                                           entry.editorID.find("master") != std::string::npos ||
                                           entry.editorID.find("MASTER") != std::string::npos);
                    ImGuiMCP::SetTooltip("Reset to default (%.2f)",
                        isMaster ? 1.0f : 0.7f);
                }
            }

            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();

            // "Block game override" toggle
            bool block = settings.blockGameOverride;
            if (ImGuiMCP::Checkbox("Block game pause-menu from overriding", &block)) {
                settings.blockGameOverride = block;
                g_dirty = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip(
                    "When enabled, changes made in Skyrim's own audio settings\n"
                    "will be reverted back to the values set here.");
            }

            ImGuiMCP::Spacing();

            // Save button
            if (ImGuiMCP::Button("Save to INI", ImGuiMCP::ImVec2(140.f, 0.f))) {
                settings.Save();
                g_dirty = false;
                spdlog::info("[SoundFXPanel] Settings saved by user.");
            }

            ImGuiMCP::SameLine();

            // Reset-all button
            if (ImGuiMCP::Button("Reset ALL to Default", ImGuiMCP::ImVec2(170.f, 0.f))) {
                for (auto& entry : categories) {
                    const bool isMaster = (entry.editorID.find("Master") != std::string::npos ||
                                           entry.editorID.find("master") != std::string::npos ||
                                           entry.editorID.find("MASTER") != std::string::npos);
                    const float def = isMaster ? 1.0f : 0.7f;
                    manager.SetVolume(entry, def);
                }
                settings.Save();
                g_dirty = false;
            }

            if (g_dirty) {
                ImGuiMCP::SameLine();
                ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.f, 0.8f, 0.2f, 1.f), "(unsaved)");
            }
        }
    }

    // Register our section with SKSE Menu Framework.
    // Call once during kDataLoaded.
    inline void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) {
            spdlog::error("[MenuPanel] SKSE Menu Framework is not installed! "
                          "UI will not be available.");
            return;
        }

        SKSEMenuFramework::SetSection("Sound FX Panel");
        SKSEMenuFramework::AddSectionItem("Volume Controls", detail::RenderPanel);
        spdlog::info("[MenuPanel] Registered with SKSE Menu Framework.");
    }
}
