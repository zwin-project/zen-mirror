#include "pch.h"

#include "logger.h"
#include "openxr-action.h"
#include "openxr-context.h"
#include "openxr-view-config.h"

using namespace zen::display_system::oculus;

void
android_main(struct android_app *app)
{
  try {
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    InitializeLogger();
    LOG_DEBUG("Boost Version %d", BOOST_VERSION);

    auto context = std::make_shared<OpenXRContext>();
    if (!context->Init(app)) {
      LOG_ERROR("Failed to initialize OpenXR context");
      return;
    }

    auto view_config = std::make_unique<OpenXRViewConfig>(context);
    view_config->Init();

    auto action = std::make_unique<OpenXRAction>();

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }
}
