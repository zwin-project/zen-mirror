#include "pch.h"

#include "logger.h"
#include "openxr-program.h"

namespace zen::display_system::oculus {

bool
OpenXRProgram::Initialize() const
{
  if (!InitializeLoader()) LOG_ERROR("Failed to initialize OpenXR");
  return true;
}

bool
OpenXRProgram::InitializeLoader() const
{
  PFN_xrInitializeLoaderKHR InitializeLoader = nullptr;
  auto res = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
      (PFN_xrVoidFunction *)(&InitializeLoader));
  if (XR_FAILED(res)) {
    LOG_ERROR("Failed to get xrInitializeLoaderKHR proc address");
    return false;
  }

  XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
  memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
  loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
  loaderInitInfoAndroid.next = nullptr;
  loaderInitInfoAndroid.applicationVM = app_->activity->vm;
  loaderInitInfoAndroid.applicationContext = app_->activity->clazz;
  InitializeLoader(
      (const XrLoaderInitInfoBaseHeaderKHR *)&loaderInitInfoAndroid);

  return true;
}

}  // namespace zen::display_system::oculus
