#include "pch.h"

#include "logger.h"
#include "openxr-util.h"
#include "openxr-view-source.h"

namespace zen::display_system::oculus {

constexpr float kRenderingScale = 2.f;

OpenXRViewSource::~OpenXRViewSource()
{
  for (auto swapchain : swapchains_) {
    xrDestroySwapchain(swapchain.handle);
  }
}

OpenXRViewSource::SwapchainFramebuffer::~SwapchainFramebuffer()
{
  if (framebuffer != 0) glDeleteFramebuffers(1, &framebuffer);
  if (depth_buffer != 0) glDeleteRenderbuffers(1, &depth_buffer);
}

bool
OpenXRViewSource::Init()
{
  XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
  IF_XR_FAILED (err, xrGetSystemProperties(context_->instance(),
                         context_->system_id(), &system_properties)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("System Properties: Name=%s VendorId=%d",
      system_properties.systemName, system_properties.vendorId);
  LOG_DEBUG("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
      system_properties.graphicsProperties.maxSwapchainImageWidth,
      system_properties.graphicsProperties.maxSwapchainImageHeight,
      system_properties.graphicsProperties.maxLayerCount);
  LOG_DEBUG(
      "System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
      system_properties.trackingProperties.orientationTracking == XR_TRUE
          ? "True"
          : "False",
      system_properties.trackingProperties.positionTracking == XR_TRUE
          ? "True"
          : "False");

  // Get View Configuration Views
  uint32_t view_count;
  std::vector<XrViewConfigurationView> config_views;
  {
    IF_XR_FAILED (err,
        xrEnumerateViewConfigurationViews(context_->instance(),
            context_->system_id(), context_->view_configuration_type(), 0,
            &view_count, nullptr)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    if (view_count == 0) {
      LOG_ERROR("Failed to find an available view");
      return false;
    }

    config_views.resize(view_count);

    IF_XR_FAILED (err,
        xrEnumerateViewConfigurationViews(context_->instance(),
            context_->system_id(), context_->view_configuration_type(),
            view_count, &view_count, config_views.data())) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Select Swapchain Format
  int64_t color_swapchain_format = 0;
  {
    uint32_t swapchain_format_count;
    const std::vector<int64_t> kSupportedColorSwapchainFormats{
        GL_RGBA8, GL_RGBA8_SNORM, GL_SRGB8_ALPHA8};

    IF_XR_FAILED (err, xrEnumerateSwapchainFormats(context_->session(), 0,
                           &swapchain_format_count, nullptr)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    std::vector<int64_t> swapchain_formats(swapchain_format_count);

    IF_XR_FAILED (err,
        xrEnumerateSwapchainFormats(context_->session(), swapchain_format_count,
            &swapchain_format_count, swapchain_formats.data())) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    auto swapchain_format_iterator =
        std::find_first_of(swapchain_formats.begin(), swapchain_formats.end(),
            kSupportedColorSwapchainFormats.begin(),
            kSupportedColorSwapchainFormats.end());

    if (swapchain_format_iterator == swapchain_formats.end()) {
      LOG_ERROR("No runtime swapchain format supported for color swapchain");
      return false;
    }

    color_swapchain_format = *swapchain_format_iterator;

    std::stringstream formats_string_stream;
    for (auto format : swapchain_formats) {
      const bool selected = format == color_swapchain_format;

      formats_string_stream << " ";
      if (selected) formats_string_stream << "[";
      formats_string_stream << "0x" << std::hex << format;
      if (selected) formats_string_stream << "]";
    }

    LOG_DEBUG("Swapchain Formats: %s", formats_string_stream.str().c_str());
  }

  // Create a swapchain for each view
  for (uint32_t i = 0; i < view_count; i++) {
    auto &config_view = config_views[i];
    uint32_t rendering_width =
        config_view.recommendedImageRectWidth * kRenderingScale;
    uint32_t rendering_height =
        config_view.recommendedImageRectHeight * kRenderingScale;

    if (rendering_width > config_view.maxImageRectWidth ||
        rendering_height > config_view.maxImageRectHeight) {
      rendering_width = config_view.recommendedImageRectWidth;
      rendering_height = config_view.recommendedImageRectHeight;
    }

    LOG_DEBUG(
        "Creating swapchain for view %d with dimensions Width=%d Height=%d "
        "SampleCount=%d",
        i, rendering_width, rendering_height,
        config_view.recommendedSwapchainSampleCount);

    XrSwapchainCreateInfo swapchain_create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchain_create_info.arraySize = 1;
    swapchain_create_info.format = color_swapchain_format;
    swapchain_create_info.width = rendering_width;
    swapchain_create_info.height = rendering_height;
    swapchain_create_info.mipCount = 1;
    swapchain_create_info.faceCount = 1;
    swapchain_create_info.sampleCount =
        config_view.recommendedSwapchainSampleCount;
    swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    OpenXRViewSource::Swapchain swapchain;
    swapchain.width = rendering_width;
    swapchain.height = rendering_height;
    IF_XR_FAILED (err, xrCreateSwapchain(context_->session(),
                           &swapchain_create_info, &swapchain.handle)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    uint32_t image_count;
    IF_XR_FAILED (err, xrEnumerateSwapchainImages(
                           swapchain.handle, 0, &image_count, nullptr)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    swapchain.images.resize(
        image_count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});

    IF_XR_FAILED (err,
        xrEnumerateSwapchainImages(swapchain.handle, image_count, &image_count,
            reinterpret_cast<XrSwapchainImageBaseHeader *>(
                swapchain.images.data()))) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    swapchain.framebuffers.resize(swapchain.images.size());

    for (uint i = 0; i < swapchain.images.size(); i++) {
      GLuint framebuffer, depth_buffer;

      glGenFramebuffers(1, &framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      glFramebufferTexture(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, swapchain.images[i].image, 0);

      glGenRenderbuffers(1, &depth_buffer);
      glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F,
          swapchain.width, swapchain.height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      glFramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      swapchain.framebuffers[i].framebuffer = framebuffer;
      swapchain.framebuffers[i].depth_buffer = depth_buffer;
    }

    swapchains_.emplace_back(std::move(swapchain));
  }

  // Resize view buffer for xrLocateViews later.
  views_.resize(view_count, {XR_TYPE_VIEW});

  return true;
}

void
OpenXRViewSource::Process()
{
  if (context_->is_session_running() == false) return;

  XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frame_state{XR_TYPE_FRAME_STATE};
  IF_XR_FAILED (err,
      xrWaitFrame(context_->session(), &frame_wait_info, &frame_state)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }

  if (context_->app_space() == XR_NULL_HANDLE) {
    if (!context_->InitializeAppSpace(frame_state.predictedDisplayTime)) {
      loop_->Terminate();
      return;
    }
  }

  XrFrameBeginInfo frame_begin_info{XR_TYPE_FRAME_BEGIN_INFO};
  IF_XR_FAILED (err, xrBeginFrame(context_->session(), &frame_begin_info)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }

  std::vector<XrCompositionLayerBaseHeader *> layers;
  XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  std::vector<XrCompositionLayerProjectionView> projection_layer_views;
  if (frame_state.shouldRender == XR_TRUE) {
    if (RenderViews(
            frame_state.predictedDisplayPeriod, projection_layer_views)) {
      layer.space = context_->app_space();

      if (context_->environment_blend_mode() ==
          XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) {
        layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                           XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
      } else {
        layer.layerFlags = 0;
      }

      layer.viewCount = (uint32_t)projection_layer_views.size();
      layer.views = projection_layer_views.data();

      layers.push_back(
          reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
    }
  }

  XrFrameEndInfo frame_end_info{XR_TYPE_FRAME_END_INFO};
  frame_end_info.displayTime = frame_state.predictedDisplayTime;
  frame_end_info.environmentBlendMode = context_->environment_blend_mode();
  frame_end_info.layerCount = (uint32_t)layers.size();
  frame_end_info.layers = layers.data();
  IF_XR_FAILED (err, xrEndFrame(context_->session(), &frame_end_info)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }
}

bool
OpenXRViewSource::RenderViews(XrTime predict_display_time,
    std::vector<XrCompositionLayerProjectionView> &projection_layer_views)
{
  XrViewState view_state{XR_TYPE_VIEW_STATE};
  uint32_t view_capacity_input = (uint32_t)views_.size();
  uint32_t view_count_output;

  XrViewLocateInfo view_locate_info{XR_TYPE_VIEW_LOCATE_INFO};
  view_locate_info.viewConfigurationType = context_->view_configuration_type();
  view_locate_info.displayTime = predict_display_time;
  view_locate_info.space = context_->app_space();
  IF_XR_FAILED (err,
      xrLocateViews(context_->session(), &view_locate_info, &view_state,
          view_capacity_input, &view_count_output, views_.data())) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return false;
  }

  if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return false;  // There  is no valid tracking poses for the views.
  }

  CHECK(view_count_output == view_capacity_input);
  CHECK(view_count_output == swapchains_.size());

  projection_layer_views.resize(view_count_output);

  remote_->UpdateScene();

  for (uint32_t i = 0; i < view_count_output; i++) {
    auto &swapchain = swapchains_[i];
    XrSwapchainImageAcquireInfo acquire_info{
        XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

    uint32_t swapchain_image_index;
    IF_XR_FAILED (err, xrAcquireSwapchainImage(swapchain.handle, &acquire_info,
                           &swapchain_image_index)) {
      LOG_ERROR("%s", err.c_str());
      loop_->Terminate();
      return false;
    }

    XrSwapchainImageWaitInfo swapchain_wait_info{
        XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    swapchain_wait_info.timeout = XR_INFINITE_DURATION;
    IF_XR_FAILED (err,
        xrWaitSwapchainImage(swapchain.handle, &swapchain_wait_info)) {
      LOG_ERROR("%s", err.c_str());
      loop_->Terminate();
      return false;
    }

    projection_layer_views[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    projection_layer_views[i].pose = views_[i].pose;
    projection_layer_views[i].fov = views_[i].fov;
    projection_layer_views[i].subImage.swapchain = swapchain.handle;
    projection_layer_views[i].subImage.imageRect.offset = {0, 0};
    projection_layer_views[i].subImage.imageRect.extent = {
        swapchain.width, swapchain.height};

    auto framebuffer =
        swapchain.framebuffers[swapchain_image_index].framebuffer;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    auto projection = Math::ToProjectionMatrix(views_[i].fov, 0.05, 1000.0);
    auto position = Math::ToGlm(views_[i].pose.position);
    auto orientation = Math::ToGlm(views_[i].pose.orientation);
    auto view = glm::mat4(1.0);
    view = glm::translate(view, -position);
    view = glm::toMat4(glm::inverse(orientation)) * view;

    glViewport(0, 0, swapchain.width, swapchain.height);

    glClearColor(17.f / 256.f, 31.f / 256.f, 77.f / 256.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    zen::remote::client::Camera camera;
    memcpy(&camera.view, &view, sizeof(view));
    memcpy(&camera.projection, &projection, sizeof(projection));

    remote_->Render(&camera);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    XrSwapchainImageReleaseInfo release_info{
        XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    IF_XR_FAILED (err,
        xrReleaseSwapchainImage(swapchain.handle, &release_info)) {
      LOG_ERROR("%s", err.c_str());
      loop_->Terminate();
      return false;
    }
  }

  return true;
}

}  // namespace zen::display_system::oculus
