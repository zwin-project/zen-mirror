#pragma once

namespace zen::display_system::oculus {

struct OpenXRContext {
  XrInstance instance{XR_NULL_HANDLE};
  XrSystemId system_id{XR_NULL_SYSTEM_ID};
  XrViewConfigurationType view_configuration_type{};
  XrEnvironmentBlendMode environment_blend_mode{};

  static const XrViewConfigurationType kAcceptableViewConfigType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  static const XrEnvironmentBlendMode kAcceptableEnvironmentBlendModeType =
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
};

}  // namespace zen::display_system::oculus
