#include "pch.h"

#include "logger.h"
#include "openxr-util.h"
#include "openxr-view-config.h"

namespace zen::display_system::oculus {

bool
OpenXRViewConfig::Init()
{
  CHECK(context_->instance != XR_NULL_HANDLE);
  CHECK(context_->system_id != XR_NULL_SYSTEM_ID);
  CHECK(context_->session != XR_NULL_HANDLE);
  CHECK(context_->egl);

  XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
  IF_XR_FAILED (err, xrGetSystemProperties(context_->instance,
                         context_->system_id, &system_properties)) {
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
        xrEnumerateViewConfigurationViews(context_->instance,
            context_->system_id, context_->view_configuration_type, 0,
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
        xrEnumerateViewConfigurationViews(context_->instance,
            context_->system_id, context_->view_configuration_type, view_count,
            &view_count, config_views.data())) {
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

    IF_XR_FAILED (err, xrEnumerateSwapchainFormats(context_->session, 0,
                           &swapchain_format_count, nullptr)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    std::vector<int64_t> swapchain_formats(swapchain_format_count);

    IF_XR_FAILED (err,
        xrEnumerateSwapchainFormats(context_->session, swapchain_format_count,
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
    LOG_DEBUG(
        "Creating swapchain for view %d with dimensions Width=%d Height=%d "
        "SampleCount=%d",
        i, config_view.recommendedImageRectWidth,
        config_view.recommendedImageRectHeight,
        config_view.recommendedSwapchainSampleCount);

    XrSwapchainCreateInfo swapchain_create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchain_create_info.arraySize = 1;
    swapchain_create_info.format = color_swapchain_format;
    swapchain_create_info.width = config_view.recommendedImageRectWidth;
    swapchain_create_info.height = config_view.recommendedImageRectHeight;
    swapchain_create_info.mipCount = 1;
    swapchain_create_info.faceCount = 1;
    swapchain_create_info.sampleCount =
        config_view.recommendedSwapchainSampleCount;
    swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    OpenXRViewConfig::Swapchain swapchain;
    swapchain.width = config_view.recommendedImageRectWidth;
    swapchain.height = config_view.recommendedImageRectHeight;
    IF_XR_FAILED (err, xrCreateSwapchain(context_->session,
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

    swapchains_.push_back(swapchain);
  }

  // Resize view buffer for xrLocateViews later.
  views_.resize(view_count, {XR_TYPE_VIEW});

  return true;
}

}  // namespace zen::display_system::oculus
