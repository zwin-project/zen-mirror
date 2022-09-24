#include "pch.h"

#include "config.h"
#include "logger.h"
#include "loop.h"
#include "openxr-action.h"
#include "openxr-context.h"
#include "openxr-event-source.h"
#include "openxr-view-config.h"
#include "render-source.h"

using namespace zen::display_system::oculus;

void
android_main(struct android_app *app)
{
  try {
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    InitializeLogger();
    LOG_DEBUG("Boost Version %d", BOOST_VERSION);
    LOG_DEBUG("%s", GLM_VERSION_MESSAGE);

    auto loop = std::make_shared<Loop>(app);

    auto context = std::make_shared<OpenXRContext>(loop);
    if (!context->Init(app)) {
      LOG_ERROR("Failed to initialize OpenXR context");
      return;
    }

    auto view_config = std::make_unique<OpenXRViewConfig>(context);
    if (!view_config->Init()) {
      LOG_ERROR("Failed to initialize OpenXR view configuration");
      return;
    }

    auto action = std::make_unique<OpenXRAction>(context);
    if (!action->Init()) {
      LOG_ERROR("Failed to initialize OpenXR actions");
      return;
    }

    auto xr_event_source = std::make_shared<OpenXREventSource>(context, loop);
    auto render_source = std::make_shared<RenderSource>(context, loop);

    loop->AddBusy(xr_event_source);
    loop->AddBusy(render_source);

    loop->Run();

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }

  LOG_INFO("%s Exit", config::APP_NAME);
}
