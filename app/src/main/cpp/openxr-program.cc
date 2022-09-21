#include "pch.h"

#include "common.h"
#include "config.h"
#include "logger.h"
#include "openxr-program.h"

namespace zen::display_system::oculus {

namespace {

#define ENUM_CASE_STR(name, val) \
  case name:                     \
    return #name;

#define MAKE_TO_STRING_FUNC(enumType)            \
  inline const char *to_string(enumType e)       \
  {                                              \
    switch (e) {                                 \
      XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
                                                 \
          default : return "Unknown " #enumType; \
    }                                            \
  }

// Create other to_string function on demand
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrFormFactor);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);

std::string
CheckXrResult(XrResult res, const char *originator, const char *source_location)
{
  std::ostringstream err;
  if (XR_FAILED(res)) {
    err << "XrResult failure [" << to_string(res) << "]" << std::endl;
    err << "    Origin: " << originator << std::endl;
    err << "    Source: " << source_location;
  }

  return err.str();
}

#define IF_XR_FAILED(err, cmd) \
  if (std::string err = CheckXrResult(cmd, #cmd, FILE_AND_LINE); !err.empty())

}  // namespace

bool
OpenXRProgram::InitializeLoader(struct android_app *app) const
{
  PFN_xrInitializeLoaderKHR InitializeLoader = nullptr;
  auto res = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
      (PFN_xrVoidFunction *)(&InitializeLoader));
  if (XR_FAILED(res)) {
    LOG_ERROR("Failed to get xrInitializeLoaderKHR proc address");
    return false;
  }

  XrLoaderInitInfoAndroidKHR loader_init_info_android;
  memset(&loader_init_info_android, 0, sizeof(loader_init_info_android));
  loader_init_info_android.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
  loader_init_info_android.next = nullptr;
  loader_init_info_android.applicationVM = app->activity->vm;
  loader_init_info_android.applicationContext = app->activity->clazz;
  InitializeLoader(
      (const XrLoaderInitInfoBaseHeaderKHR *)&loader_init_info_android);

  return true;
}

bool
OpenXRProgram::InitializeContext(const std::unique_ptr<OpenXRContext> &context,
    struct android_app *app) const
{
  LogLayersAndExtensions();

  if (!InitializeInstance(context, app)) return false;

  LogInstanceInfo(context);

  if (!InitializeSystem(context)) return false;

  if (!InitializeViewConfig(context)) return false;

  if (!InitializeEnvironmentBlendMode(context)) return false;

  return true;
}

bool
OpenXRProgram::InitializeInstance(const std::unique_ptr<OpenXRContext> &context,
    struct android_app *app) const
{
  std::vector<const char *> extensions;
  extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);

  XrInstanceCreateInfo create_info{XR_TYPE_INSTANCE_CREATE_INFO};

  XrInstanceCreateInfoAndroidKHR create_info_android{
      XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  create_info_android.applicationVM = app->activity->vm;
  create_info_android.applicationActivity = app->activity->clazz;

  create_info.next = &create_info_android;
  create_info.enabledExtensionCount = (uint32_t)extensions.size();
  create_info.enabledExtensionNames = extensions.data();

  strcpy(create_info.applicationInfo.applicationName, config::APP_NAME);
  create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  IF_XR_FAILED (err, xrCreateInstance(&create_info, &context->instance)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRProgram::InitializeSystem(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->system_id == XR_NULL_SYSTEM_ID);
  const XrFormFactor kFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = kFormFactor;
  IF_XR_FAILED (err,
      xrGetSystem(context->instance, &system_info, &context->system_id)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Using system %" PRIu64 " for form factor %s", context->system_id,
      to_string(kFormFactor));

  return true;
}

bool
OpenXRProgram::InitializeViewConfig(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->instance != XR_NULL_HANDLE);
  CHECK(context->system_id != XR_NULL_SYSTEM_ID);
  bool acceptableConfigTypeFound = false;

  uint32_t view_config_type_count;
  IF_XR_FAILED (err,
      xrEnumerateViewConfigurations(context->instance, context->system_id, 0,
          &view_config_type_count, nullptr)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  std::vector<XrViewConfigurationType> view_config_types(
      view_config_type_count);

  IF_XR_FAILED (err, xrEnumerateViewConfigurations(context->instance,
                         context->system_id, view_config_type_count,
                         &view_config_type_count, view_config_types.data())) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Available View Configuration Types: (%d)", view_config_type_count);
  for (auto view_config_type : view_config_types) {
    if (view_config_type == OpenXRContext::kAcceptableViewConfigType) {
      context->view_configuration_type = view_config_type;
      acceptableConfigTypeFound = true;
    }

    LOG_DEBUG("  View Configuration Type: %s %s", to_string(view_config_type),
        view_config_type == OpenXRContext::kAcceptableViewConfigType
            ? "(selected)"
            : "");

    XrViewConfigurationProperties view_config_props{
        XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
    IF_XR_FAILED (err,
        xrGetViewConfigurationProperties(context->instance, context->system_id,
            view_config_type, &view_config_props)) {
      LOG_WARN("%s", err.c_str());
      continue;
    }

    LOG_DEBUG("  View Configuration FovMutable: %s",
        view_config_props.fovMutable == XR_TRUE ? "True" : "False");

    uint32_t view_count;
    IF_XR_FAILED (err,
        xrEnumerateViewConfigurationViews(context->instance, context->system_id,
            view_config_type, 0, &view_count, nullptr)) {
      LOG_WARN("%s", err.c_str());
      continue;
    }

    if (view_count > 0) {
      std::vector<XrViewConfigurationView> views(
          view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
      IF_XR_FAILED (err, xrEnumerateViewConfigurationViews(context->instance,
                             context->system_id, view_config_type, view_count,
                             &view_count, views.data())) {
        LOG_WARN("%s", err.c_str());
        continue;
      }

      for (uint32_t i = 0; i < views.size(); i++) {
        const XrViewConfigurationView &view = views[i];
        LOG_DEBUG(
            "    View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i,
            view.recommendedImageRectWidth, view.recommendedImageRectHeight,
            view.recommendedSwapchainSampleCount);
        LOG_DEBUG("    View [%d]: Maximum Width=%d Height=%d SampleCount=%d", i,
            view.maxImageRectWidth, view.maxImageRectHeight,
            view.maxSwapchainSampleCount);
      }
    } else {
      LOG_WARN("Empty view configuration type");
    }
  }

  if (acceptableConfigTypeFound == false) {
    LOG_ERROR("Failed to find an acceptable view configuration type");
    return false;
  }

  return true;
}

bool
OpenXRProgram::InitializeEnvironmentBlendMode(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->instance != XR_NULL_HANDLE);
  CHECK(context->system_id != XR_NULL_SYSTEM_ID);
  CHECK(context->view_configuration_type ==
        OpenXRContext::kAcceptableViewConfigType);
  bool acceptable_blend_mode_found = false;

  uint32_t count;
  IF_XR_FAILED (err,
      xrEnumerateEnvironmentBlendModes(context->instance, context->system_id,
          context->view_configuration_type, 0, &count, nullptr)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  std::vector<XrEnvironmentBlendMode> blend_modes(count);

  IF_XR_FAILED (err, xrEnumerateEnvironmentBlendModes(context->instance,
                         context->system_id, context->view_configuration_type,
                         count, &count, blend_modes.data())) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Available Environment Blend Modes for %s: (%d)",
      to_string(context->view_configuration_type), count);
  for (uint32_t i = 0; i < count; i++) {
    auto mode = blend_modes[i];
    if (mode == OpenXRContext::kAcceptableEnvironmentBlendModeType) {
      context->environment_blend_mode =
          OpenXRContext::kAcceptableEnvironmentBlendModeType;
      acceptable_blend_mode_found = true;
    }

    LOG_DEBUG("    Mode [%u]: %s %s", i, to_string(mode),
        mode == OpenXRContext::kAcceptableEnvironmentBlendModeType
            ? "(selected)"
            : "");
  }

  if (acceptable_blend_mode_found == false) {
    LOG_ERROR("Failed to find an acceptable blend mode");
    return false;
  }

  return true;
}

void
OpenXRProgram::LogInstanceInfo(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->instance != XR_NULL_HANDLE);

  XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
  IF_XR_FAILED (err,
      xrGetInstanceProperties(context->instance, &instanceProperties)) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  LOG_INFO("Instance RuntimeName=%s RuntimeVersion=%s",
      instanceProperties.runtimeName,
      GetXrVersionString(instanceProperties.runtimeVersion).c_str());
}

void
OpenXRProgram::LogLayersAndExtensions() const
{
  const auto LogExtensions = [](const char *layerName, int indent = 0) {
    uint32_t instance_extension_count;
    IF_XR_FAILED (err, xrEnumerateInstanceExtensionProperties(
                           layerName, 0, &instance_extension_count, nullptr)) {
      LOG_WARN("%s", err.c_str());
      return;
    }

    std::vector<XrExtensionProperties> extensions(instance_extension_count);
    for (auto &extension : extensions) {
      extension.type = XR_TYPE_EXTENSION_PROPERTIES;
    }

    IF_XR_FAILED (err, xrEnumerateInstanceExtensionProperties(layerName,
                           (uint32_t)extensions.size(),
                           &instance_extension_count, extensions.data())) {
      LOG_WARN("%s", err.c_str());
      return;
    }

    const std::string indent_str(indent, ' ');
    LOG_DEBUG("%sAvailable Extensions: (%d)", indent_str.c_str(),
        instance_extension_count);
    for (const auto &extension : extensions) {
      LOG_DEBUG("%s  Name=%s SpecVersion=%d", indent_str.c_str(),
          extension.extensionName, extension.extensionVersion);
    }
  };

  // Log non-layer extensions
  LogExtensions(nullptr);

  // Log layers and any of their extensions
  uint32_t layer_count;
  IF_XR_FAILED (err, xrEnumerateApiLayerProperties(0, &layer_count, nullptr)) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  std::vector<XrApiLayerProperties> layers(layer_count);
  for (auto &layer : layers) {
    layer.type = XR_TYPE_API_LAYER_PROPERTIES;
  }

  IF_XR_FAILED (err, xrEnumerateApiLayerProperties((uint32_t)layers.size(),
                         &layer_count, layers.data())) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  LOG_INFO("Available Layers: (%d)", layer_count);
  for (const auto &layer : layers) {
    LOG_DEBUG(" Name%s SpecVersion=%s LayerVersion=%d Description=%s",
        layer.layerName, GetXrVersionString(layer.specVersion).c_str(),
        layer.layerVersion, layer.description);
    LogExtensions(layer.layerName, 4);
  }
}

std::string
OpenXRProgram::GetXrVersionString(XrVersion version) const
{
  std::ostringstream oss;
  oss << XR_VERSION_MAJOR(version) << ".";
  oss << XR_VERSION_MINOR(version) << ".";
  oss << XR_VERSION_PATCH(version);
  return oss.str();
}

}  // namespace zen::display_system::oculus
