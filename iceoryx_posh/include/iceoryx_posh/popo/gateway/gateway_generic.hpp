// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IOX_POSH_GATEWAY_GENERIC_HPP
#define IOX_POSH_GATEWAY_GENERIC_HPP

#include "iceoryx_posh/iceoryx_posh_types.hpp"
#include "iceoryx_posh/iceoryx_posh_config.hpp"
#include "iceoryx_posh/capro/service_description.hpp"
#include "iceoryx_posh/popo/gateway/gateway_config.hpp"
#include "iceoryx_posh/popo/gateway/gateway_base.hpp"
#include "iceoryx_utils/cxx/expected.hpp"
#include "iceoryx_utils/cxx/function_ref.hpp"
#include "iceoryx_utils/cxx/optional.hpp"
#include "iceoryx_utils/cxx/string.hpp"
#include "iceoryx_utils/cxx/vector.hpp"
#include "iceoryx_utils/internal/concurrent/smart_lock.hpp"

#include <atomic>
#include <thread>

namespace iox
{
namespace popo
{
enum class GatewayError : uint8_t
{
    UNSUPPORTED_SERVICE_TYPE,
    UNSUCCESSFUL_CHANNEL_CREATION,
    NONEXISTANT_CHANNEL
};

///
/// @brief A reference generic gateway implementation.
/// @details This class can be extended to quickly implement any type of gateway, only custom initialization,
/// configuration, discovery and forwarding logic needs to be provided.
///
/// When run, the gateway will automatically call the respective methods when required.
///
template <typename channel_t, typename gateway_t = iox::popo::GatewayBase>
class GatewayGeneric : public gateway_t
{
    using ChannelVector = iox::cxx::vector<channel_t, MAX_CHANNEL_NUMBER>;
    using ConcurrentChannelVector = iox::concurrent::smart_lock<ChannelVector>;

  public:
    virtual ~GatewayGeneric() noexcept;

    GatewayGeneric(const GatewayGeneric&) = delete;
    GatewayGeneric& operator=(const GatewayGeneric&) = delete;
    GatewayGeneric(GatewayGeneric&&) = delete;
    GatewayGeneric& operator=(GatewayGeneric&&) = delete;

    void runMultithreaded() noexcept;
    void shutdown() noexcept;

    ///
    /// @brief loadConfiguration Load the provided configuration.
    /// @param config A generic gateay implementation.
    ///
    virtual void loadConfiguration(const GatewayConfig& config) noexcept = 0;
    ///
    /// @brief discover Process discovery messages coming from iceoryx.
    /// @param msg The discovery message.
    ///
    virtual void discover(const iox::capro::CaproMessage& msg) noexcept = 0;
    ///
    /// @brief forward Forward data between the two terminals of the channel used by the implementation.
    /// @param channel The channel to propogate data across.
    ///
    virtual void forward(const channel_t& channel) noexcept = 0;

    uint64_t getNumberOfChannels() const noexcept;

  protected:
    GatewayGeneric() noexcept;

    ///
    /// @brief addChannel Creates a channel for the given service and stores a copy of it in an internal collection for
    /// later access.
    /// @param service The service to create a channel for.
    /// @return an expected containing a copy of the added channel, otherwise an error
    ///
    /// @note Wildcard services are not allowed and will be ignored.
    ///
    /// @note Channels are supposed to be lightweight, consisting only of pointers to the terminals and a copy of the
    /// service description, therefore a copy is provided to any entity that requires them.
    /// When no more copies of a channel exists in the system, the terminals will automatically be cleaned up via
    /// the custom deleters included in their pointers.
    ///
    /// The service description is perhaps too large for copying since they contain strings, however this should be
    /// addressed with a service description repository feature.
    ///
    iox::cxx::expected<channel_t, GatewayError>
    addChannel(const iox::capro::ServiceDescription& service) noexcept;

    ///
    /// @brief findChannel Searches for a channel for the given service in the internally stored collection and returns
    /// it one exists.
    /// @param service The service to find a channel for.
    /// @return An optional containining the matching channel if one exists, otherwise an empty optional.
    ///
    iox::cxx::optional<channel_t> findChannel(const iox::capro::ServiceDescription& service) const noexcept;

    ///
    /// @brief forEachChannel Executs the given function for each channel in the internally stored collection.
    /// @param f The function to execute.
    /// @note This operation allows thread-safe access to the internal collection.
    ///
    void forEachChannel(const iox::cxx::function_ref<void(channel_t&)> f) const noexcept;

    ///
    /// @brief discardChannel Discard the channel for the given service in the internal collection if one exists.
    /// @param service The service whose channels hiould be discarded.
    /// @return an empty expected on success, otherwise an error
    ///
    iox::cxx::expected<GatewayError> discardChannel(const iox::capro::ServiceDescription& service) noexcept;

  private:
    ConcurrentChannelVector m_channels;

    std::atomic_bool m_isRunning{false};

    std::thread m_discoveryThread;
    std::thread m_forwardingThread;

    void forwardingLoop() noexcept;
    void discoveryLoop() noexcept;
};

} // namespace popo
} // namespace iox

#include "iceoryx_dds/internal/gateway/dds_gateway_generic.inl"

#endif // IOX_POSH_GATEWAY_GENERIC_HPP
