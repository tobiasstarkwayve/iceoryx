#pragma once

#include <memory>

#include <iceoryx_posh/capro/service_description.hpp>
#include <iceoryx_utils/internal/objectpool/objectpool.hpp>
#include "ioxdds/dds/dds_configs.hpp"

namespace iox {
namespace dds{

///
/// @class Channel
/// @brief A data structure representing a channel between Iceoryx and DDS.
///
/// The class couples related iceoryx and dds entities that communicate with eachother to form the communication
/// channel.
/// For example: An Iceoryx subscriber and its corresponding DDS data writer, which communicate eachother to form
///              an outbound communication channel.
/// These entites are conceptualized as channel "Terminals".
///
/// The structure holds pointers to the instances of the terminals.
/// The terminals can be created and managed externally, in which case the structure only serves as a means of coupling
/// the two.
/// This can be achieved by simply calling the constructor with pointers to them.
///
/// Alternatively, the class can manage the terminals internally in a static object pool, automatically
/// cleaning them up when the channel is discarded.
/// This can be achieved via the Channel::create method.
///
template <typename IoxTerminal, typename DDSTerminal>
class Channel
{
    using IoxTerminalPtr = std::shared_ptr<IoxTerminal>;
    using IoxTerminalPool = iox::cxx::ObjectPool<IoxTerminal, MAX_CHANNEL_NUMBER>;
    using DDSTerminalPtr = std::shared_ptr<DDSTerminal>;
    using DDSTerminalPool = iox::cxx::ObjectPool<DDSTerminal, MAX_CHANNEL_NUMBER>;

public:

    Channel(const iox::capro::ServiceDescription& service,
            const IoxTerminalPtr ioxInterface,
            const DDSTerminalPtr ddsInterface) noexcept;

    static Channel create(const iox::capro::ServiceDescription& service) noexcept;

    iox::capro::ServiceDescription getService() const noexcept;
    IoxTerminalPtr getIceoryxTerminal() const noexcept;
    DDSTerminalPtr getDDSTerminal() const noexcept;

private:

    static IoxTerminalPool s_ioxTerminals;
    static DDSTerminalPool s_ddsTerminals;

    iox::capro::ServiceDescription m_service;
    IoxTerminalPtr m_ioxTerminal;
    DDSTerminalPtr m_ddsTerminal;

};

} // namespace dds
} // namespace iox

#include "ioxdds/internal/gateway/channel.inl"
