#pragma once

#include <RE/B/BGSSoundCategory.h>

namespace Hooks
{
    // -------------------------------------------------------
    // Called from plugin.cpp during kDataLoaded:
    //   Patches BGSSoundCategory::SetCategoryVolume vtable so the
    //   game's pause-menu audio settings cannot overwrite volumes
    //   the user has configured through this plugin.
    // -------------------------------------------------------
    void Install();

    // -------------------------------------------------------
    // Used by SoundCategoryManager to write volumes without
    // being swallowed by our own SetCategoryVolume hook.
    // -------------------------------------------------------
    void SetCategoryVolumeBypass(RE::BGSSoundCategory* a_cat, float a_vol);
}
