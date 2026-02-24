#include "hook.h"
#include "settings.h"
#include "SoundCategoryManager.h"

#include <RE/B/BGSSoundCategory.h>
#include <RE/B/BSISoundCategory.h>
#include <RE/T/TESDataHandler.h>

// ============================================================
//  Module-level state
// ============================================================

namespace
{
    // Thread-local flag: set to true while *we* are calling SetCategoryVolume
    // so the hook knows to let our call through unchanged.
    thread_local bool g_inPluginSetVolume = false;

    // The saved vtable entry from BSISoundCategory's vtable at index 3.
    // It is a thunk that expects BSISoundCategory* (the subobject at offset
    // 0x30 in BGSSoundCategory), NOT BGSSoundCategory*.
    using SetCategoryVolume_t = void (*)(RE::BSISoundCategory*, float);
    SetCategoryVolume_t g_originalSetCategoryVolume = nullptr;

    // --------------------------------------------------------
    //  Helper: temporarily remove page protection, patch one
    //  pointer-sized slot, then restore protection.
    // --------------------------------------------------------
    void PatchVtableSlot(void** a_slot, void* a_newFn)
    {
        DWORD old{};
        VirtualProtect(a_slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        *a_slot = a_newFn;
        VirtualProtect(a_slot, sizeof(void*), old, &old);
    }
}

// ============================================================
//  Audio vtable hook  (BSISoundCategory::SetCategoryVolume)
// ============================================================

namespace
{
    // The hook replaces BSISoundCategory vtable slot 3.
    // The first parameter is the BSISoundCategory* subobject pointer.
    void SetCategoryVolumeHook(RE::BSISoundCategory* a_bsi, float a_vol)
    {
        // Down-cast to the real BGSSoundCategory (adjusts pointer by -0x30).
        auto* a_cat = static_cast<RE::BGSSoundCategory*>(a_bsi);

        // Our own calls are always allowed through.
        if (g_inPluginSetVolume) {
            g_originalSetCategoryVolume(a_bsi, a_vol);
            return;
        }

        auto& settings = Settings::GetSingleton();

        // If the user asked us to block the game's pause-menu changes, check
        // whether we have a saved value for this category.
        if (settings.blockGameOverride) {
            const char* eid = a_cat->GetFormEditorID();
            if (eid && settings.HasVolume(eid)) {
                // Silently re-apply our saved value instead.
                const float ours = settings.GetVolume(eid);
                g_inPluginSetVolume = true;
                g_originalSetCategoryVolume(a_bsi, ours);
                g_inPluginSetVolume = false;

                // Keep our in-memory entry in sync.
                for (auto& entry : SoundCategoryManager::GetSingleton().GetCategories()) {
                    if (entry.editorID == eid) {
                        entry.volume = ours;
                        break;
                    }
                }
                return;
            }
        }

        // No override - let the game write its value and update our tracking.
        g_originalSetCategoryVolume(a_bsi, a_vol);

        const char* eid = a_cat->GetFormEditorID();
        for (auto& entry : SoundCategoryManager::GetSingleton().GetCategories()) {
            if (entry.editorID == (eid ? eid : "")) {
                entry.volume = a_vol;
                break;
            }
        }
    }

    void InstallAudioHook()
    {
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::warn("[Hooks] TESDataHandler unavailable - audio vtable hook skipped.");
            return;
        }

        auto& array = handler->GetFormArray<RE::BGSSoundCategory>();
        if (array.empty()) {
            spdlog::warn("[Hooks] No BGSSoundCategory forms - audio vtable hook skipped.");
            return;
        }

        // BGSSoundCategory memory layout:
        //   0x00  TESForm           vtbl ptr
        //   0x20  TESFullName       (no separate vtbl in MSVC ABI)
        //   0x30  BSISoundCategory  vtbl ptr  <- we want this one
        //
        // BSISoundCategory vtable indices:
        //   0 = ~BSISoundCategory   1 = Matches   2 = GetCategoryVolume
        //   3 = SetCategoryVolume   4 = GetCategoryFrequency  ...

        auto* cat    = array.front();
        auto* bsiPtr = reinterpret_cast<std::uint8_t*>(cat) + 0x30;
        auto** vtbl  = *reinterpret_cast<void***>(bsiPtr);

        g_originalSetCategoryVolume =
            reinterpret_cast<SetCategoryVolume_t>(vtbl[3]);
        PatchVtableSlot(&vtbl[3], reinterpret_cast<void*>(&SetCategoryVolumeHook));

        spdlog::info("[Hooks] BGSSoundCategory::SetCategoryVolume vtable patched.");
    }
}

// ============================================================
//  Public API
// ============================================================

void Hooks::Install()
{
    InstallAudioHook();
}

void Hooks::SetCategoryVolumeBypass(RE::BGSSoundCategory* a_cat, float a_vol)
{
    if (!g_originalSetCategoryVolume) {
        // Hook not yet installed - call through the virtual dispatch normally.
        a_cat->SetCategoryVolume(a_vol);
        return;
    }
    // Convert BGSSoundCategory* -> BSISoundCategory* (adds 0x30) because
    // the saved vtable thunk expects the secondary-base subobject pointer.
    auto* bsi = static_cast<RE::BSISoundCategory*>(a_cat);
    g_inPluginSetVolume = true;
    g_originalSetCategoryVolume(bsi, a_vol);
    g_inPluginSetVolume = false;
}
