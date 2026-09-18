#include "ControllerDevice.hpp"

#include <Windows.h>
#include <Xinput.h>

#include <numbers>

#include "InputMath.hpp"

using namespace DirectX;

namespace
{
using OpenVREmulatorDriver::Clamp01;

constexpr DWORD XInputControllerIndex = 0;
constexpr float MinHapticDurationSeconds = 0.02f;
constexpr float MillisecondsPerSecond = 1000.0f;
constexpr float MaxMotorSpeed = 65535.0f;
constexpr float JoystickTouchThreshold = 0.1f;
constexpr float VibrateYOffset = -0.2f;
constexpr float VibrateAmplitude = 0.01f;
constexpr float VibrateFrequency = 8.0f;
constexpr float ControllerXOffset = 0.2f;
constexpr float ControllerZOffset = -0.5f;

bool KeyDown(int virtualKey) noexcept
{
    return virtualKey != 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

struct SharedXInputRumbleState
{
    float left_motor = 0.0f;
    float right_motor = 0.0f;
    std::chrono::steady_clock::time_point left_motor_end_time;
    std::chrono::steady_clock::time_point right_motor_end_time;
};

SharedXInputRumbleState
    gXinputRumbleState;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void QueueXInputRumble(bool leftMotor,
                       float amplitude,  // NOLINT(bugprone-easily-swappable-parameters)
                       float durationSeconds)
{
    auto endTime =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(std::fmax(durationSeconds, MinHapticDurationSeconds)));

    if (leftMotor)
    {
        gXinputRumbleState.left_motor = Clamp01(amplitude);
        gXinputRumbleState.left_motor_end_time = endTime;
    }
    else
    {
        gXinputRumbleState.right_motor = Clamp01(amplitude);
        gXinputRumbleState.right_motor_end_time = endTime;
    }
}

void ApplyXInputRumble(bool hasXinput)
{
    auto now = std::chrono::steady_clock::now();
    if (now >= gXinputRumbleState.left_motor_end_time)
    {
        gXinputRumbleState.left_motor = 0.0f;
    }

    if (now >= gXinputRumbleState.right_motor_end_time)
    {
        gXinputRumbleState.right_motor = 0.0f;
    }

    if (!hasXinput)
    {
        return;
    }

    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = static_cast<WORD>(gXinputRumbleState.left_motor * MaxMotorSpeed);
    vibration.wRightMotorSpeed = static_cast<WORD>(gXinputRumbleState.right_motor * MaxMotorSpeed);
    XInputSetState(XInputControllerIndex, &vibration);
}
}  // namespace

namespace OpenVREmulatorDriver
{

ControllerDevice::ControllerDevice(std::string serial, ControllerDevice::Handedness handedness,
                                   InputConfig config)
    : serial_(std::move(serial)),
      handedness_(handedness),
      config_(config),
      last_pose_(IVRDevice::MakeDefaultPose())
{
}

bool ControllerDevice::m_sSwapped = false;
bool ControllerDevice::m_sBackWasPressed = false;

std::string ControllerDevice::GetSerial()
{
    return this->serial_;
}

void ControllerDevice::Update()  // NOLINT(readability-function-cognitive-complexity)
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }

    const float deltaSeconds =
        static_cast<float>(GetDriver()->GetLastFrameTime().count()) / MillisecondsPerSecond;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events)
    {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not
        // sure why, but the component handle will match so we can just use that instead
        // if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration)
        {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_)
            {
                this->did_vibrate_ = true;
                // Route rumble based on effective (possibly swapped) handedness.
                const bool effectiveLeft = m_sSwapped ? (this->handedness_ == Handedness::RIGHT)
                                                      : (this->handedness_ == Handedness::LEFT);
                if (effectiveLeft)
                {
                    QueueXInputRumble(true, event.data.hapticVibration.fAmplitude,
                                      event.data.hapticVibration.fDurationSeconds);
                }
                else
                {
                    QueueXInputRumble(false, event.data.hapticVibration.fAmplitude,
                                      event.data.hapticVibration.fDurationSeconds);
                }
            }
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_)
    {
        this->vibrate_anim_state_ +=
            static_cast<float>(GetDriver()->GetLastFrameTime().count()) / MillisecondsPerSecond;
        if (this->vibrate_anim_state_ > 1.0f)
        {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    // Setup pose for this frame
    auto pose = IVRDevice::MakeDefaultPose();
    XINPUT_STATE xinputState = {};
    // Keyboard-only mode deliberately bypasses XInput completely.
    const bool hasXinput =
        !config_.keyboard_only && XInputGetState(XInputControllerIndex, &xinputState) == ERROR_SUCCESS;

    // Back button (either controller sees it) toggles left/right input swap.
    const bool backNow = hasXinput && (xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
    if (this->handedness_ == Handedness::LEFT)
    {  // only one controller drives the toggle
        if (backNow && !m_sBackWasPressed)
        {
            m_sSwapped = !m_sSwapped;
            GetDriver()->Log(std::string("Controller mapping ") +
                             (m_sSwapped ? "swapped" : "restored"));
        }
        m_sBackWasPressed = backNow;
    }

    // Effective handedness: swap input reading when m_sSwapped is active.
    const bool isLeftController = m_sSwapped ? (this->handedness_ == Handedness::RIGHT)
                                             : (this->handedness_ == Handedness::LEFT);
    const bool isRightController = m_sSwapped ? (this->handedness_ == Handedness::LEFT)
                                              : (this->handedness_ == Handedness::RIGHT);

    const auto &lc = config_.left_controller;
    const auto &rc = config_.right_controller;

    // Find a HMD
    auto devices = GetDriver()->GetDevices();
    auto hmd = std::ranges::find_if(devices, [](const std::shared_ptr<IVRDevice> &devicePtr) {
        return devicePtr->GetDeviceType() == DeviceType::HMD;
    });
    if (hmd != devices.end())
    {
        // Found a HMD
        const vr::DriverPose_t hmdPose = (*hmd)->GetPose();

        // Here we setup some transforms so our controllers are offset from the headset by a small
        // amount so we can see them
        const XMFLOAT3 hmdPosition{static_cast<float>(hmdPose.vecPosition[0]),
                                   static_cast<float>(hmdPose.vecPosition[1]),
                                   static_cast<float>(hmdPose.vecPosition[2])};
        const XMFLOAT4 hmdRotation{
            static_cast<float>(hmdPose.qRotation.x), static_cast<float>(hmdPose.qRotation.y),
            static_cast<float>(hmdPose.qRotation.z), static_cast<float>(hmdPose.qRotation.w)};

        // Do shaking animation if haptic vibration was requested
        const float controllerY =
            VibrateYOffset +
            (VibrateAmplitude *
             std::sinf(VibrateFrequency * std::numbers::pi_v<float> * vibrate_anim_state_));

        // Left hand controller on the left, right hand controller on the right, any other
        // handedness sticks to the middle
        float controllerX = 0.0f;
        if (this->handedness_ == Handedness::LEFT)
        {
            controllerX = -ControllerXOffset;
        }
        else if (this->handedness_ == Handedness::RIGHT)
        {
            controllerX = ControllerXOffset;
        }

        // Controller pose controls.
        // In keyboard-only mode each hand has its own 6DOF digital pose controls.
        const auto &kc = isLeftController ? lc.keyboard : rc.keyboard;
        const float moveSpeed = isLeftController ? rc.pose_move_speed : rc.pose_move_speed;
        const float rotateSpeed = rc.pose_rotate_speed;

        const bool resetNow = config_.keyboard_only && KeyDown(kc.reset_pose);
        if (resetNow && !this->reset_pose_was_pressed_)
        {
            this->pose_adjust_x_ = 0.f;
            this->pose_adjust_y_ = 0.f;
            this->pose_adjust_z_ = 0.f;
            this->hand_pitch_ = 0.f;
            this->hand_yaw_ = 0.f;
            this->hand_roll_ = 0.f;
        }
        this->reset_pose_was_pressed_ = resetNow;

        if (config_.keyboard_only)
        {
            const float moveStep = moveSpeed * deltaSeconds;
            const float rotateStep = rotateSpeed * deltaSeconds;

            if (KeyDown(kc.move_right)) this->pose_adjust_x_ += moveStep;
            if (KeyDown(kc.move_left)) this->pose_adjust_x_ -= moveStep;
            if (KeyDown(kc.move_up)) this->pose_adjust_y_ += moveStep;
            if (KeyDown(kc.move_down)) this->pose_adjust_y_ -= moveStep;
            if (KeyDown(kc.move_back)) this->pose_adjust_z_ += moveStep;
            if (KeyDown(kc.move_forward)) this->pose_adjust_z_ -= moveStep;

            if (KeyDown(kc.rotate_right)) this->hand_yaw_ += rotateStep;
            if (KeyDown(kc.rotate_left)) this->hand_yaw_ -= rotateStep;
            if (KeyDown(kc.rotate_up)) this->hand_pitch_ += rotateStep;
            if (KeyDown(kc.rotate_down)) this->hand_pitch_ -= rotateStep;
            if (KeyDown(kc.rotate_roll_right)) this->hand_roll_ += rotateStep;
            if (KeyDown(kc.rotate_roll_left)) this->hand_roll_ -= rotateStep;
        }
        else if (isRightController && hasXinput)
        {
            const float spd = rc.pose_move_speed * deltaSeconds;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0)
                this->pose_adjust_x_ += spd;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0)
                this->pose_adjust_x_ -= spd;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0)
                this->pose_adjust_y_ += spd;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0)
                this->pose_adjust_y_ -= spd;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0)
                this->pose_adjust_z_ -= spd;
            if ((xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0)
                this->pose_adjust_z_ += spd;
        }

        const float adjX = isRightController ? this->pose_adjust_x_ : 0.f;
        const float adjY = isRightController ? this->pose_adjust_y_ : 0.f;
        const float adjZ = isRightController ? this->pose_adjust_z_ : 0.f;

        const XMFLOAT3 offset{controllerX + adjX, controllerY + adjY, ControllerZOffset + adjZ};
        XMFLOAT3 rotatedOffset{};
        XMStoreFloat3(&rotatedOffset,
                      XMVector3Rotate(XMLoadFloat3(&offset), XMLoadFloat4(&hmdRotation)));

        pose.vecPosition[0] = rotatedOffset.x + hmdPosition.x;
        pose.vecPosition[1] = rotatedOffset.y + hmdPosition.y;
        pose.vecPosition[2] = rotatedOffset.z + hmdPosition.z;

        if (config_.keyboard_only)
        {
            // Apply independent keyboard-controlled hand rotation on top of HMD orientation.
            const XMVECTOR handRotation =
                XMQuaternionRotationRollPitchYaw(this->hand_pitch_, this->hand_yaw_, this->hand_roll_);
            const XMVECTOR worldRotation =
                XMQuaternionNormalize(XMQuaternionMultiply(handRotation, XMLoadFloat4(&hmdRotation)));
            XMFLOAT4 q{};
            XMStoreFloat4(&q, worldRotation);
            pose.qRotation.x = q.x;
            pose.qRotation.y = q.y;
            pose.qRotation.z = q.z;
            pose.qRotation.w = q.w;
        }
        else
        {
            // Original behaviour: controllers inherit HMD rotation.
            pose.qRotation.w = hmdRotation.w;
            pose.qRotation.x = hmdRotation.x;
            pose.qRotation.y = hmdRotation.y;
            pose.qRotation.z = hmdRotation.z;
        }
    }

    auto updateButtonState = [&](vr::VRInputComponentHandle_t clickComponent,
                                 vr::VRInputComponentHandle_t touchComponent, bool pressed) {
        GetDriver()->GetInput()->UpdateBooleanComponent(clickComponent, pressed, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(touchComponent, pressed, 0);
    };

    auto updateScalarState = [&](vr::VRInputComponentHandle_t component, float value) {
        GetDriver()->GetInput()->UpdateScalarComponent(component, value, 0);
    };

    auto gamepadButtonPressed = [&](WORD buttonMask) {
        return hasXinput && (xinputState.Gamepad.wButtons & buttonMask) != 0;
    };

    float triggerValue = 0.0f;
    float gripValue = 0.0f;
    float joystickX = 0.0f;
    float joystickY = 0.0f;
    bool joystickClick = false;
    bool systemPressed = false;

    if (config_.keyboard_only)
    {
        const auto &k = isLeftController ? lc.keyboard : rc.keyboard;

        updateButtonState(this->a_button_click_component_, this->a_button_touch_component_,
                          KeyDown(k.a));
        updateButtonState(this->b_button_click_component_, this->b_button_touch_component_,
                          KeyDown(k.b));
        updateButtonState(this->x_button_click_component_, this->x_button_touch_component_,
                          KeyDown(k.x));
        updateButtonState(this->y_button_click_component_, this->y_button_touch_component_,
                          KeyDown(k.y));

        triggerValue = KeyDown(k.trigger) ? 1.0f : 0.0f;
        gripValue = KeyDown(k.grip) ? 1.0f : 0.0f;
        joystickClick = KeyDown(k.joystick_click);
        systemPressed = KeyDown(k.system);

        if (KeyDown(k.joystick_right)) joystickX += 1.0f;
        if (KeyDown(k.joystick_left)) joystickX -= 1.0f;
        if (KeyDown(k.joystick_up)) joystickY += 1.0f;
        if (KeyDown(k.joystick_down)) joystickY -= 1.0f;
    }
    else if (hasXinput)
    {
        // Preserve the original XInput behaviour when keyboard_only is false.
        updateButtonState(this->a_button_click_component_, this->a_button_touch_component_,
                          isRightController &&
                              (KeyDown(rc.key_a) || gamepadButtonPressed(rc.btn_a)));
        updateButtonState(this->b_button_click_component_, this->b_button_touch_component_,
                          isRightController &&
                              (KeyDown(rc.key_b) || gamepadButtonPressed(rc.btn_b)));
        updateButtonState(this->x_button_click_component_, this->x_button_touch_component_, false);
        updateButtonState(this->y_button_click_component_, this->y_button_touch_component_, false);

        if (isLeftController)
        {
            joystickClick = gamepadButtonPressed(lc.btn_joystick_click);
        }
        else if (isRightController)
        {
            triggerValue = NormalizeTrigger(xinputState.Gamepad.bRightTrigger);
            gripValue = gamepadButtonPressed(rc.btn_grip) ? 1.0f : 0.0f;
            joystickClick = gamepadButtonPressed(rc.btn_joystick_click);
            systemPressed = gamepadButtonPressed(rc.btn_system);
            joystickX = NormalizeThumbAxis(xinputState.Gamepad.sThumbRX,
                                           XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
            joystickY = NormalizeThumbAxis(xinputState.Gamepad.sThumbRY,
                                           XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        }
    }
    else
    {
        updateButtonState(this->a_button_click_component_, this->a_button_touch_component_, false);
        updateButtonState(this->b_button_click_component_, this->b_button_touch_component_, false);
        updateButtonState(this->x_button_click_component_, this->x_button_touch_component_, false);
        updateButtonState(this->y_button_click_component_, this->y_button_touch_component_, false);
    }

    const float triggerClickThreshold = rc.trigger_click_threshold;

    GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_click_component_,
                                                    triggerValue >= triggerClickThreshold, 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_touch_component_,
                                                    triggerValue > 0.0f, 0);
    updateScalarState(this->trigger_value_component_, triggerValue);

    GetDriver()->GetInput()->UpdateBooleanComponent(this->grip_touch_component_, gripValue > 0.0f,
                                                    0);
    updateScalarState(this->grip_value_component_, gripValue);
    updateScalarState(this->grip_force_component_, gripValue);

    GetDriver()->GetInput()->UpdateBooleanComponent(this->system_click_component_, systemPressed,
                                                    0);
    GetDriver()->GetInput()->UpdateBooleanComponent(this->system_touch_component_, systemPressed,
                                                    0);

    GetDriver()->GetInput()->UpdateBooleanComponent(this->trackpad_click_component_, false, 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(this->trackpad_touch_component_, false, 0);
    updateScalarState(this->trackpad_x_component_, 0.0f);
    updateScalarState(this->trackpad_y_component_, 0.0f);

    GetDriver()->GetInput()->UpdateBooleanComponent(this->joystick_click_component_, joystickClick,
                                                    0);
    GetDriver()->GetInput()->UpdateBooleanComponent(
        this->joystick_touch_component_,
        joystickClick || (std::abs(joystickX) > JoystickTouchThreshold ||
                          std::abs(joystickY) > JoystickTouchThreshold),
        0);
    updateScalarState(this->joystick_x_component_, joystickX);
    updateScalarState(this->joystick_y_component_, joystickY);
    ApplyXInputRumble(hasXinput);

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose,
                                                           sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

ControllerDevice::Handedness ControllerDevice::GetHandedness()
{
    return this->handedness_;
}

vr::TrackedDeviceIndex_t ControllerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError ControllerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating controller " + this->serial_);

    // Get the properties handle
    auto props =
        GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Setup inputs and outputs
    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic",
                                                   &this->haptic_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/click",
                                                    &this->a_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/touch",
                                                    &this->a_button_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/click",
                                                    &this->b_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/touch",
                                                    &this->b_button_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/click",
                                                    &this->x_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/touch",
                                                    &this->x_button_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/click",
                                                    &this->y_button_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/touch",
                                                    &this->y_button_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/click",
                                                    &this->trigger_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/touch",
                                                    &this->trigger_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trigger/value", &this->trigger_value_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/grip/touch",
                                                    &this->grip_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/grip/value", &this->grip_value_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/grip/force", &this->grip_force_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click",
                                                    &this->system_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch",
                                                    &this->system_touch_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/click",
                                                    &this->trackpad_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/touch",
                                                    &this->trackpad_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trackpad/x", &this->trackpad_x_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/trackpad/y", &this->trackpad_y_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/click",
                                                    &this->joystick_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/touch",
                                                    &this->joystick_touch_component_);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/joystick/x", &this->joystick_x_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    GetDriver()->GetInput()->CreateScalarComponent(
        props, "/input/joystick/y", &this->joystick_y_component_,
        vr::EVRScalarType::VRScalarType_Absolute,
        vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);

    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,
                                                    "openvr-emulator_controller");

    // Use SteamVR's built-in Oculus Touch Plus render models when available locally.
    const std::string renderModelName = this->handedness_ == Handedness::LEFT
                                            ? "oculus_quest_plus_controller_left"
                                            : "oculus_quest_plus_controller_right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,
                                                    renderModelName.c_str());

    // Give SteamVR a hint at what hand this controller is for
    if (this->handedness_ == Handedness::LEFT)
    {
        GetDriver()->GetProperties()->SetInt32Property(
            props, vr::Prop_ControllerRoleHint_Int32,
            vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
    }
    else if (this->handedness_ == Handedness::RIGHT)
    {
        GetDriver()->GetProperties()->SetInt32Property(
            props, vr::Prop_ControllerRoleHint_Int32,
            vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
    }
    else
    {
        GetDriver()->GetProperties()->SetInt32Property(
            props, vr::Prop_ControllerRoleHint_Int32,
            vr::ETrackedControllerRole::TrackedControllerRole_OptOut);
    }

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_InputProfilePath_String,
        "{openvr-emulator}/input/openvr-emulator_controller_bindings.json");

    // Change the icon depending on which handedness this controller is using (ANY uses right)
    const std::string controllerHandednessStr =
        this->handedness_ == Handedness::LEFT ? "left" : "right";
    const std::string controllerReadyFile =
        "{openvr-emulator}/icons/controller_ready_" + controllerHandednessStr + ".png";
    const std::string controllerNotReadyFile =
        "{openvr-emulator}/icons/controller_not_ready_" + controllerHandednessStr + ".png";

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String,
                                                    controllerReadyFile.c_str());

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String,
                                                    controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearching_String, controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceReadyAlert_String, controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceNotReady_String, controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceStandby_String, controllerNotReadyFile.c_str());
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceAlertLow_String, controllerNotReadyFile.c_str());

    return vr::EVRInitError::VRInitError_None;
}

void ControllerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void ControllerDevice::EnterStandby() {}

void *OpenVREmulatorDriver::ControllerDevice::GetComponent(
    const char * /*pchComponentNameAndVersion*/)
{
    return nullptr;
}

void ControllerDevice::DebugRequest(const char * /*pchRequest*/, char *pchResponseBuffer,
                                    uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
    {
        *pchResponseBuffer = 0;
    }
}

vr::DriverPose_t ControllerDevice::GetPose()
{
    return last_pose_;
}

bool ControllerDevice::IsJoystickEnabled() const
{
    return this->joystick_enabled_;
}

}  // namespace OpenVREmulatorDriver
