#include "Services/SLIDIntegration.h"

#include <filesystem>

namespace Services {

    // SLID message types (must match SLID's API)
    namespace SLIDMessageType {
        constexpr std::uint32_t kRequestNetworkList = 'SLNL';
        constexpr std::uint32_t kRequestNetworkContainers = 'SLNC';
        constexpr std::uint32_t kResponseNetworkList = 'SLRL';
        constexpr std::uint32_t kResponseNetworkContainers = 'SLRC';
    }

    // SLID request/response structures (must match SLID's API)
    struct NetworkContainersRequest {
        char networkName[64];
    };

    struct NetworkContainersResponse {
        char networkName[64];           // Echoed back from request
        RE::FormID masterFormID;
        RE::FormID catchAllFormID;
        std::uint32_t filterCount;
        // Followed by filterCount RE::FormID values
    };

    SLIDIntegration* SLIDIntegration::GetSingleton() {
        static SLIDIntegration singleton;
        return &singleton;
    }

    void SLIDIntegration::Initialize() {
        // Register listener for SLID messages
        auto* messaging = SKSE::GetMessagingInterface();
        if (messaging) {
            messaging->RegisterListener("SLID", [](SKSE::MessagingInterface::Message* a_msg) {
                GetSingleton()->HandleMessage(a_msg);
            });
            logger::info("SLID integration: registered message listener");
        }
    }

    bool SLIDIntegration::IsSLIDInstalled() const {
        if (m_slidChecked) {
            return m_slidDetected;
        }

        // Check for SLID DLL
        auto pluginPath = std::filesystem::path("Data/SKSE/Plugins/SLID.dll");
        m_slidDetected = std::filesystem::exists(pluginPath);
        m_slidChecked = true;

        if (m_slidDetected) {
            logger::info("SLID integration: SLID.dll detected");
        }

        return m_slidDetected;
    }

    void SLIDIntegration::RequestNetworkList() {
        if (!IsSLIDInstalled()) return;

        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) return;

        logger::debug("SLID integration: requesting network list");
        messaging->Dispatch(
            SLIDMessageType::kRequestNetworkList,
            nullptr,
            0,
            "SLID"
        );
    }

    void SLIDIntegration::RequestNetworkContainers(const std::string& a_networkName) {
        if (!IsSLIDInstalled()) return;

        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) return;

        NetworkContainersRequest request{};
        strncpy_s(request.networkName, a_networkName.c_str(), sizeof(request.networkName) - 1);

        logger::debug("SLID integration: requesting containers for network '{}'", a_networkName);
        messaging->Dispatch(
            SLIDMessageType::kRequestNetworkContainers,
            &request,
            sizeof(request),
            "SLID"
        );
    }

    void SLIDIntegration::HandleMessage(SKSE::MessagingInterface::Message* a_msg) {
        if (!a_msg) return;

        // Only process messages from SLID
        if (!a_msg->sender || strcmp(a_msg->sender, "SLID") != 0) {
            return;
        }

        logger::debug("SLID integration: received message type {:08X}", a_msg->type);

        switch (a_msg->type) {
            case SLIDMessageType::kResponseNetworkList: {
                std::lock_guard<std::mutex> lock(m_networkNamesMutex);
                m_networkNames.clear();

                if (a_msg->data && a_msg->dataLen >= sizeof(std::uint32_t)) {
                    auto* data = static_cast<const char*>(a_msg->data);
                    std::uint32_t count = *reinterpret_cast<const std::uint32_t*>(data);
                    data += sizeof(std::uint32_t);

                    for (std::uint32_t i = 0; i < count; ++i) {
                        m_networkNames.emplace_back(data);
                        data += strlen(data) + 1;  // Skip past null terminator
                    }

                    logger::info("SLID integration: received {} network names", count);
                    for (const auto& name : m_networkNames) {
                        logger::debug("  - {}", name);
                    }
                }
                break;
            }

            case SLIDMessageType::kResponseNetworkContainers: {
                if (a_msg->data && a_msg->dataLen >= sizeof(NetworkContainersResponse)) {
                    auto* response = static_cast<const NetworkContainersResponse*>(a_msg->data);

                    std::lock_guard<std::mutex> lock(m_containersMutex);
                    std::uint32_t added = 0;

                    // Helper to add FormID if not already seen
                    auto addFormID = [this, &added](RE::FormID formID) {
                        if (formID != 0 && !m_seenContainers.contains(formID)) {
                            m_collectedContainers.push_back(formID);
                            m_seenContainers.insert(formID);
                            ++added;
                        }
                    };

                    addFormID(response->masterFormID);
                    addFormID(response->catchAllFormID);

                    auto* filterIDs = reinterpret_cast<const RE::FormID*>(
                        static_cast<const char*>(a_msg->data) + sizeof(NetworkContainersResponse)
                    );
                    for (std::uint32_t i = 0; i < response->filterCount; ++i) {
                        addFormID(filterIDs[i]);
                    }

                    logger::debug("SLID integration: received {} containers for network '{}' (master={:08X}, catchAll={:08X}, filters={})",
                        added, response->networkName, response->masterFormID, response->catchAllFormID, response->filterCount);
                }
                break;
            }
        }
    }

    std::vector<std::string> SLIDIntegration::GetNetworkNames() const {
        std::lock_guard<std::mutex> lock(m_networkNamesMutex);
        return m_networkNames;
    }

    bool SLIDIntegration::IsNetworkEnabled(const std::string& a_networkName) const {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        return m_enabledNetworks.contains(a_networkName);
    }

    void SLIDIntegration::SetNetworkEnabled(const std::string& a_networkName, bool a_enabled) {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        if (a_enabled) {
            m_enabledNetworks.insert(a_networkName);
            logger::info("SLID integration: enabled network '{}'", a_networkName);
        } else {
            m_enabledNetworks.erase(a_networkName);
            logger::info("SLID integration: disabled network '{}'", a_networkName);
        }
    }

    std::vector<std::string> SLIDIntegration::GetEnabledNetworks() const {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        return std::vector<std::string>(m_enabledNetworks.begin(), m_enabledNetworks.end());
    }

    std::vector<RE::FormID> SLIDIntegration::GetCollectedContainers() const {
        std::lock_guard<std::mutex> lock(m_containersMutex);
        return m_collectedContainers;
    }

    std::uint32_t SLIDIntegration::QueryEnabledNetworkContainers() {
        // Clear previous collection
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_collectedContainers.clear();
            m_seenContainers.clear();
        }

        // Get current network list to check for missing
        auto currentNetworks = GetNetworkNames();
        std::unordered_set<std::string> available(currentNetworks.begin(), currentNetworks.end());

        auto enabled = GetEnabledNetworks();
        std::uint32_t queried = 0;

        for (const auto& networkName : enabled) {
            if (available.contains(networkName)) {
                RequestNetworkContainers(networkName);
                ++queried;
            } else {
                logger::warn("SLID integration: enabled network '{}' no longer exists, skipping", networkName);
            }
        }

        logger::debug("SLID integration: queried {} networks for containers", queried);
        return queried;
    }

    std::vector<RE::TESObjectREFR*> SLIDIntegration::GetEnabledNetworkContainerRefs() {
        // Query containers for all enabled networks (logs warnings for missing)
        QueryEnabledNetworkContainers();

        auto formIDs = GetCollectedContainers();
        std::vector<RE::TESObjectREFR*> result;
        result.reserve(formIDs.size());

        for (auto formID : formIDs) {
            auto* form = RE::TESForm::LookupByID(formID);
            auto* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
            if (refr) {
                result.push_back(refr);
            } else {
                logger::warn("SLID integration: failed to resolve FormID {:08X}", formID);
            }
        }

        return result;
    }

    void SLIDIntegration::RemoveNetwork(const std::string& a_networkName) {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        if (m_enabledNetworks.erase(a_networkName)) {
            logger::info("SLID integration: removed network '{}' from enabled set", a_networkName);
        }
    }

    std::vector<std::string> SLIDIntegration::GetMissingNetworks() const {
        std::lock_guard<std::mutex> enabledLock(m_enabledNetworksMutex);
        std::lock_guard<std::mutex> namesLock(m_networkNamesMutex);

        std::vector<std::string> missing;
        std::unordered_set<std::string> available(m_networkNames.begin(), m_networkNames.end());

        for (const auto& enabled : m_enabledNetworks) {
            if (!available.contains(enabled)) {
                missing.push_back(enabled);
            }
        }

        return missing;
    }

    void SLIDIntegration::ClearCache() {
        {
            std::lock_guard<std::mutex> lock(m_networkNamesMutex);
            m_networkNames.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_collectedContainers.clear();
            m_seenContainers.clear();
        }
        logger::debug("SLID integration: cache cleared");
    }

    void SLIDIntegration::Refresh() {
        if (!IsSLIDInstalled()) return;

        ClearCache();
        RequestNetworkList();
        // Note: don't query containers here - that happens at craft time
    }

    void SLIDIntegration::SaveToCoSave(SKSE::SerializationInterface* a_intfc) {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);

        // Write count
        std::uint32_t count = static_cast<std::uint32_t>(m_enabledNetworks.size());
        a_intfc->WriteRecordData(&count, sizeof(count));

        // Write each network name (length-prefixed string)
        for (const auto& name : m_enabledNetworks) {
            std::uint32_t len = static_cast<std::uint32_t>(name.size());
            a_intfc->WriteRecordData(&len, sizeof(len));
            a_intfc->WriteRecordData(name.data(), len);
        }

        logger::info("SLID integration: saved {} enabled networks to cosave", count);
    }

    void SLIDIntegration::LoadFromCoSave(SKSE::SerializationInterface* a_intfc, std::uint32_t) {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        m_enabledNetworks.clear();

        // Read count
        std::uint32_t count = 0;
        if (!a_intfc->ReadRecordData(&count, sizeof(count))) {
            logger::warn("SLID integration: failed to read network count from cosave");
            return;
        }

        // Read each network name
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t len = 0;
            if (!a_intfc->ReadRecordData(&len, sizeof(len))) {
                logger::warn("SLID integration: failed to read network name length");
                break;
            }

            std::string name(len, '\0');
            if (!a_intfc->ReadRecordData(name.data(), len)) {
                logger::warn("SLID integration: failed to read network name");
                break;
            }

            m_enabledNetworks.insert(name);
        }

        logger::info("SLID integration: loaded {} enabled networks from cosave", m_enabledNetworks.size());
    }

    void SLIDIntegration::Revert() {
        std::lock_guard<std::mutex> lock(m_enabledNetworksMutex);
        m_enabledNetworks.clear();
        ClearCache();
        logger::debug("SLID integration: reverted");
    }

}  // namespace Services
