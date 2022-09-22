#pragma once

#include "common.h"
#include "egl-instance.h"

namespace zen::display_system::oculus {

class OpenXRContext {
 public:
  DISABLE_MOVE_AND_COPY(OpenXRContext);
  OpenXRContext() = default;
  ~OpenXRContext() = default;

  /* Initialize OpenXRContext */
  bool Init(struct android_app *app);

  inline XrInstance instance();
  inline XrSystemId system_id();
  inline XrSession session();
  inline XrViewConfigurationType view_configuration_type();
  inline XrEnvironmentBlendMode environment_blend_mode();

 private:
  /* Initialize the OpenXR loader */
  bool InitializeLoader(struct android_app *app);

  /* Create a new XrInstance and store it in the context */
  bool InitializeInstance(struct android_app *app);

  /* Get a XrSystem and store it in the context */
  bool InitializeSystem();

  /* Initialize EGL context and check OpenGL ES version */
  bool InitializeGraphicsLibrary();

  /* Create a new XrSession and store it in the context */
  bool InitializeSession();

  /* Create a new app space and store it in the context */
  bool InitializeAppSpace();

  /* Write out available view configurations, determine the view config type
   * to use and store it in the context */
  bool InitializeViewConfig();

  /* Write out available environment blend modes, determine the blend mode to
   * use and store it in the context */
  bool InitializeEnvironmentBlendMode();

  /* Write out reference spaces */
  void LogReferenceSpaces() const;

  /* Write out XrInstance info */
  void LogInstanceInfo() const;

  /** Write out
   * - non-layer extensions
   * - available layers' information and their extensions
   **/
  void LogLayersAndExtensions() const;

  static constexpr XrViewConfigurationType kAcceptableViewConfigType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  static constexpr XrEnvironmentBlendMode kAcceptableEnvironmentBlendModeType =
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

  XrInstance instance_{XR_NULL_HANDLE};
  XrSystemId system_id_{XR_NULL_SYSTEM_ID};
  XrSession session_{XR_NULL_HANDLE};
  XrSpace app_space_{XR_NULL_HANDLE};
  XrViewConfigurationType view_configuration_type_{};
  XrEnvironmentBlendMode environment_blend_mode_{};
  std::unique_ptr<EglInstance> egl_;
};

inline XrInstance
OpenXRContext::instance()
{
  return instance_;
}

inline XrSystemId
OpenXRContext::system_id()
{
  return system_id_;
}

inline XrSession
OpenXRContext::session()
{
  return session_;
}

inline XrViewConfigurationType
OpenXRContext::view_configuration_type()
{
  return view_configuration_type_;
}

inline XrEnvironmentBlendMode
OpenXRContext::environment_blend_mode()
{
  return environment_blend_mode_;
}

}  // namespace zen::display_system::oculus
