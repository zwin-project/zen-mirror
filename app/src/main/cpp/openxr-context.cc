#include "pch.h"

#include "common.h"
#include "config.h"
#include "logger.h"
#include "openxr-context.h"
#include "openxr-util.h"

namespace zen::display_system::oculus {

OpenXRContext::~OpenXRContext()
{
  if (app_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(app_space_);
  }

  if (session_ != XR_NULL_HANDLE) {
    xrDestroySession(session_);
  }

  if (instance_ != XR_NULL_HANDLE) {
    xrDestroyInstance(instance_);
  }
}

bool
OpenXRContext::Init(struct android_app *app)
{
  if (!InitializeLoader(app)) return false;

  LogLayersAndExtensions();

  if (!InitializeInstance(app)) return false;

  LogInstanceInfo();

  if (!InitializeSystem()) return false;

  if (!InitializeViewConfig()) return false;

  if (!InitializeEnvironmentBlendMode()) return false;

  if (!InitializeGraphicsLibrary()) return false;

  if (!InitializeSession()) return false;

  LogReferenceSpaces();

  if (!InitializeAppSpace()) return false;

  return true;
}

void
OpenXRContext::UpdateSessionState(XrSessionState state, XrTime time)
{
  XrSessionState old_state = session_state_;
  session_state_ = state;
  LOG_INFO("XrEventDataSessionStateChanged: state %s -> %s time=%" PRId64,
      to_string(old_state), to_string(session_state_), time);

  switch (session_state_) {
    case XR_SESSION_STATE_READY: {
      XrSessionBeginInfo session_begin_info{XR_TYPE_SESSION_BEGIN_INFO};
      session_begin_info.primaryViewConfigurationType =
          view_configuration_type_;
      IF_XR_FAILED (err, xrBeginSession(session_, &session_begin_info)) {
        LOG_ERROR("%s", err.c_str());
        loop_->Terminate();
      }
      is_session_running_ = true;
      remote_->EnableSession();
      break;
    }

    case XR_SESSION_STATE_STOPPING: {
      IF_XR_FAILED (err, xrEndSession(session_)) {
        LOG_ERROR("%s", err.c_str());
        loop_->Terminate();
      }
      is_session_running_ = false;
      remote_->DisableSession();
      break;
    }

    case XR_SESSION_STATE_EXITING:  // fall through
    case XR_SESSION_STATE_LOSS_PENDING:
      loop_->Terminate();
      break;

    default:
      break;
  }
}

bool
OpenXRContext::InitializeLoader(struct android_app *app)
{
  PFN_xrInitializeLoaderKHR xrInitializeLoader = nullptr;
  auto res = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
      (PFN_xrVoidFunction *)(&xrInitializeLoader));
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
  IF_XR_FAILED (err,
      xrInitializeLoader(
          (const XrLoaderInitInfoBaseHeaderKHR *)&loader_init_info_android)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRContext::InitializeInstance(struct android_app *app)
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

  IF_XR_FAILED (err, xrCreateInstance(&create_info, &instance_)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRContext::InitializeSystem()
{
  CHECK(system_id_ == XR_NULL_SYSTEM_ID);
  constexpr XrFormFactor kFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = kFormFactor;
  IF_XR_FAILED (err, xrGetSystem(instance_, &system_info, &system_id_)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Using system %" PRIu64 " for form factor %s", system_id_,
      to_string(kFormFactor));

  return true;
}

bool
OpenXRContext::InitializeGraphicsLibrary()
{
  PFN_xrGetOpenGLESGraphicsRequirementsKHR
      xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
  IF_XR_FAILED (err,
      xrGetInstanceProcAddr(instance_, "xrGetOpenGLESGraphicsRequirementsKHR",
          reinterpret_cast<PFN_xrVoidFunction *>(
              &xrGetOpenGLESGraphicsRequirementsKHR))) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  IF_XR_FAILED (err, xrGetOpenGLESGraphicsRequirementsKHR(
                         instance_, system_id_, &graphicsRequirements)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  egl_ = std::make_unique<EglInstance>();
  if (!egl_->Initialize()) {
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

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(
      [](GLenum /*source*/, GLenum type, GLuint /*id*/, GLenum /*severity*/,
          GLsizei length, const GLchar *message, const void * /*user_param*/) {
        switch (type) {
          case GL_DEBUG_TYPE_ERROR:  // fallthrough
          case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            LOG_ERROR("GLES: %s", std::string(message, 0, length).c_str());
          case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            LOG_WARN("GLES: %s", std::string(message, 0, length).c_str());
          case GL_DEBUG_TYPE_PORTABILITY:  // fallthrough
          case GL_DEBUG_TYPE_PERFORMANCE:  // fallthrough
          case GL_DEBUG_TYPE_OTHER:        // fallthrough
          case GL_DEBUG_TYPE_MARKER:       // fallthrough
          case GL_DEBUG_TYPE_PUSH_GROUP:   // fallthrough
          case GL_DEBUG_TYPE_POP_GROUP:
            LOG_DEBUG("GLES: %s", std::string(message, 0, length).c_str());
            break;
          default:
            LOG_ERROR("GLES: %s", std::string(message, 0, length).c_str());
            break;
        }
      },
      nullptr);

  // Silence unwanted message types
  glDebugMessageControl(
      GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
  glDebugMessageControl(
      GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);

  return true;
}

bool
OpenXRContext::InitializeSession()
{
  CHECK(instance_ != XR_NULL_HANDLE);
  CHECK(system_id_ != XR_NULL_SYSTEM_ID);

  XrSessionCreateInfo session_create_info{XR_TYPE_SESSION_CREATE_INFO};

  XrGraphicsBindingOpenGLESAndroidKHR graphics_binding{
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
  graphics_binding.display = egl_->display();
  graphics_binding.config = egl_->config();
  graphics_binding.context = egl_->context();

  session_create_info.next =
      reinterpret_cast<XrBaseInStructure *>(&graphics_binding);
  session_create_info.systemId = system_id_;
  IF_XR_FAILED (err,
      xrCreateSession(instance_, &session_create_info, &session_)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRContext::InitializeAppSpace()
{
  CHECK(session_ != XR_NULL_HANDLE);

  glm::vec3 position(0, 0, 0);
  glm::quat orientation(1, 0, 0, 0);
  XrReferenceSpaceCreateInfo reference_space_create_info{
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  reference_space_create_info.poseInReferenceSpace =
      Math::ToXrPosef(position, orientation);
  reference_space_create_info.referenceSpaceType =
      XR_REFERENCE_SPACE_TYPE_STAGE;

  IF_XR_FAILED (err, xrCreateReferenceSpace(
                         session_, &reference_space_create_info, &app_space_)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

bool
OpenXRContext::InitializeViewConfig()
{
  CHECK(instance_ != XR_NULL_HANDLE);
  CHECK(system_id_ != XR_NULL_SYSTEM_ID);
  bool acceptableConfigTypeFound = false;

  uint32_t view_config_type_count;
  IF_XR_FAILED (err, xrEnumerateViewConfigurations(instance_, system_id_, 0,
                         &view_config_type_count, nullptr)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  std::vector<XrViewConfigurationType> view_config_types(
      view_config_type_count);

  IF_XR_FAILED (err, xrEnumerateViewConfigurations(instance_, system_id_,
                         view_config_type_count, &view_config_type_count,
                         view_config_types.data())) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Available View Configuration Types: (%d)", view_config_type_count);
  for (auto view_config_type : view_config_types) {
    if (view_config_type == OpenXRContext::kAcceptableViewConfigType) {
      view_configuration_type_ = view_config_type;
      acceptableConfigTypeFound = true;
    }

    LOG_DEBUG("  View Configuration Type: %s %s", to_string(view_config_type),
        view_config_type == OpenXRContext::kAcceptableViewConfigType
            ? "(selected)"
            : "");

    XrViewConfigurationProperties view_config_props{
        XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
    IF_XR_FAILED (err, xrGetViewConfigurationProperties(instance_, system_id_,
                           view_config_type, &view_config_props)) {
      LOG_WARN("%s", err.c_str());
      continue;
    }

    LOG_DEBUG("  View Configuration FovMutable: %s",
        view_config_props.fovMutable == XR_TRUE ? "True" : "False");

    uint32_t view_count;
    IF_XR_FAILED (err, xrEnumerateViewConfigurationViews(instance_, system_id_,
                           view_config_type, 0, &view_count, nullptr)) {
      LOG_WARN("%s", err.c_str());
      continue;
    }

    if (view_count > 0) {
      std::vector<XrViewConfigurationView> views(
          view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
      IF_XR_FAILED (err,
          xrEnumerateViewConfigurationViews(instance_, system_id_,
              view_config_type, view_count, &view_count, views.data())) {
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
OpenXRContext::InitializeEnvironmentBlendMode()
{
  CHECK(instance_ != XR_NULL_HANDLE);
  CHECK(system_id_ != XR_NULL_SYSTEM_ID);
  CHECK(view_configuration_type_ == OpenXRContext::kAcceptableViewConfigType);
  bool acceptable_blend_mode_found = false;

  uint32_t count;
  IF_XR_FAILED (err, xrEnumerateEnvironmentBlendModes(instance_, system_id_,
                         view_configuration_type_, 0, &count, nullptr)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  std::vector<XrEnvironmentBlendMode> blend_modes(count);

  IF_XR_FAILED (err,
      xrEnumerateEnvironmentBlendModes(instance_, system_id_,
          view_configuration_type_, count, &count, blend_modes.data())) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_DEBUG("Available Environment Blend Modes for %s: (%d)",
      to_string(view_configuration_type_), count);
  for (uint32_t i = 0; i < count; i++) {
    auto mode = blend_modes[i];
    if (mode == OpenXRContext::kAcceptableEnvironmentBlendModeType) {
      environment_blend_mode_ =
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
OpenXRContext::LogReferenceSpaces() const
{
  CHECK(session_ != XR_NULL_HANDLE);

  uint32_t space_count;
  IF_XR_FAILED (err,
      xrEnumerateReferenceSpaces(session_, 0, &space_count, nullptr)) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  std::vector<XrReferenceSpaceType> spaces(space_count);

  IF_XR_FAILED (err, xrEnumerateReferenceSpaces(
                         session_, space_count, &space_count, spaces.data())) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  LOG_DEBUG("Available reference spaces: %d", space_count);
  for (auto space : spaces) {
    LOG_DEBUG("    Name: %s", to_string(space));
  }
}

void
OpenXRContext::LogInstanceInfo() const
{
  CHECK(instance_ != XR_NULL_HANDLE);

  XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
  IF_XR_FAILED (err, xrGetInstanceProperties(instance_, &instanceProperties)) {
    LOG_WARN("%s", err.c_str());
    return;
  }

  LOG_INFO("Instance RuntimeName=%s RuntimeVersion=%s",
      instanceProperties.runtimeName,
      GetXrVersionString(instanceProperties.runtimeVersion).c_str());
}

void
OpenXRContext::LogLayersAndExtensions() const
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

}  // namespace zen::display_system::oculus
