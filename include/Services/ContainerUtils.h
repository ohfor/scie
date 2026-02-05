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

}  // namespace Services
