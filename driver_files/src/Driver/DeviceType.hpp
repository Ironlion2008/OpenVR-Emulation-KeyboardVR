#pragma once
#include <cstdint>
enum class DeviceType : std::uint8_t
{
    HMD,
    CONTROLLER,
    TRACKER,
    TrackingReference
};
