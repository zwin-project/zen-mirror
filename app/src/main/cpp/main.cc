#include "pch.h"

#include "logger.h"
#include "openxr-action.h"
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

    auto context = std::make_unique<OpenXRContext>();
    auto openxr = std::make_unique<OpenXRProgram>();
    auto action = std::make_unique<OpenXRAction>();

    if (!openxr->InitializeLoader(app)) {
      LOG_ERROR("Failed to initialize OpenXR loader");
      return;
    }

    if (!openxr->InitializeContext(context, app)) {
      LOG_ERROR("Failed to initialize OpenXR context");
      return;
    }

    if (!openxr->InitializeAction(context, action)) {
      LOG_ERROR("Failed to initialize OpenXR actions");
      return;
    }

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }
}
