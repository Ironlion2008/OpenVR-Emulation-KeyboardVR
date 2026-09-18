#pragma once

#include <openvr_driver.h>

#include <Driver/IVRDriver.hpp>
#include <cstdlib>
#include <memory>

extern "C" __declspec(dllexport) void *HmdDriverFactory(const char *interfaceName, int *returnCode);

namespace OpenVREmulatorDriver
{
std::shared_ptr<OpenVREmulatorDriver::IVRDriver> GetDriver();
}
