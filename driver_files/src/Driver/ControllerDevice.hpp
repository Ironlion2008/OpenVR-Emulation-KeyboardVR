#pragma once

#include <Driver/IVRDevice.hpp>
#include <Driver/InputConfig.hpp>
#include <Native/DriverFactory.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace OpenVREmulatorDriver
{
class ControllerDevice : public IVRDevice
{
public:
    enum class Handedness : std::uint8_t
    {
        LEFT,
        RIGHT,
        ANY
    };

    explicit ControllerDevice(std::string serial, Handedness handedness = Handedness::ANY,
                               InputConfig config = InputConfig::Defaults());
    ~ControllerDevice() override = default; // NOLINT(cppcoreguidelines-special-member-functions)

    // Inherited via IVRDevice
    std::string GetSerial() override;
    void Update() override;
    vr::TrackedDeviceIndex_t GetDeviceIndex() override;
    DeviceType GetDeviceType() override;
    Handedness GetHandedness();

    vr::EVRInitError Activate(uint32_t unObjectId) override;
    void Deactivate() override;
    void EnterStandby() override;
    void *GetComponent(const char *pchComponentNameAndVersion) override;
    void DebugRequest(const char *pchRequest, char *pchResponseBuffer,
                      uint32_t unResponseBufferSize) override;
    vr::DriverPose_t GetPose() override;
    [[nodiscard]] bool IsJoystickEnabled() const;

private:
    vr::TrackedDeviceIndex_t device_index_ = vr::k_unTrackedDeviceIndexInvalid;
    std::string serial_;
    Handedness handedness_;
    InputConfig config_;

    vr::DriverPose_t last_pose_;

    bool did_vibrate_ = false;
    float vibrate_anim_state_ = 0.f;

    // Accumulated controller pose offset (keyboard mode or XInput mode).
    float pose_adjust_x_ = 0.f;
    float pose_adjust_y_ = 0.f;
    float pose_adjust_z_ = 0.f;

    // Independent hand rotation in keyboard mode (radians).
    float hand_pitch_ = 0.f;
    float hand_yaw_ = 0.f;
    float hand_roll_ = 0.f;

    bool reset_pose_was_pressed_ = false;

    bool joystick_enabled_ = false;

    // Shared across both controller instances: Back button swaps left/right input mapping.
    static bool m_sSwapped;
    static bool m_sBackWasPressed;

    vr::VRInputComponentHandle_t haptic_component_ = 0;

    vr::VRInputComponentHandle_t a_button_click_component_ = 0;
    vr::VRInputComponentHandle_t a_button_touch_component_ = 0;

    vr::VRInputComponentHandle_t b_button_click_component_ = 0;
    vr::VRInputComponentHandle_t b_button_touch_component_ = 0;

    vr::VRInputComponentHandle_t x_button_click_component_ = 0;
    vr::VRInputComponentHandle_t x_button_touch_component_ = 0;

    vr::VRInputComponentHandle_t y_button_click_component_ = 0;
    vr::VRInputComponentHandle_t y_button_touch_component_ = 0;

    vr::VRInputComponentHandle_t trigger_value_component_ = 0;
    vr::VRInputComponentHandle_t trigger_click_component_ = 0;
    vr::VRInputComponentHandle_t trigger_touch_component_ = 0;

    vr::VRInputComponentHandle_t grip_touch_component_ = 0;
    vr::VRInputComponentHandle_t grip_value_component_ = 0;
    vr::VRInputComponentHandle_t grip_force_component_ = 0;

    vr::VRInputComponentHandle_t system_click_component_ = 0;
    vr::VRInputComponentHandle_t system_touch_component_ = 0;

    vr::VRInputComponentHandle_t trackpad_click_component_ = 0;
    vr::VRInputComponentHandle_t trackpad_touch_component_ = 0;
    vr::VRInputComponentHandle_t trackpad_x_component_ = 0;
    vr::VRInputComponentHandle_t trackpad_y_component_ = 0;

    vr::VRInputComponentHandle_t joystick_click_component_ = 0;
    vr::VRInputComponentHandle_t joystick_touch_component_ = 0;
    vr::VRInputComponentHandle_t joystick_x_component_ = 0;
    vr::VRInputComponentHandle_t joystick_y_component_ = 0;

    // vr::VRInputComponentHandle_t skeleton_left_component_ = 0;
    // vr::VRInputComponentHandle_t skeleton_right_component_ = 0;
};
};  // namespace OpenVREmulatorDriver
