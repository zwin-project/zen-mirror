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
MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
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

namespace Math {

XrQuaternionf
ToXrQuaternionf(glm::quat orientation)
{
  return XrQuaternionf{
      orientation.x, orientation.y, orientation.z, orientation.w};
}

XrVector3f
ToXrVector3f(glm::vec3 position)
{
  return XrVector3f{position.x, position.y, position.z};
}

XrPosef
ToXrPosef(glm::vec3 position, glm::quat orientation)
{
  return XrPosef{
      ToXrQuaternionf(orientation),
      ToXrVector3f(position),
  };
}

}  // namespace Math

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

  if (!InitializeGraphicsLibrary(context)) return false;

  if (!InitializeSession(context)) return false;

  LogReferenceSpaces(context);

  if (!InitializeAppSpace(context)) return false;

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
  constexpr XrFormFactor kFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

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
OpenXRProgram::InitializeGraphicsLibrary(
    [[maybe_unused]] const std::unique_ptr<OpenXRContext> &context) const
{
  PFN_xrGetOpenGLESGraphicsRequirementsKHR
      xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
  IF_XR_FAILED (err, xrGetInstanceProcAddr(context->instance,
                         "xrGetOpenGLESGraphicsRequirementsKHR",
                         reinterpret_cast<PFN_xrVoidFunction *>(
                             &xrGetOpenGLESGraphicsRequirementsKHR))) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  IF_XR_FAILED (err, xrGetOpenGLESGraphicsRequirementsKHR(context->instance,
                         context->system_id, &graphicsRequirements)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  context->egl = std::make_unique<EglInstance>();
  if (!context->egl->Initialize()) {
    LOG_ERROR("Failed to initialize EGL context");
    return false;
  }

  GLint gl_major_version, gl_minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
  glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);

  const XrVersion gl_version =
      XR_MAKE_VERSION(gl_major_version, gl_minor_version, 0);
  if (graphicsRequirements.minApiVersionSupported > gl_version ||
      graphicsRequirements.maxApiVersionSupported < gl_version) {
    LOG_ERROR(
        "OpenGL ES Version not supported\n"
        "    Using: %d.%d\n"
        "    Required: %s - %s",
        gl_major_version, gl_minor_version,
        GetXrVersionString(graphicsRequirements.minApiVersionSupported).c_str(),
        GetXrVersionString(graphicsRequirements.maxApiVersionSupported)
            .c_str());
    return false;
  }

  LOG_INFO("Using OpenGLES %d.%d", gl_major_version, gl_minor_version);

  return true;
}

bool
OpenXRProgram::InitializeSession(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->instance != XR_NULL_HANDLE);
  CHECK(context->system_id != XR_NULL_SYSTEM_ID);

  XrSessionCreateInfo session_create_info{XR_TYPE_SESSION_CREATE_INFO};

  XrGraphicsBindingOpenGLESAndroidKHR graphics_binding{
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
  graphics_binding.display = context->egl->display();
  graphics_binding.config = (EGLConfig)0;
  graphics_binding.context = context->egl->context();

  session_create_info.next =
      reinterpret_cast<XrBaseInStructure *>(&graphics_binding);
  session_create_info.systemId = context->system_id;
  IF_XR_FAILED (err, xrCreateSession(context->instance, &session_create_info,
                         &context->session)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRProgram::InitializeAppSpace(
    [[maybe_unused]] const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->session != XR_NULL_HANDLE);

  glm::vec3 position(0, 0, 0);
  glm::quat orientation(0, 0, 0, 1);
  XrReferenceSpaceCreateInfo reference_space_create_info{
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  reference_space_create_info.poseInReferenceSpace =
      Math::ToXrPosef(position, orientation);
  reference_space_create_info.referenceSpaceType =
      XR_REFERENCE_SPACE_TYPE_STAGE;

  IF_XR_FAILED (err, xrCreateReferenceSpace(context->session,
                         &reference_space_create_info, &context->app_space)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

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
OpenXRProgram::LogReferenceSpaces(
    const std::unique_ptr<OpenXRContext> &context) const
{
  CHECK(context->session != XR_NULL_HANDLE);

  uint32_t space_count;
  IF_XR_FAILED (err,
      xrEnumerateReferenceSpaces(context->session, 0, &space_count, nullptr)) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  std::vector<XrReferenceSpaceType> spaces(space_count);

  IF_XR_FAILED (err, xrEnumerateReferenceSpaces(context->session, space_count,
                         &space_count, spaces.data())) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  LOG_DEBUG("Available reference spaces: %d", space_count);
  for (auto space : spaces) {
    LOG_DEBUG("    Name: %s", to_string(space));
  }
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
