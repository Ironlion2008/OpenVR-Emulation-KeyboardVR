#include "TrackerDevice.hpp"

#include <Windows.h>

#include <numbers>

#include "InputMath.hpp"

using namespace DirectX;

namespace OpenVREmulatorDriver
{

namespace
{
constexpr float MillisecondsPerSecond = 1000.0f;
constexpr float VibrateYOffset = -0.35f;
constexpr float VibrateAmplitude = 0.01f;
constexpr float VibrateFrequency = 8.0f;
constexpr float TrackerZOffset = -0.5f;
}  // namespace

TrackerDevice::TrackerDevice(std::string serial) : serial_(std::move(serial)) {}

std::string TrackerDevice::GetSerial()
{
    return this->serial_;
}

void TrackerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }

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

        const XMFLOAT3 hmdPoseOffset{0.f, controllerY, TrackerZOffset};
        XMFLOAT3 rotatedOffset{};
        XMStoreFloat3(&rotatedOffset,
                      XMVector3Rotate(XMLoadFloat3(&hmdPoseOffset), XMLoadFloat4(&hmdRotation)));

        pose.vecPosition[0] = rotatedOffset.x + hmdPosition.x;
        pose.vecPosition[1] = rotatedOffset.y + hmdPosition.y;
        pose.vecPosition[2] = rotatedOffset.z + hmdPosition.z;

        pose.qRotation.w = hmdRotation.w;
        pose.qRotation.x = hmdRotation.x;
        pose.qRotation.y = hmdRotation.y;
        pose.qRotation.z = hmdRotation.z;
    }

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose,
                                                           sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    // Get the properties handle
    auto props =
        GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Setup inputs and outputs
    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic",
                                                   &this->haptic_component_);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click",
                                                    &this->system_click_component_);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch",
                                                    &this->system_touch_component_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);

    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,
                                                    "openvr-emulator_tracker");

    // Opt out of hand selection
    GetDriver()->GetProperties()->SetInt32Property(
        props, vr::Prop_ControllerRoleHint_Int32,
        vr::ETrackedControllerRole::TrackedControllerRole_OptOut);

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,
                                                    "vr_controller_05_wireless_b");

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_InputProfilePath_String,
        "{openvr-emulator}/input/openvr-emulator_tracker_bindings.json");

    // Set the icon
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String,
                                                    "{openvr-emulator}/icons/tracker_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceOff_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearching_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearchingAlert_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceReadyAlert_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceNotReady_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceStandby_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceAlertLow_String,
        "{openvr-emulator}/icons/tracker_not_ready.png");

    return vr::EVRInitError::VRInitError_None;
}

void TrackerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void TrackerDevice::EnterStandby() {}

void *TrackerDevice::GetComponent(const char * /*pchComponentNameAndVersion*/)
{
    return nullptr;
}

void TrackerDevice::DebugRequest(const char * /*pchRequest*/, char *pchResponseBuffer,
                                 uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
    {
        *pchResponseBuffer = 0;
    }
}

vr::DriverPose_t TrackerDevice::GetPose()
{
    return last_pose_;
}

}  // namespace OpenVREmulatorDriver
