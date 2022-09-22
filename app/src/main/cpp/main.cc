#include "pch.h"

#include "logger.h"
#include "openxr-context.h"
#include "openxr-program.h"

using namespace zen::display_system::oculus;

void
android_main(struct android_app *app)
{
  try {
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    InitializeLogger();
    LOG_DEBUG("Boost Version %d", BOOST_VERSION);

    auto openxr_context = std::make_unique<OpenXRContext>();
    auto openxr = std::make_unique<OpenXRProgram>();

    if (!openxr->InitializeLoader(app)) {
      LOG_ERROR("Failed to initialize OpenXR loader");
      return;
    }

    if (!openxr->InitializeContext(openxr_context, app)) {
      LOG_ERROR("Failed to initialize OpenXR context");
      return;
    }

    // TODO: Initialize actions

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }
}
