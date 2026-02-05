#pragma once

namespace Services {
    /// Manages discovery and caching of nearby crafting-enabled containers
    class ContainerManager {
    public:
        static ContainerManager* GetSingleton();

        /// Initialize the container manager (called on data load)
        void Initialize();

        /// Get all containers marked as crafting sources in the current cell
        /// @param a_maxDistance Maximum distance from player (0 = use INI setting)
        /// @return Vector of container references sorted by distance
        std::vector<RE::TESObjectREFR*> GetNearbyCraftingContainers(float a_maxDistance = 0.0f);

        /// Check if a container is a valid crafting source
        /// @param a_container The container reference to check
        /// @return true if the container should be included in crafting
        bool IsValidCraftingSource(RE::TESObjectREFR* a_container);

        /// Refresh the cached container list (call when cell changes)
        void RefreshContainerCache();

        /// Get the keyword used to mark containers as crafting sources
        RE::BGSKeyword* GetCraftingSourceKeyword() const { return m_craftingSourceKeyword; }

        /// Register a container as a crafting source (called from Papyrus)
        void RegisterContainer(RE::TESObjectREFR* a_container);

        /// Unregister a container as a crafting source (called from Papyrus)
        void UnregisterContainer(RE::TESObjectREFR* a_container);

        /// Check if a container is registered
        bool IsContainerRegistered(RE::FormID a_formID) const;

    private:
        ContainerManager() = default;
        ~ContainerManager() = default;
        ContainerManager(const ContainerManager&) = delete;
        ContainerManager& operator=(const ContainerManager&) = delete;

        /// Scan the current cell for containers
        void ScanCellForContainers();

        /// Check if a container is owned by someone other than the player
        bool IsOwnedByOther(RE::TESObjectREFR* a_container);

        /// Check if a container is forbidden (merchant, jail evidence, etc.)
        bool IsForbiddenContainer(RE::TESObjectREFR* a_container);

    public:
        /// Check if an actor is a player-owned mount (horse)
        bool IsPlayerOwnedMount(RE::TESObjectREFR* a_ref);

        /// Check if a reference is a player follower or spouse
        bool IsPlayerFollower(RE::TESObjectREFR* a_ref);

        /// Get all nearby followers/spouses within range
        /// @param a_maxDistance Maximum distance from player (0 = use INI setting)
        std::vector<RE::TESObjectREFR*> GetNearbyFollowers(float a_maxDistance = 0.0f);

        /// Check if a furniture reference is a cooking station (has CraftingCookpot keyword)
        bool IsCookingStation(RE::TESObjectREFR* a_furniture);

        /// Get NFF Additional Inventory container for a follower (nullptr if none)
        RE::TESObjectREFR* GetNFFAdditionalInventory(RE::Actor* a_follower);

    private:

        RE::BGSKeyword* m_craftingSourceKeyword = nullptr;
        RE::TESFaction* m_playerHorseFaction = nullptr;
        RE::TESFaction* m_chHorseFaction = nullptr;   // Convenient Horses compatibility
        RE::TESFaction* m_nffXStoreFaction = nullptr;  // NFF nwsFF_xStoreFac
        RE::TESQuest* m_nffXStorageQuest = nullptr;    // NFF nwsFollowerXStorage
        RE::TESFaction* m_currentFollowerFaction = nullptr;
        RE::TESFaction* m_playerMarriedFaction = nullptr;
        RE::BGSKeyword* m_craftingCookpotKeyword = nullptr;
        std::vector<RE::TESObjectREFR*> m_cachedContainers;
        RE::TESObjectCELL* m_cachedCell = nullptr;
        mutable std::mutex m_cacheMutex;
    };
}
