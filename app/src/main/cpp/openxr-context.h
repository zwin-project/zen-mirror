#pragma once

#include "egl-instance.h"

namespace zen::display_system::oculus {

struct OpenXRContext {
  friend class OpenXRProgram;

 private:
  struct Swapchain;

  XrInstance instance{XR_NULL_HANDLE};
  XrSystemId system_id{XR_NULL_SYSTEM_ID};
  XrSession session{XR_NULL_HANDLE};
  XrSpace app_space{XR_NULL_HANDLE};
  XrViewConfigurationType view_configuration_type{};
  XrEnvironmentBlendMode environment_blend_mode{};

  /**
   * The following vectors are of the samve size, and items at the same index
   * correspond to each other.
   */
  std::vector<XrView> views;  // resized properly when initialized
  std::vector<Swapchain> swapchains;

  std::unique_ptr<EglInstance> egl;

  static constexpr XrViewConfigurationType kAcceptableViewConfigType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  static constexpr XrEnvironmentBlendMode kAcceptableEnvironmentBlendModeType =
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
};

struct OpenXRContext::Swapchain {
  int32_t width;
  int32_t height;
  XrSwapchain handle;
  std::vector<XrSwapchainImageOpenGLESKHR> images;
};

}  // namespace zen::display_system::oculus
