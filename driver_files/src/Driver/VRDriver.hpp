#pragma once

#include <openvr_driver.h>

#include <Driver/IVRDevice.hpp>
#include <Driver/IVRDriver.hpp>
#include <memory>
#include <vector>

namespace OpenVREmulatorDriver
{
class VRDriver : public IVRDriver
{
public:
    // Inherited via IVRDriver
    std::vector<std::shared_ptr<IVRDevice>> GetDevices() override;
    std::vector<vr::VREvent_t> GetOpenVREvents() override;
    std::chrono::milliseconds GetLastFrameTime() override;
    bool AddDevice(std::shared_ptr<IVRDevice> device) override;
    SettingsValue GetSettingsValue(std::string key) override;
    void Log(std::string message) override;

    vr::IVRDriverInput *GetInput() override;
    vr::CVRPropertyHelpers *GetProperties() override;
    vr::IVRServerDriverHost *GetDriverHost() override;

    // Inherited via IServerTrackedDeviceProvider
    vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override; // NOLINT(misc-override-with-different-visibility)
    void Cleanup() override; // NOLINT(misc-override-with-different-visibility)
    void RunFrame() override; // NOLINT(misc-override-with-different-visibility)
    bool ShouldBlockStandbyMode() override; // NOLINT(misc-override-with-different-visibility)
    void EnterStandby() override; // NOLINT(misc-override-with-different-visibility)
    void LeaveStandby() override; // NOLINT(misc-override-with-different-visibility)
    ~VRDriver() override = default; // NOLINT(cppcoreguidelines-special-member-functions)

private:
    static constexpr int FrameTimingMs = 16;

    std::vector<std::shared_ptr<IVRDevice>> devices_;
    std::vector<vr::VREvent_t> openvr_events_;
    std::chrono::milliseconds frame_timing_ = std::chrono::milliseconds(FrameTimingMs);
    std::chrono::system_clock::time_point last_frame_time_ = std::chrono::system_clock::now();
    std::string settings_key_ = "driver_example";
};
};  // namespace OpenVREmulatorDriver
