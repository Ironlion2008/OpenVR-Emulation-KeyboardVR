#pragma once

#include <Driver/IVRDevice.hpp>
#include <Driver/InputConfig.hpp>
#include <Native/DriverFactory.hpp>
#include <chrono>
#include <cmath>

namespace OpenVREmulatorDriver
{
class HMDDevice : public IVRDevice, public vr::IVRDisplayComponent
{
public:
    explicit HMDDevice(std::string serial, InputConfig config = InputConfig::Defaults());
    ~HMDDevice() override = default; // NOLINT(cppcoreguidelines-special-member-functions)

    // Inherited via IVRDevice
    std::string GetSerial() override;
    void Update() override;
    vr::TrackedDeviceIndex_t GetDeviceIndex() override;
    DeviceType GetDeviceType() override;

    vr::EVRInitError Activate(uint32_t unObjectId) override;
    void Deactivate() override;
    void EnterStandby() override;
    void *GetComponent(const char *pchComponentNameAndVersion) override;
    void DebugRequest(const char *pchRequest, char *pchResponseBuffer,
                      uint32_t unResponseBufferSize) override;
    vr::DriverPose_t GetPose() override;

    // Inherited via IVRDisplayComponent
    void GetWindowBounds(int32_t *pnX, int32_t *pnY, uint32_t *pnWidth,
                         uint32_t *pnHeight) override;
    bool IsDisplayOnDesktop() override;
    bool IsDisplayRealDisplay() override;
    void GetRecommendedRenderTargetSize(uint32_t *pnWidth, uint32_t *pnHeight) override;
    void GetEyeOutputViewport(vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth,
                              uint32_t *pnHeight) override;
    void GetProjectionRaw(vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop,
                          float *pfBottom) override;
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eEye, float fU, float fV) override;
    bool ComputeInverseDistortion(
        vr::HmdVector2_t* pResult,
        vr::EVREye eEye,
        uint32_t unChannel,
        float fU,
        float fV
    ) override;

private:
    static constexpr uint32_t DefaultWindowWidth = 1920U;
    static constexpr uint32_t DefaultWindowHeight = 1080U;

    vr::TrackedDeviceIndex_t device_index_ = vr::k_unTrackedDeviceIndexInvalid;
    std::string serial_;
    InputConfig config_;

    vr::DriverPose_t last_pose_ = IVRDevice::MakeDefaultPose();

    uint32_t window_x_ = 0;
    uint32_t window_y_ = 0;
    uint32_t window_width_ = DefaultWindowWidth;
    uint32_t window_height_ = DefaultWindowHeight;

    float pos_x_ = 0, pos_y_ = 0, pos_z_ = 0;
    float rot_y_ = 0, rot_x_ = 0;
    long last_mouse_x_ = 0;
    long last_mouse_y_ = 0;
    bool mouse_pos_valid_ = false;
    bool mouse_emulation_enabled_ = true;
    bool space_was_down_ = false;

    vr::VRInputComponentHandle_t proximity_component_ = 0;
};
};  // namespace OpenVREmulatorDriver
