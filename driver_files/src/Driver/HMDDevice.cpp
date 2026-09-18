#include "HMDDevice.hpp"

#include <Windows.h>
#include <Xinput.h>

#include <numbers>

#include "ControllerDevice.hpp"
#include "InputMath.hpp"

using namespace DirectX;

namespace
{
constexpr DWORD XInputControllerIndex = 0;
constexpr float SecondsFromVsyncToPhotons = 0.011f;
constexpr float MillisecondsPerSecond = 1000.0f;
constexpr float DisplayFrequency = 90.0f;
constexpr SHORT KeyStateMask = static_cast<SHORT>(0x8000);
}  // namespace

OpenVREmulatorDriver::HMDDevice::HMDDevice(std::string serial, InputConfig config)
    : serial_(std::move(serial)), config_(config)
{
}

std::string OpenVREmulatorDriver::HMDDevice::GetSerial()
{
    return this->serial_;
}

void OpenVREmulatorDriver::HMDDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }

    // Setup pose for this frame
    auto pose = IVRDevice::MakeDefaultPose();

    const float deltaSeconds =
        static_cast<float>(GetDriver()->GetLastFrameTime().count()) / MillisecondsPerSecond;
    const auto &hmdCfg = config_.hmd;

    const bool spaceDown = (GetAsyncKeyState(hmdCfg.key_mouse_toggle) & KeyStateMask) != 0;
    if (spaceDown && !this->space_was_down_)
    {
        this->mouse_emulation_enabled_ = !this->mouse_emulation_enabled_;
        GetDriver()->Log(std::string("Mouse emulation ") +
                         (this->mouse_emulation_enabled_ ? "enabled" : "disabled"));
    }
    this->space_was_down_ = spaceDown;

    // Get orientation
    POINT currentMousePos;
    if (GetCursorPos(&currentMousePos) != 0)
    {
        if (this->mouse_emulation_enabled_ && this->mouse_pos_valid_)
        {
            this->rot_y_ -= static_cast<float>(currentMousePos.x - this->last_mouse_x_) *
                            hmdCfg.mouse_sensitivity;
            this->rot_x_ -= static_cast<float>(currentMousePos.y - this->last_mouse_y_) *
                            hmdCfg.mouse_sensitivity;
        }
        this->last_mouse_x_ = currentMousePos.x;
        this->last_mouse_y_ = currentMousePos.y;
        this->mouse_pos_valid_ = true;
    }

    XINPUT_STATE xinputState = {};
    const bool hasXinput = XInputGetState(XInputControllerIndex, &xinputState) == ERROR_SUCCESS;

    // Left stick → HMD look (yaw / pitch)
    if (hasXinput)
    {
        this->rot_y_ -=
            NormalizeThumbAxis(xinputState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) *
            hmdCfg.look_speed * deltaSeconds;
        this->rot_x_ +=
            NormalizeThumbAxis(xinputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) *
            hmdCfg.look_speed * deltaSeconds;
    }

    this->rot_x_ = std::fmax(this->rot_x_, -std::numbers::pi_v<float> / 2);
    this->rot_x_ = std::fmin(this->rot_x_, std::numbers::pi_v<float> / 2);

    const XMFLOAT4 yQuatF{0, std::sinf(this->rot_y_ / 2), 0, std::cosf(this->rot_y_ / 2)};
    const XMFLOAT4 xQuatF{std::sinf(this->rot_x_ / 2), 0, 0, std::cosf(this->rot_x_ / 2)};
    const XMVECTOR poseRot = XMQuaternionMultiply(XMLoadFloat4(&xQuatF), XMLoadFloat4(&yQuatF));

    XMFLOAT4 poseRotF{};
    XMStoreFloat4(&poseRotF, poseRot);
    pose.qRotation.w = poseRotF.w;
    pose.qRotation.x = poseRotF.x;
    pose.qRotation.y = poseRotF.y;
    pose.qRotation.z = poseRotF.z;

    // Left trigger alone → move forward; left trigger + LB → move backward
    if (hasXinput)
    {
        const float ltValue = NormalizeTrigger(xinputState.Gamepad.bLeftTrigger);
        if (ltValue > 0.0f)
        {
            const bool lbPressed =
                (xinputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
            const float sign = lbPressed ? 1.0f : -1.0f;
            const XMFLOAT3 moveDirF{0, 0, sign * ltValue * hmdCfg.move_speed * deltaSeconds};
            XMFLOAT3 rotatedF{};
            XMStoreFloat3(&rotatedF, XMVector3Rotate(XMLoadFloat3(&moveDirF), poseRot));
            this->pos_x_ += rotatedF.x;
            this->pos_y_ += rotatedF.y;
            this->pos_z_ += rotatedF.z;
        }
    }

    pose.vecPosition[0] = this->pos_x_;
    pose.vecPosition[1] = this->pos_y_;
    pose.vecPosition[2] = this->pos_z_;

    // Report the emulated HMD as being worn so SteamVR doesn't sleep it.
    if (this->proximity_component_ != 0)
    {
        GetDriver()->GetInput()->UpdateBooleanComponent(this->proximity_component_, true, 0);
    }

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose,
                                                           sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType OpenVREmulatorDriver::HMDDevice::GetDeviceType()
{
    return DeviceType::HMD;
}

vr::TrackedDeviceIndex_t OpenVREmulatorDriver::HMDDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OpenVREmulatorDriver::HMDDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;
    POINT currentMousePos;
    this->mouse_pos_valid_ = GetCursorPos(&currentMousePos) == TRUE;
    if (this->mouse_pos_valid_)
    {
        this->last_mouse_x_ = currentMousePos.x;
        this->last_mouse_y_ = currentMousePos.y;
    }

    GetDriver()->Log("Activating HMD " + this->serial_);

    // Load settings values
    // Could probably make this cleaner with making a wrapper class
    try
    {
        const int windowX = std::get<int>(GetDriver()->GetSettingsValue("window_x"));
        if (windowX > 0)
        {
            this->window_x_ = windowX;
        }
    }
    catch (const std::bad_variant_access &)  // NOLINT(bugprone-empty-catch)
    {
        // Setting not found or wrong type; keep the default value.
    };

    try
    {
        const int windowY = std::get<int>(GetDriver()->GetSettingsValue("window_y"));
        if (windowY > 0)
        {
            this->window_y_ = windowY;
        }
    }
    catch (const std::bad_variant_access &)  // NOLINT(bugprone-empty-catch)
    {
        // Setting not found or wrong type; keep the default value.
    };

    try
    {
        const int windowWidth = std::get<int>(GetDriver()->GetSettingsValue("window_width"));
        if (windowWidth > 0)
        {
            this->window_width_ = windowWidth;
        }
    }
    catch (const std::bad_variant_access &)  // NOLINT(bugprone-empty-catch)
    {
        // Setting not found or wrong type; keep the default value.
    };

    try
    {
        const int windowHeight = std::get<int>(GetDriver()->GetSettingsValue("window_height"));
        if (windowHeight > 0)
        {
            this->window_height_ = windowHeight;
        }
    }
    catch (const std::bad_variant_access &)  // NOLINT(bugprone-empty-catch)
    {
        // Setting not found or wrong type; keep the default value.
    };

    // Get the properties handle
    auto props =
        GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/proximity",
                                                    &this->proximity_component_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);

    // Set the IPD to be whatever steam has configured
    GetDriver()->GetProperties()->SetFloatProperty(
        props, vr::Prop_UserIpdMeters_Float,
        vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float));
    GetDriver()->GetProperties()->SetFloatProperty(props, vr::Prop_UserHeadToEyeDepthMeters_Float,
                                                   0.0f);

    // Set the display FPS
    GetDriver()->GetProperties()->SetFloatProperty(props, vr::Prop_DisplayFrequency_Float,
                                                   DisplayFrequency);
    GetDriver()->GetProperties()->SetFloatProperty(props, vr::Prop_SecondsFromVsyncToPhotons_Float,
                                                   SecondsFromVsyncToPhotons);

    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,
                                                    "OPENVR-EMULATOR_HMD_DEVICE");

    // Set up icon paths
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String,
                                                    "{openvr-emulator}/icons/hmd_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceSearching_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearchingAlert_String,
        "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceReadyAlert_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceNotReady_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceStandby_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props,
                                                    vr::Prop_NamedIconPathDeviceAlertLow_String,
                                                    "{openvr-emulator}/icons/hmd_not_ready.png");

    GetDriver()->GetProperties()->SetBoolProperty(props, vr::Prop_HasDisplayComponent_Bool, true);
    GetDriver()->GetProperties()->SetBoolProperty(props, vr::Prop_DeviceCanPowerOff_Bool, false);
    GetDriver()->GetProperties()->SetBoolProperty(props, vr::Prop_IsOnDesktop_Bool, false);
    GetDriver()->GetProperties()->SetBoolProperty(props, vr::Prop_DisplayDebugMode_Bool, true);

    return vr::EVRInitError::VRInitError_None;
}

void OpenVREmulatorDriver::HMDDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OpenVREmulatorDriver::HMDDevice::EnterStandby() {}

void *OpenVREmulatorDriver::HMDDevice::GetComponent(const char *pchComponentNameAndVersion)
{
    if (_stricmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version) == 0)
    {
        return static_cast<vr::IVRDisplayComponent *>(this);
    }
    return nullptr;
}

void OpenVREmulatorDriver::HMDDevice::DebugRequest(const char * /*pchRequest*/,
                                                   char *pchResponseBuffer,
                                                   uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
    {
        *pchResponseBuffer = 0;
    }
}

vr::DriverPose_t OpenVREmulatorDriver::HMDDevice::GetPose()
{
    return this->last_pose_;
}

// NOLINT(bugprone-easily-swappable-parameters): OpenVR API signature
void OpenVREmulatorDriver::HMDDevice::GetWindowBounds(
    int32_t *pnX, int32_t *pnY,
    uint32_t *pnWidth,  // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t *pnHeight)
{
    *pnX = static_cast<int32_t>(this->window_x_);
    *pnY = static_cast<int32_t>(this->window_y_);
    *pnWidth = this->window_width_;
    *pnHeight = this->window_height_;
}

bool OpenVREmulatorDriver::HMDDevice::IsDisplayOnDesktop()
{
    return true;
}

bool OpenVREmulatorDriver::HMDDevice::IsDisplayRealDisplay()
{
    return false;
}

// NOLINT(bugprone-easily-swappable-parameters): OpenVR API signature
void OpenVREmulatorDriver::HMDDevice::GetRecommendedRenderTargetSize(
    uint32_t *pnWidth,  // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t *pnHeight)
{
    *pnWidth = this->window_width_ / 2;
    *pnHeight = this->window_height_;
}

// NOLINT(bugprone-easily-swappable-parameters): OpenVR API signature
void OpenVREmulatorDriver::HMDDevice::GetEyeOutputViewport(
    vr::EVREye eEye, uint32_t *pnX,
    uint32_t *pnY,  // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t *pnWidth, uint32_t *pnHeight)
{
    const uint32_t eyeWidth = this->window_width_ / 2;

    *pnY = 0;
    *pnWidth = eyeWidth;
    *pnHeight = this->window_height_;

    if (eEye == vr::EVREye::Eye_Left)
    {
        *pnX = 0;
    }
    else
    {
        *pnX = eyeWidth;
    }
}

// NOLINT(bugprone-easily-swappable-parameters): OpenVR API signature
void OpenVREmulatorDriver::HMDDevice::GetProjectionRaw(
    vr::EVREye /*eEye*/, float *pfLeft,  // NOLINT(bugprone-easily-swappable-parameters)
    float *pfRight, float *pfTop, float *pfBottom)
{
    const float eyeAspect =
        static_cast<float>(this->window_width_) / 2.0f / static_cast<float>(this->window_height_);

    *pfLeft = -eyeAspect;
    *pfRight = eyeAspect;
    *pfTop = -1;
    *pfBottom = 1;
}

vr::DistortionCoordinates_t OpenVREmulatorDriver::HMDDevice::ComputeDistortion(vr::EVREye /*eEye*/,
                                                                               float fU, float fV)
{
    vr::DistortionCoordinates_t coordinates{};
    coordinates.rfBlue[0] = fU;
    coordinates.rfBlue[1] = fV;
    coordinates.rfGreen[0] = fU;
    coordinates.rfGreen[1] = fV;
    coordinates.rfRed[0] = fU;
    coordinates.rfRed[1] = fV;
    return coordinates;
}

bool OpenVREmulatorDriver::HMDDevice::ComputeInverseDistortion(
    vr::HmdVector2_t* pResult,
    vr::EVREye eEye,
    uint32_t unChannel,
    float fU,
    float fV
)
{
    if (!pResult)
        return false;

    // This emulator does not apply lens distortion.
    // Return the input coordinates unchanged.
    pResult->v[0] = fU;
    pResult->v[1] = fV;

    return true;
}
