#include "log.h"
#include "settings.h"
#include "SoundCategoryManager.h"
#include "hook.h"
#include "MenuPanel.h"

// Called once all game forms are loaded (vanilla + all active plugins).
static void OnDataLoaded()
{
    auto& manager = SoundCategoryManager::GetSingleton();

    // 1. Discover every BGSSoundCategory form across all loaded mods.
    manager.DiscoverCategories();

    // 2. Apply any volume overrides the user previously saved.
    manager.ApplySavedSettings();

    // 3. Install the BGSSoundCategory::SetCategoryVolume vtable hook
    //    to block the game's pause-menu from overwriting our values.
    Hooks::Install();

    // 4. Register our section with SKSE Menu Framework.
    MenuPanel::Register();

    spdlog::info("[SoundFXPanel] Initialisation complete.");
}

// Called after each game load - re-apply our volumes because the engine
// reloads SkyrimPrefs.ini category values on every load.
static void OnPostLoadGame()
{
    SoundCategoryManager::GetSingleton().ApplySavedSettings();
    spdlog::info("[SoundFXPanel] Re-applied saved volumes after game load.");
}

// On a new game the engine also resets audio volumes.
static void OnNewGame()
{
    SoundCategoryManager::GetSingleton().ApplySavedSettings();
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        OnDataLoaded();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        OnPostLoadGame();
        break;
    case SKSE::MessagingInterface::kNewGame:
        OnNewGame();
        break;
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();

    spdlog::info("[SoundFXPanel] Plugin loading...");

    // Load saved INI settings immediately so they are ready before kDataLoaded.
    Settings::GetSingleton().Load();

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler)) {
        spdlog::error("[SoundFXPanel] Failed to register SKSE messaging listener.");
        return false;
    }

    spdlog::info("[SoundFXPanel] Plugin registered successfully.");
    return true;
}