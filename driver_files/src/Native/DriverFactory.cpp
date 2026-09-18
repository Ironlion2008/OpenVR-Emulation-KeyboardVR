#include "DriverFactory.hpp"

#include <Windows.h>

#include <Driver/VRDriver.hpp>
#include <sstream>
#include <thread>

namespace
{
std::shared_ptr<OpenVREmulatorDriver::IVRDriver>
    driver;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

void *HmdDriverFactory(const char *interfaceName, int *returnCode)
{
    if (std::strcmp(interfaceName, vr::IServerTrackedDeviceProvider_Version) == 0)
    {
        if (!driver)
        {
            // Instantiate concrete impl
            driver = std::make_shared<OpenVREmulatorDriver::VRDriver>();
        }
        // We always have at least 1 ref to the shared ptr in "driver" so passing out raw pointer is
        // ok
        return driver.get();
    }

    if (returnCode != nullptr)
    {
        *returnCode = vr::VRInitError_Init_InterfaceNotFound;
    }

    return nullptr;
}

std::shared_ptr<OpenVREmulatorDriver::IVRDriver> OpenVREmulatorDriver::GetDriver()
{
    return driver;
}
