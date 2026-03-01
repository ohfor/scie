#pragma once

/// VR ESL-aware form lookup
///
/// CommonLibSSE-NG's LookupFormID on VR ignores smallFileCompileIndex, computing
/// form IDs as (compileIndex << 24 | localFormID). This works for regular plugins
/// but breaks for ESL-flagged plugins when smallFileCompileIndex > 0, because
/// VR ESL Support registers forms as (0xFE000000 | smallFileCompileIndex << 12 | localFormID).
///
/// On SE/AE this is a straight passthrough to the normal LookupForm.
/// On VR, we compute the form ID ourselves using the file's actual compile indices.

namespace FormLookup {

    template <typename T = RE::TESForm>
    T* LookupForm(RE::FormID a_localFormID, std::string_view a_pluginName)
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return nullptr;
        }

        if (!REL::Module::IsVR()) {
            return dataHandler->LookupForm<T>(a_localFormID, a_pluginName);
        }

        // VR: compute the form ID ourselves to handle ESL files correctly
        auto* file = dataHandler->LookupModByName(a_pluginName);
        if (!file || file->compileIndex == 0xFF) {
            return nullptr;
        }

        RE::FormID fullFormID;
        if (file->IsLight()) {
            // ESL file: use VR ESL Support's form ID scheme
            fullFormID = 0xFE000000u
                | (static_cast<std::uint32_t>(file->smallFileCompileIndex) << 12)
                | (a_localFormID & 0xFFF);
            logger::debug("VR ESL lookup: {} formID 0x{:X} -> 0x{:08X} (compileIndex=0x{:02X}, smallFileCompileIndex={})",
                a_pluginName, a_localFormID, fullFormID, file->compileIndex, file->smallFileCompileIndex);
        } else {
            // Regular file: standard form ID computation
            fullFormID = (static_cast<std::uint32_t>(file->compileIndex) << 24)
                | (a_localFormID & 0xFFFFFF);
        }

        auto* form = RE::TESForm::LookupByID(fullFormID);
        if (!form) {
            return nullptr;
        }

        return form->As<T>();
    }

}
