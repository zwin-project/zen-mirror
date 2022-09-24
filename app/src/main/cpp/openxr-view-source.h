#pragma once

#include "loop.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRViewSource : public Loop::ISource {
 public:
  struct Swapchain;

  DISABLE_MOVE_AND_COPY(OpenXRViewSource);
  OpenXRViewSource(
      std::shared_ptr<OpenXRContext> context, std::shared_ptr<Loop> loop)
      : context_(std::move(context)), loop_(std::move(loop))
  {
  }
  ~OpenXRViewSource();

  /* Allocate view buffer and create a swapchain for each view */
  bool Init();
  void Process() override;

 private:
  std::shared_ptr<OpenXRContext> context_;
  std::shared_ptr<Loop> loop_;

  /**
   * The following vectors are of the same size, and items at the same index
   * correspond to each other.
   */
  std::vector<XrView> views_;  // resized properly when initialized
  std::vector<Swapchain> swapchains_;
};

struct OpenXRViewSource::Swapchain {
  int32_t width;
  int32_t height;
  XrSwapchain handle;
  std::vector<XrSwapchainImageOpenGLESKHR> images;
};

}  // namespace zen::display_system::oculus
