#include "TrackingReferenceDevice.hpp"

#include <Windows.h>

#include <numbers>
#include <random>

#include "InputMath.hpp"

using namespace DirectX;

namespace OpenVREmulatorDriver
{

TrackingReferenceDevice::TrackingReferenceDevice(std::string serial)
    : serial_(std::move(serial)), random_angle_rad_([&] {
          std::mt19937 rng{std::random_device{}()};
          const float twoPi = std::numbers::pi_v<float> + std::numbers::pi_v<float>;
          return std::uniform_real_distribution<float>{0.0f, twoPi}(rng);
      }())
{
}

std::string TrackingReferenceDevice::GetSerial()
{
    return this->serial_;
}

void TrackingReferenceDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
    {
        return;
    }

    // Setup pose for this frame
    auto pose = IVRDevice::MakeDefaultPose();

    const XMFLOAT3 devicePositionF{0.f, 1.f, 1.f};

    const XMFLOAT4 yQuatF{0, std::sinf(this->random_angle_rad_ / 2), 0,
                          std::cosf(this->random_angle_rad_ / 2)};  // Point inwards (z- is forward)

    const XMFLOAT4 xLookDownF{
        std::sinf((-std::numbers::pi_v<float> / 4) / 2), 0, 0,
        std::cosf((-std::numbers::pi_v<float> / 4) / 2)};  // Tilt downwards to look at the centre

    XMFLOAT4 deviceRotationF{};
    XMStoreFloat4(&deviceRotationF,
                  XMQuaternionMultiply(XMLoadFloat4(&yQuatF), XMLoadFloat4(&xLookDownF)));

    XMFLOAT3 rotatedPos{};
    XMStoreFloat3(&rotatedPos,
                  XMVector3Rotate(XMLoadFloat3(&devicePositionF), XMLoadFloat4(&yQuatF)));

    pose.vecPosition[0] = rotatedPos.x;
    pose.vecPosition[1] = rotatedPos.y;
    pose.vecPosition[2] = rotatedPos.z;

    pose.qRotation.w = deviceRotationF.w;
    pose.qRotation.x = deviceRotationF.x;
    pose.qRotation.y = deviceRotationF.y;
    pose.qRotation.z = deviceRotationF.z;

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose,
                                                           sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType TrackingReferenceDevice::GetDeviceType()
{
    return DeviceType::TrackingReference;
}

vr::TrackedDeviceIndex_t TrackingReferenceDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError TrackingReferenceDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracking reference " + this->serial_);

    // Get the properties handle
    auto props =
        GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);

    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String,
                                                    "openvr-emulator_trackingreference");

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,
                                                    "locator");

    // Set the icons
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceReady_String,
        "{openvr-emulator}/icons/trackingreference_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceOff_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearching_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceSearchingAlert_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceReadyAlert_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceNotReady_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceStandby_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(
        props, vr::Prop_NamedIconPathDeviceAlertLow_String,
        "{openvr-emulator}/icons/trackingreference_not_ready.png");

    return vr::EVRInitError::VRInitError_None;
}

void TrackingReferenceDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void TrackingReferenceDevice::EnterStandby() {}

void *TrackingReferenceDevice::GetComponent(const char * /*pchComponentNameAndVersion*/)
{
    return nullptr;
}

void TrackingReferenceDevice::DebugRequest(const char * /*pchRequest*/, char *pchResponseBuffer,
                                           uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
    {
        *pchResponseBuffer = 0;
    }
}

vr::DriverPose_t TrackingReferenceDevice::GetPose()
{
    return last_pose_;
}

}  // namespace OpenVREmulatorDriver
