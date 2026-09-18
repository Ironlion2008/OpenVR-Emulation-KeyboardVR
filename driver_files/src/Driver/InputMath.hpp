#pragma once

#include <DirectXMath.h>
#include <Windows.h>
#include <Xinput.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace OpenVREmulatorDriver
{

// ---------------------------------------------------------------------------
// XInput normalisation helpers
// ---------------------------------------------------------------------------

// Normalize a thumb-stick axis value to [-1, 1], applying a dead-zone.
// Returns 0 when |value| <= deadzone.
inline float NormalizeThumbAxis(SHORT value, SHORT deadzone) noexcept
{
    if (value > deadzone) {
        return static_cast<float>(value - deadzone) /
               static_cast<float>((std::numeric_limits<int16_t>::max)() - deadzone);  // NOLINT(readability-redundant-parentheses)
    }
    if (value < -deadzone) {
        return static_cast<float>(value + deadzone) /
               static_cast<float>((std::numeric_limits<int16_t>::max)() + 1 - deadzone);  // NOLINT(readability-redundant-parentheses)
    }
    return 0.0f;
}

// Normalize a trigger byte value to [0, 1], applying the XInput trigger threshold.
// Returns 0 when value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD.
inline float NormalizeTrigger(BYTE value) noexcept
{
    if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        return 0.0f;
    }
    return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
           static_cast<float>((std::numeric_limits<uint8_t>::max)() -  // NOLINT(readability-redundant-parentheses)
                               XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

// Clamp a float value to [0, 1].
inline float Clamp01(float value) noexcept
{
    return std::fmax(0.0f, std::fmin(value, 1.0f));
}

}  // namespace OpenVREmulatorDriver
