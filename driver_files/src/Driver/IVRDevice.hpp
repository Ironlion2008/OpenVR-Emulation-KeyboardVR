#pragma once

#include <openvr_driver.h>

#include <Driver/DeviceType.hpp>
#include <variant>

namespace OpenVREmulatorDriver
{

class IVRDevice : public vr::ITrackedDeviceServerDriver
{
public:
    /// <summary>
    /// Returns the serial string for this device
    /// </summary>
    /// <returns>Device serial</returns>
    virtual std::string GetSerial() = 0;

    /// <summary>
    /// Runs any update logic for this device.
    /// Called once per frame
    /// </summary>
    virtual void Update() = 0;

    /// <summary>
    /// Returns the OpenVR device index
    /// This should be 0 for HMDs
    /// </summary>
    /// <returns>OpenVR device index</returns>
    virtual vr::TrackedDeviceIndex_t GetDeviceIndex() = 0;

    /// <summary>
    /// Returns which type of device this device is
    /// </summary>
    /// <returns>The type of device</returns>
    virtual DeviceType GetDeviceType() = 0;

    /// <summary>
    /// Makes a default device pose
    /// </summary>
    /// <returns>Default initialised pose</returns>
    static vr::DriverPose_t MakeDefaultPose(bool connected = true, bool tracking = true)
    {
        vr::DriverPose_t outPose = {}; // NOLINT(bugprone-invalid-enum-default-initialization)

        outPose.deviceIsConnected = connected;
        outPose.poseIsValid = tracking;
        outPose.result = tracking ? vr::ETrackingResult::TrackingResult_Running_OK
                                  : vr::ETrackingResult::TrackingResult_Running_OutOfRange;
        outPose.willDriftInYaw = false;
        outPose.shouldApplyHeadModel = false;
        outPose.qDriverFromHeadRotation.w = outPose.qWorldFromDriverRotation.w =
            outPose.qRotation.w = 1.0;

        return outPose;
    }

    // Inherited via ITrackedDeviceServerDriver
    vr::EVRInitError Activate(uint32_t unObjectId) override = 0;
    void Deactivate() override = 0;
    void EnterStandby() override = 0;
    void *GetComponent(const char *pchComponentNameAndVersion) override = 0;
    void DebugRequest(const char *pchRequest, char *pchResponseBuffer,
                      uint32_t unResponseBufferSize) override = 0;
    vr::DriverPose_t GetPose() override = 0;

    virtual ~IVRDevice() = default; // NOLINT(cppcoreguidelines-special-member-functions)
};
};  // namespace OpenVREmulatorDriver
