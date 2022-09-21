#pragma once

#include "common.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRProgram {
 public:
  DISABLE_MOVE_AND_COPY(OpenXRProgram);
  OpenXRProgram() = default;
  ~OpenXRProgram() = default;

  /* Initialize the OpenXR loader */
  bool InitializeLoader(struct android_app *app) const;

  /* Initialize OpenXRContext */
  bool InitializeContext(const std::unique_ptr<OpenXRContext> &context,
      struct android_app *app) const;

 private:
  /* Create a new XrInstance and store it in the context */
  bool InitializeInstance(const std::unique_ptr<OpenXRContext> &context,
      struct android_app *app) const;

  /* Get a XrSystem and store it in the context */
  bool InitializeSystem(const std::unique_ptr<OpenXRContext> &context) const;

  /* Initialize EGL context and check OpenGL ES version */
  bool InitializeGraphicsLibrary(
      const std::unique_ptr<OpenXRContext> &context) const;

  /* Create a new XrSession and store it in the context */
  bool InitializeSession(const std::unique_ptr<OpenXRContext> &context) const;

  /* Write out available view configurations, determine the view config type
   * to use and store it in the context */
  bool InitializeViewConfig(
      const std::unique_ptr<OpenXRContext> &context) const;

  /* Write out available environment blend modes, determine the blend mode to
   * use and store it in the context */
  bool InitializeEnvironmentBlendMode(
      const std::unique_ptr<OpenXRContext> &context) const;

  /* Write out reference spaces */
  void LogReferenceSpaces(const std::unique_ptr<OpenXRContext> &context) const;

  /* Write out XrInstance info */
  void LogInstanceInfo(const std::unique_ptr<OpenXRContext> &context) const;

  /** Write out
   * - non-layer extensions
   * - available layers' information and their extensions
   **/
  void LogLayersAndExtensions() const;

  /* Convert XrVersion to a human readable version string */
  std::string GetXrVersionString(XrVersion version) const;
};

}  // namespace zen::display_system::oculus
