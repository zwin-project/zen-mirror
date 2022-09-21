#include "pch.h"

#include "egl-instance.h"
#include "logger.h"

namespace zen::display_system::oculus {

namespace {

static const char *
EglErrorString(const EGLint error)
{
  switch (error) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
      return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    case EGL_CONTEXT_LOST:
      return "EGL_CONTEXT_LOST";
    default:
      return "unknown";
  }
}

std::string
CheckEglResult(
    EGLBoolean res, const char *originator, const char *source_location)
{
  std::ostringstream err;
  if (res == EGL_FALSE) {
    err << "EGL command failure: " << EglErrorString(eglGetError())
        << std::endl;
    err << "    Origin: " << originator << std::endl;
    err << "    Source: " << source_location;
  }

  return err.str();
}

#define IF_EGL_FAILED(err, cmd) \
  if (std::string err = CheckEglResult(cmd, #cmd, FILE_AND_LINE); !err.empty())

}  // namespace

bool
EglInstance::Initialize()
{
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major_version, minor_version;

  IF_EGL_FAILED (err, eglInitialize(display_, &major_version, &minor_version)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  LOG_INFO("EGL Version: %d.%d", major_version, minor_version);

  // Find a config that satisfies `config_attribs` and several other conditions
  {
    constexpr int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint num_configs = 0;
    IF_EGL_FAILED (err,
        eglGetConfigs(display_, configs, MAX_CONFIGS, &num_configs)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    // clang-format off
    const EGLint config_attribs[] = {
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     8,
        EGL_DEPTH_SIZE,     24,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_SAMPLES,        0,
        EGL_NONE,
    };
    // clang-format on

    for (int i = 0; i < num_configs; i++) {
      EGLint value = 0;
      eglGetConfigAttrib(display_, configs[i], EGL_RENDERABLE_TYPE, &value);
      if ((value & EGL_OPENGL_ES3_BIT) != EGL_OPENGL_ES3_BIT) {
        continue;
      }

      eglGetConfigAttrib(display_, configs[i], EGL_SURFACE_TYPE, &value);
      if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) !=
          (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
        continue;
      }

      int j = 0;
      for (; config_attribs[j] != EGL_NONE; j += 2) {
        eglGetConfigAttrib(display_, configs[i], config_attribs[j], &value);
        if (value != config_attribs[j + 1]) {
          break;
        }
      }

      if (config_attribs[j] == EGL_NONE) {
        config_ = configs[i];
      }
    }
  }

  if (config_ == nullptr) {
    LOG_ERROR("Failed to find a desired EGLConfig");
    return false;
  }

  // clang-format off
  EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE,
  };
  // clang-format on

  context_ =
      eglCreateContext(display_, config_, EGL_NO_CONTEXT, context_attribs);
  if (context_ == EGL_NO_CONTEXT) {
    LOG_ERROR("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
    return false;
  }

  // clang-format off
  const EGLint surface_attribs[] = {
      EGL_WIDTH,  16,
      EGL_HEIGHT, 16,
      EGL_NONE,
  };
  // clang-format on

  surface_ = eglCreatePbufferSurface(display_, config_, surface_attribs);
  if (surface_ == EGL_NO_SURFACE) {
    LOG_ERROR(
        "eglCreatePbufferSurface() failed %s", EglErrorString(eglGetError()));
    return false;
  }

  IF_EGL_FAILED (err, eglMakeCurrent(display_, surface_, surface_, context_)) {
    LOG_ERROR("%s", err.c_str());
    return false;
  }

  return true;
}

}  // namespace zen::display_system::oculus
