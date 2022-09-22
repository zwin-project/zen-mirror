#pragma once

#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRViewConfig {
 public:
  struct Swapchain;

  DISABLE_MOVE_AND_COPY(OpenXRViewConfig);
  OpenXRViewConfig(std::shared_ptr<OpenXRContext> context) : context_(context)
  {
  }
  ~OpenXRViewConfig() = default;

  /* Allocate view buffer and create a swapchain for each view */
  bool Init();

 private:
  std::shared_ptr<OpenXRContext> context_;

  /**
   * The following vectors are of the same size, and items at the same index
   * correspond to each other.
   */
  std::vector<XrView> views_;  // resized properly when initialized
  std::vector<Swapchain> swapchains_;
};

struct OpenXRViewConfig::Swapchain {
  int32_t width;
  int32_t height;
  XrSwapchain handle;
  std::vector<XrSwapchainImageOpenGLESKHR> images;
};

}  // namespace zen::display_system::oculus
