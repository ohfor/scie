#include "Services/ContainerManager.h"
#include "Services/ContainerRegistry.h"
#include "Services/INISettings.h"
#include <chrono>

namespace Services {
    ContainerManager* ContainerManager::GetSingleton() {
        static ContainerManager singleton;
        return &singleton;
    }

    void ContainerManager::Initialize() {
        logger::info("Initializing ContainerManager...");

        // Use TESDataHandler::LookupForm for all form lookups — LookupByEditorID
        // doesn't work on VR (the EditorID map relocation has no VR offset)
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::error("TESDataHandler not available - ContainerManager cannot initialize");
            return;
        }

        // CraftingSource_Enabled keyword from our ESP
        m_craftingSourceKeyword = dataHandler->LookupForm<RE::BGSKeyword>(0x800, "CraftingInventoryExtender.esp");
        if (m_craftingSourceKeyword) {
            logger::info("Found CraftingSource_Enabled keyword");
        } else {
            logger::warn("CraftingSource_Enabled keyword not found - player marking will not work");
        }

        // Vanilla factions for horse/follower/spouse support
        m_playerHorseFaction = dataHandler->LookupForm<RE::TESFaction>(0x68D78, "Skyrim.esm");
        if (m_playerHorseFaction) {
            logger::info("Found PlayerHorseFaction ({:08X}) - horse saddlebag support enabled",
                m_playerHorseFaction->GetFormID());
        } else {
            logger::warn("PlayerHorseFaction not found - horse saddlebag support disabled");
        }

        m_currentFollowerFaction = dataHandler->LookupForm<RE::TESFaction>(0x5C84E, "Skyrim.esm");
        if (m_currentFollowerFaction) {
            logger::info("Found CurrentFollowerFaction ({:08X}) - follower inventory support enabled",
                m_currentFollowerFaction->GetFormID());
        } else {
            logger::warn("CurrentFollowerFaction not found - follower inventory support disabled");
        }

        m_playerMarriedFaction = dataHandler->LookupForm<RE::TESFaction>(0xC6472, "Skyrim.esm");
        if (m_playerMarriedFaction) {
            logger::info("Found PlayerMarriedFaction ({:08X}) - spouse inventory support enabled",
                m_playerMarriedFaction->GetFormID());
        } else {
            logger::warn("PlayerMarriedFaction not found - spouse inventory support disabled");
        }

        m_craftingCookpotKeyword = dataHandler->LookupForm<RE::BGSKeyword>(0xA5CB3, "Skyrim.esm");
        if (m_craftingCookpotKeyword) {
            logger::info("Found CraftingCookpot keyword ({:08X})",
                m_craftingCookpotKeyword->GetFormID());
        } else {
            logger::warn("CraftingCookpot keyword not found - cooking station detection disabled");
        }

        // Check for Convenient Horses compatibility
        if (dataHandler && dataHandler->LookupModByName("Convenient Horses.esp")) {
            m_chHorseFaction = dataHandler->LookupForm<RE::TESFaction>(0x73D1, "Convenient Horses.esp");
            if (m_chHorseFaction) {
                logger::info("Found Convenient Horses - CHHorseFaction ({:08X}) enabled",
                    m_chHorseFaction->GetFormID());
            } else {
                logger::warn("Convenient Horses.esp loaded but CHHorseFaction not found");
            }
        }

        // Check for Nether's Follower Framework compatibility
        if (dataHandler && dataHandler->LookupModByName("nwsFollowerFramework.esp")) {
            m_nffXStoreFaction = dataHandler->LookupForm<RE::TESFaction>(0x42702A, "nwsFollowerFramework.esp");
            m_nffXStorageQuest = dataHandler->LookupForm<RE::TESQuest>(0x4220F4, "nwsFollowerFramework.esp");
            if (m_nffXStoreFaction && m_nffXStorageQuest) {
                logger::info("Found Nether's Follower Framework - nwsFF_xStoreFac ({:08X}), nwsFollowerXStorage ({:08X})",
                    m_nffXStoreFaction->GetFormID(), m_nffXStorageQuest->GetFormID());
            } else {
                logger::warn("nwsFollowerFramework.esp loaded but NFF forms not found (faction={}, quest={})",
                    m_nffXStoreFaction != nullptr, m_nffXStorageQuest != nullptr);
                m_nffXStoreFaction = nullptr;
                m_nffXStorageQuest = nullptr;
            }
        }

        logger::info("ContainerManager initialized");
    }

    std::vector<RE::TESObjectREFR*> ContainerManager::GetNearbyCraftingContainers(float a_maxDistance) {
        std::lock_guard lock(m_cacheMutex);

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {};

        auto* currentCell = player->GetParentCell();
        if (!currentCell) return {};

        // Refresh cache if cell changed
        bool didRescan = false;
        if (currentCell != m_cachedCell) {
            m_cachedCell = currentCell;
            ScanCellForContainers();
            didRescan = true;
        }

        auto filterStart = std::chrono::high_resolution_clock::now();

        // Filter by distance if specified
        float maxDist = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();
        auto playerPos = player->GetPosition();

        std::vector<RE::TESObjectREFR*> result;
        for (auto* container : m_cachedContainers) {
            if (!container || container->IsDeleted() || container->IsDisabled()) continue;

            float distance = playerPos.GetDistance(container->GetPosition());
            if (distance <= maxDist) {
                result.push_back(container);
            }
        }

        // Sort by distance
        std::ranges::sort(result, [&playerPos](RE::TESObjectREFR* a, RE::TESObjectREFR* b) {
            return playerPos.GetDistance(a->GetPosition()) < playerPos.GetDistance(b->GetPosition());
        });

        auto filterEnd = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] GetNearbyCraftingContainers: filter/sort took {:.2f}ms (rescan={}, cached={}, result={})",
            std::chrono::duration<double, std::milli>(filterEnd - filterStart).count(),
            didRescan, m_cachedContainers.size(), result.size());

        return result;
    }

    bool ContainerManager::IsPlayerOwnedMount(RE::TESObjectREFR* a_ref) {
        // Need at least one horse faction to check
        if (!a_ref || (!m_playerHorseFaction && !m_chHorseFaction)) return false;

        // Must be an actor
        auto* actor = a_ref->As<RE::Actor>();
        if (!actor) return false;

        // Check mount status and faction for debugging
        bool isMount = actor->IsAMount();
        bool inVanillaFaction = m_playerHorseFaction && actor->IsInFaction(m_playerHorseFaction);
        bool inCHFaction = m_chHorseFaction && actor->IsInFaction(m_chHorseFaction);
        bool inFaction = inVanillaFaction || inCHFaction;
        bool isDead = actor->IsDead();

        // Log any actor that's a mount (for debugging)
        if (isMount) {
            logger::trace("Mount check: {:08X} '{}' - IsAMount={}, InPlayerHorseFaction={}, InCHHorseFaction={}, IsDead={}",
                a_ref->GetFormID(),
                a_ref->GetDisplayFullName() ? a_ref->GetDisplayFullName() : "unnamed",
                isMount, inVanillaFaction, inCHFaction, isDead);
        }

        if (!isMount) return false;
        if (!inFaction) return false;
        if (isDead) return false;

        return true;
    }

    bool ContainerManager::IsValidCraftingSource(RE::TESObjectREFR* a_container) {
        if (!a_container) return false;

        // Check for player-owned mount (horse saddlebag support)
        if (IsPlayerOwnedMount(a_container)) {
            // Player manually disabled this mount - overrides everything
            if (ContainerRegistry::GetSingleton()->IsManuallyDisabled(a_container->GetFormID())) {
                return false;
            }

            // Check if explicitly enabled or in INI
            if (ContainerRegistry::GetSingleton()->ShouldPullFrom(a_container)) {
                return true;
            }

            // By default, player-owned mounts are NOT auto-enabled
            // User must toggle them on with the lesser power (for immersion)
            return false;
        }

        // Check if it's actually a container
        auto* baseObj = a_container->GetBaseObject();
        if (!baseObj || baseObj->GetFormType() != RE::FormType::Container) {
            return false;
        }

        // Skip locked containers
        if (a_container->IsLocked()) {
            return false;
        }

        // Check if owned by someone else (would be stealing)
        if (IsOwnedByOther(a_container)) {
            return false;
        }

        // Check if it's a forbidden container type
        if (IsForbiddenContainer(a_container)) {
            return false;
        }

        // Player manually disabled this container - overrides everything
        if (ContainerRegistry::GetSingleton()->IsManuallyDisabled(a_container->GetFormID())) {
            return false;
        }

        // Check ContainerRegistry (handles INI + player enabled)
        if (ContainerRegistry::GetSingleton()->ShouldPullFrom(a_container)) {
            return true;
        }

        // Legacy: Check if it has the crafting source keyword (base form)
        if (m_craftingSourceKeyword && a_container->HasKeyword(m_craftingSourceKeyword)) {
            return true;
        }

        return false;
    }

    void ContainerManager::RefreshContainerCache() {
        std::lock_guard lock(m_cacheMutex);
        m_cachedCell = nullptr;
        m_cachedContainers.clear();
    }

    void ContainerManager::ScanCellForContainers() {
        m_cachedContainers.clear();

        if (!m_cachedCell) return;

        logger::debug("Scanning cell for containers: {}", m_cachedCell->GetFormEditorID());

        auto scanStart = std::chrono::high_resolution_clock::now();
        int totalRefs = 0;
        int containerRefs = 0;

        int mountRefs = 0;
        m_cachedCell->ForEachReference([this, &totalRefs, &containerRefs, &mountRefs](RE::TESObjectREFR& ref) {
            totalRefs++;

            // Check if it's a container
            auto* baseObj = ref.GetBaseObject();
            if (baseObj && baseObj->GetFormType() == RE::FormType::Container) {
                containerRefs++;
                // Log all containers we find
                bool hasKeyword = m_craftingSourceKeyword && ref.HasKeyword(m_craftingSourceKeyword);
                logger::trace("Container {:08X} '{}' - hasKeyword={}",
                    ref.GetFormID(),
                    baseObj->GetName() ? baseObj->GetName() : "unnamed",
                    hasKeyword);
            }

            // Check if it's a player-owned mount (horse saddlebag)
            if (IsPlayerOwnedMount(&ref)) {
                mountRefs++;
                logger::trace("Player-owned mount {:08X} '{}' found in cell",
                    ref.GetFormID(),
                    ref.GetDisplayFullName() ? ref.GetDisplayFullName() : "unnamed");
            }

            if (IsValidCraftingSource(&ref)) {
                m_cachedContainers.push_back(&ref);
                logger::trace("Found valid crafting source: {:08X}", ref.GetFormID());
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });

        auto scanEnd = std::chrono::high_resolution_clock::now();
        logger::debug("[PERF] Cell scan: {:.2f}ms - {} total refs, {} containers, {} mounts, {} valid sources",
            std::chrono::duration<double, std::milli>(scanEnd - scanStart).count(),
            totalRefs, containerRefs, mountRefs, m_cachedContainers.size());
    }

    bool ContainerManager::IsOwnedByOther(RE::TESObjectREFR* a_container) {
        if (!a_container) return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        // Check ownership
        auto* owner = a_container->GetOwner();
        if (!owner) return false;  // No owner = not owned

        // If owned by player, it's fine
        if (owner == player->GetActorBase()) {
            return false;
        }

        // If owned by a faction the player is in, it's fine
        if (auto* faction = owner->As<RE::TESFaction>()) {
            if (player->IsInFaction(faction)) {
                return false;
            }
        }

        // Owned by someone else
        return true;
    }

    bool ContainerManager::IsForbiddenContainer(RE::TESObjectREFR* a_container) {
        if (!a_container) return false;

        // TODO: Check for forbidden container types:
        // - Merchant containers (has vendor faction)
        // - Jail evidence containers
        // - Stolen goods containers
        // These typically have specific keywords or are in specific factions

        return false;
    }

    void ContainerManager::RegisterContainer(RE::TESObjectREFR* a_container) {
        if (!a_container) return;

        auto formID = a_container->GetFormID();
        auto* registry = ContainerRegistry::GetSingleton();

        // Enable if not already active
        if (!registry->ShouldPullFrom(formID)) {
            registry->ToggleCycleState(formID, a_container);
        }

        logger::info("Registered container {:08X} '{}'", formID,
            a_container->GetBaseObject() ? a_container->GetBaseObject()->GetName() : "unnamed");

        // Invalidate cache so it gets refreshed
        RefreshContainerCache();
    }

    void ContainerManager::UnregisterContainer(RE::TESObjectREFR* a_container) {
        if (!a_container) return;

        auto formID = a_container->GetFormID();
        auto* registry = ContainerRegistry::GetSingleton();

        // Disable if currently active - set directly to off
        if (registry->ShouldPullFrom(formID)) {
            registry->SetContainerState(formID, Services::ContainerState::kOff);
        }

        logger::info("Unregistered container {:08X}", formID);

        // Invalidate cache so it gets refreshed
        RefreshContainerCache();
    }

    bool ContainerManager::IsContainerRegistered(RE::FormID a_formID) const {
        return ContainerRegistry::GetSingleton()->ShouldPullFrom(a_formID);
    }

    bool ContainerManager::IsPlayerFollower(RE::TESObjectREFR* a_ref) {
        if (!a_ref || (!m_currentFollowerFaction && !m_playerMarriedFaction)) return false;

        auto* actor = a_ref->As<RE::Actor>();
        if (!actor) return false;

        if (actor->IsPlayerRef()) return false;
        if (actor->IsDead()) return false;

        bool inFollowerFaction = m_currentFollowerFaction && actor->IsInFaction(m_currentFollowerFaction);
        bool inMarriedFaction = m_playerMarriedFaction && actor->IsInFaction(m_playerMarriedFaction);

        return inFollowerFaction || inMarriedFaction;
    }

    std::vector<RE::TESObjectREFR*> ContainerManager::GetNearbyFollowers(float a_maxDistance) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {};

        auto* currentCell = player->GetParentCell();
        if (!currentCell) return {};

        float maxDist = a_maxDistance > 0.0f ? a_maxDistance : INISettings::GetSingleton()->GetMaxContainerDistance();
        auto playerPos = player->GetPosition();

        std::vector<RE::TESObjectREFR*> result;
        currentCell->ForEachReference([this, &result, &playerPos, maxDist](RE::TESObjectREFR& ref) {
            if (IsPlayerFollower(&ref)) {
                float distance = playerPos.GetDistance(ref.GetPosition());
                if (distance <= maxDist) {
                    result.push_back(&ref);
                    logger::debug("Found nearby follower: {:08X} '{}' at distance {:.0f}",
                        ref.GetFormID(),
                        ref.GetDisplayFullName() ? ref.GetDisplayFullName() : "unnamed",
                        distance);
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });

        return result;
    }

    bool ContainerManager::IsCookingStation(RE::TESObjectREFR* a_furniture) {
        if (!a_furniture || !m_craftingCookpotKeyword) return false;

        auto* base = a_furniture->GetBaseObject();
        if (!base) return false;

        // TESFurniture inherits from BGSKeywordForm and has keyword support
        auto* furn = base->As<RE::TESFurniture>();
        if (!furn) return false;

        return furn->HasKeyword(m_craftingCookpotKeyword);
    }

    RE::TESObjectREFR* ContainerManager::GetNFFAdditionalInventory(RE::Actor* a_follower) {
        if (!a_follower || !m_nffXStoreFaction || !m_nffXStorageQuest) return nullptr;

        // Check if follower is in the NFF extra storage faction
        if (!a_follower->IsInFaction(m_nffXStoreFaction)) return nullptr;

        // NFF quest nwsFollowerXStorage has two parallel alias sets:
        //   aliasID 0-9:   storageFollower00-09 (holds the follower actor)
        //   aliasID 10-19: StorageContainer00-09 (holds the container ref)
        // Follower at slot N maps to container at slot N+10.
        // Find which follower slot holds this actor, then grab the container alias.
        constexpr std::uint32_t kFollowerSlotStart = 0;
        constexpr std::uint32_t kFollowerSlotCount = 10;
        constexpr std::uint32_t kContainerSlotOffset = 10;

        // First pass: find the follower's slot by checking follower aliases 0-9
        std::int32_t foundSlot = -1;
        for (auto* alias : m_nffXStorageQuest->aliases) {
            if (!alias) continue;
            if (alias->aliasID < kFollowerSlotStart || alias->aliasID >= kFollowerSlotStart + kFollowerSlotCount) continue;

            auto* refAlias = static_cast<RE::BGSRefAlias*>(alias);
            auto* aliasRef = refAlias->GetReference();
            if (aliasRef && aliasRef->GetFormID() == a_follower->GetFormID()) {
                foundSlot = static_cast<std::int32_t>(alias->aliasID);
                break;
            }
        }

        if (foundSlot < 0) {
            logger::debug("NFF: follower {:08X} in xStoreFac but not found in any follower alias slot",
                a_follower->GetFormID());
            return nullptr;
        }

        // Second pass: get the container alias at slot + offset
        std::uint32_t containerAliasID = static_cast<std::uint32_t>(foundSlot) + kContainerSlotOffset;
        for (auto* alias : m_nffXStorageQuest->aliases) {
            if (!alias || alias->aliasID != containerAliasID) continue;

            auto* refAlias = static_cast<RE::BGSRefAlias*>(alias);
            auto* containerRef = refAlias->GetReference();
            if (containerRef) {
                logger::debug("NFF: found Additional Inventory {:08X} for follower {:08X} (follower slot {}, container alias {})",
                    containerRef->GetFormID(), a_follower->GetFormID(), foundSlot, containerAliasID);
                return containerRef;
            }
        }

        logger::debug("NFF: no container ref for follower {:08X} (follower slot {}, container alias {})",
            a_follower->GetFormID(), foundSlot, containerAliasID);
        return nullptr;
    }
}
