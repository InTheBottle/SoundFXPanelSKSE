#pragma once

#include "settings.h"
#include <RE/B/BGSSoundCategory.h>
#include <RE/T/TESDataHandler.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Forward declaration so SoundCategoryManager can call the bypass without
// pulling in all of hook.h here.
namespace Hooks
{
    void SetCategoryVolumeBypass(RE::BGSSoundCategory* a_cat, float a_vol);
}

// Represents one discovered sound category with cached metadata.
struct SoundCategoryEntry
{
    RE::BGSSoundCategory* form        = nullptr;
    std::string           editorID;          // GetFormEditorID()
    std::string           displayName;       // full name if available, else editorID
    float                 volume     = 1.0f; // current runtime value (0-1)
    bool                  fromMod    = false; // true if not from a vanilla master
};

class SoundCategoryManager
{
public:
    static SoundCategoryManager& GetSingleton()
    {
        static SoundCategoryManager instance;
        return instance;
    }

    // Scan all loaded BGSSoundCategory forms and build our entry list.
    // Call after kDataLoaded.
    void DiscoverCategories()
    {
        m_categories.clear();

        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::error("[SoundCategoryManager] TESDataHandler unavailable");
            return;
        }

        auto& array = handler->GetFormArray<RE::BGSSoundCategory>();
        m_categories.reserve(array.size());

        for (auto* form : array) {
            if (!form) continue;

            SoundCategoryEntry entry;
            entry.form = form;

            // Editor ID  (may be empty for anonymous records)
            const char* eid = form->GetFormEditorID();
            entry.editorID  = (eid && *eid) ? eid : std::format("SNCT_{:08X}", form->GetFormID());

            // Display name: prefer TESFullName, strip leading '$' token,
            // capitalize first letter, fall back to editorID.
            const char* full = form->GetFullName();
            if (full && *full) {
                std::string name(full);
                // Strip leading '$' (localization lookup token)
                if (!name.empty() && name[0] == '$') {
                    name.erase(0, 1);
                }
                // Capitalize first letter
                if (!name.empty()) {
                    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
                }
                entry.displayName = name;
            } else {
                entry.displayName = entry.editorID;
            }

            // Current runtime volume
            entry.volume = form->GetCategoryVolume();

            // Determine if this category comes from a non-vanilla plugin.
            // FormIDs 0x000-0x7FF are vanilla Skyrim.esm; anything with a
            // high byte > 8 very likely originates from a mod.
            const std::uint8_t highByte = static_cast<std::uint8_t>(form->GetFormID() >> 24);
            entry.fromMod = (highByte > 0x08);

            m_categories.push_back(std::move(entry));
        }

        // Sort: vanilla first, then mods; alphabetical within each group.
        std::ranges::sort(m_categories, [](const SoundCategoryEntry& a, const SoundCategoryEntry& b) {
            if (a.fromMod != b.fromMod) return !a.fromMod; // vanilla before mod
            return a.displayName < b.displayName;
        });

        spdlog::info("[SoundCategoryManager] Discovered {} sound categories ({} from mods).",
            m_categories.size(),
            std::ranges::count_if(m_categories, [](const SoundCategoryEntry& e) { return e.fromMod; }));
    }

    // Apply our saved INI values to every category we track.
    // Call after DiscoverCategories().
    void ApplySavedSettings()
    {
        auto& settings = Settings::GetSingleton();
        int applied = 0;

        for (auto& entry : m_categories) {
            if (settings.HasVolume(entry.editorID.c_str())) {
                const float saved = settings.GetVolume(entry.editorID.c_str());
                ApplyVolumeInternal(entry, saved);
                ++applied;
            }
        }
        spdlog::info("[SoundCategoryManager] Applied {} saved volume overrides.", applied);
    }

    // Set volume on a category from our panel (bypasses the block-override hook).
    // Also persists the new value into Settings (caller should call Settings::Save).
    void SetVolume(SoundCategoryEntry& entry, float volume)
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        entry.volume = volume;
        Settings::GetSingleton().SetVolume(entry.editorID.c_str(), volume);
        ApplyVolumeInternal(entry, volume);
    }

    // Remove our override for a category and let the game manage it freely.
    void ClearVolume(SoundCategoryEntry& entry)
    {
        Settings::GetSingleton().RemoveVolume(entry.editorID.c_str());
        // Re-read whatever the form currently says
        entry.volume = entry.form->GetCategoryVolume();
    }

    [[nodiscard]] std::vector<SoundCategoryEntry>& GetCategories() noexcept { return m_categories; }
    [[nodiscard]] const std::vector<SoundCategoryEntry>& GetCategories() const noexcept { return m_categories; }

private:
    SoundCategoryManager() = default;

    // Actually write the volume to the game object via the bypass so our own
    // vtable hook does not swallow the call.
    static void ApplyVolumeInternal(SoundCategoryEntry& entry, float volume)
    {
        Hooks::SetCategoryVolumeBypass(entry.form, volume);
        entry.volume = volume;
    }

    std::vector<SoundCategoryEntry> m_categories;
};
