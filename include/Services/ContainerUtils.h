#pragma once

#include "RE/Skyrim.h"

namespace Services {

    /// Check if a container reference has the respawn flag (unsafe for permanent storage).
    /// Checks both the reference-level flag (bit 30) and the base form flag (TESObjectCONT).
    inline bool IsContainerUnsafe(RE::TESObjectREFR* a_ref) {
        if (!a_ref) return false;

        // Check reference-level flag (bit 30)
        if (a_ref->GetFormFlags() & static_cast<std::uint32_t>(RE::TESObjectREFR::RecordFlags::kRespawns)) {
            return true;
        }

        // Check base form flag
        auto* baseObj = a_ref->GetBaseObject();
        if (baseObj && baseObj->GetFormType() == RE::FormType::Container) {
            auto* cont = static_cast<RE::TESObjectCONT*>(baseObj);
            if (cont->data.flags.any(RE::CONT_DATA::Flag::kRespawn)) {
                return true;
            }
        }

        return false;
    }

    /// Check if a container reference is non-persistent (will be evicted when its cell unloads).
    /// Uses InGameFormFlag::kRefOriginalPersistent (bit 6) which the engine sets natively on refs
    /// whose plugin record has the Persistent flag. Non-persistent refs have this bit clear.
    ///
    /// Important: Runtime IsPersistent() and formFlags & 0x400 are UNRELIABLE — the engine sets
    /// kPersistent on ALL loaded refs regardless of their plugin record flags.
    /// kRefOriginalPersistent is the only reliable runtime indicator.
    inline bool IsContainerNonPersistent(RE::TESObjectREFR* a_ref) {
        if (!a_ref) return false;
        return !a_ref->inGameFormFlags.any(RE::TESForm::InGameFormFlag::kRefOriginalPersistent);
    }

}  // namespace Services
