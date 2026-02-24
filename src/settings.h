#pragma once

#include <SimpleIni.h>
#include <unordered_map>
#include <string>
#include <filesystem>

class Settings
{
public:
    static Settings& GetSingleton()
    {
        static Settings instance;
        return instance;
    }

    // Block game audio menu from overriding our saved values
    bool blockGameOverride = true;

    void Load()
    {
        CSimpleIniA ini;
        ini.SetUnicode();
        const auto path = GetINIPath();
        if (ini.LoadFile(path.c_str()) < 0) {
            spdlog::info("[Settings] No existing INI found at '{}', using defaults.", path);
            return;
        }

        blockGameOverride = ini.GetBoolValue("General", "BlockGameOverride", true);

        // Load per-category volumes: [SoundCategories] editorID = 0.0 - 1.0
        CSimpleIniA::TNamesDepend keys;
        if (ini.GetAllKeys("SoundCategories", keys)) {
            for (const auto& key : keys) {
                const char* raw = ini.GetValue("SoundCategories", key.pItem, nullptr);
                if (raw) {
                    try {
                        float vol = std::stof(raw);
                        m_volumes[key.pItem] = std::clamp(vol, 0.0f, 1.0f);
                    } catch (...) {}
                }
            }
        }
        spdlog::info("[Settings] Loaded {} category volume overrides.", m_volumes.size());
    }

    void Save()
    {
        CSimpleIniA ini;
        ini.SetUnicode();
        const auto path = GetINIPath();
        ini.LoadFile(path.c_str()); // load existing to preserve unknown keys

        ini.SetBoolValue("General", "BlockGameOverride", blockGameOverride,
            "; If true, prevent the game's pause menu from resetting volumes we manage");

        for (const auto& [id, vol] : m_volumes) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.4f", vol);
            ini.SetValue("SoundCategories", id.c_str(), buf);
        }

        // Make sure the directory exists
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        if (ini.SaveFile(path.c_str()) < 0) {
            spdlog::error("[Settings] Failed to save INI to '{}'", path);
        } else {
            spdlog::info("[Settings] Saved {} category overrides.", m_volumes.size());
        }
    }

    [[nodiscard]] bool HasVolume(const char* editorID) const
    {
        if (!editorID || *editorID == '\0') return false;
        return m_volumes.contains(editorID);
    }

    [[nodiscard]] float GetVolume(const char* editorID) const
    {
        auto it = m_volumes.find(editorID);
        return (it != m_volumes.end()) ? it->second : 1.0f;
    }

    void SetVolume(const char* editorID, float volume)
    {
        if (!editorID || *editorID == '\0') return;
        m_volumes[editorID] = std::clamp(volume, 0.0f, 1.0f);
    }

    void RemoveVolume(const char* editorID)
    {
        if (editorID) m_volumes.erase(editorID);
    }

private:
    Settings() = default;

    std::unordered_map<std::string, float> m_volumes;

    [[nodiscard]] static std::string GetINIPath()
    {
        return "Data/SKSE/Plugins/SoundFXPanel.ini";
    }
};
