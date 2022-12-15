#pragma once

#include "loop.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRViewSource : public Loop::ISource {
 public:
  struct Swapchain;
  struct SwapchainFramebuffer;

  DISABLE_MOVE_AND_COPY(OpenXRViewSource);
  OpenXRViewSource(std::shared_ptr<OpenXRContext> context,
      std::shared_ptr<Loop> loop,
      std::shared_ptr<zen::remote::client::IRemote> remote)
      : context_(std::move(context)),
        loop_(std::move(loop)),
        remote_(std::move(remote))
  {
  }
  ~OpenXRViewSource();

  /* Allocate view buffer and create a swapchain for each view */
  bool Init();
  void Process() override;

 private:
  /**
   * @returns false when the views should not be rendered.
   */
  bool RenderViews(XrTime predict_display_time,
      std::vector<XrCompositionLayerProjectionView>& projection_layer_views);

  std::shared_ptr<OpenXRContext> context_;
  std::shared_ptr<Loop> loop_;
  std::shared_ptr<zen::remote::client::IRemote> remote_;

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
  std::vector<SwapchainFramebuffer> framebuffers;
};

struct OpenXRViewSource::SwapchainFramebuffer {
  ~SwapchainFramebuffer();
  GLuint framebuffer = 0;
  GLuint color_texture = 0;
  GLuint depth_buffer = 0;
  GLuint resolve_framebuffer = 0;
};

}  // namespace zen::display_system::oculus
