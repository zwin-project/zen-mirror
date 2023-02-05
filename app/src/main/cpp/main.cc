#include "pch.h"

#include "config.h"
#include "logger.h"
#include "loop.h"
#include "openxr-action-source.h"
#include "openxr-context.h"
#include "openxr-event-source.h"
#include "openxr-view-source.h"
#include "remote-log-sink.h"
#include "remote-loop.h"

using namespace zen::mirror;

void
android_main(struct android_app *app)
{
  try {
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    InitializeLogger();
    zen::remote::InitializeLogger(std::make_unique<RemoteLogSink>());
    LOG_DEBUG("%s", GLM_VERSION_MESSAGE);

    auto loop = std::make_shared<Loop>(app);

    std::shared_ptr<zen::remote::client::IRemote> remote =
        zen::remote::client::CreateRemote(std::make_unique<RemoteLoop>(loop));

    remote->StartGrpcServer();

    auto context = std::make_shared<OpenXRContext>(loop, remote);
    if (!context->Init(app)) {
      LOG_ERROR("Failed to initialize OpenXR context");
      return;
    }

    auto xr_event_source = std::make_shared<OpenXREventSource>(context, loop);

    auto action_source = std::make_shared<OpenXRActionSource>(context, loop);
    if (!action_source->Init()) {
      LOG_ERROR("Failed to initialize OpenXR actions");
      return;
    }

    std::shared_ptr<OpenXRViewSource> view_source;
    view_source = std::make_shared<OpenXRViewSource>(context, loop, remote);

    if (!view_source->Init()) {
      LOG_ERROR("Failed to initialize OpenXR view source");
      return;
    }

    loop->AddBusy(xr_event_source);
    loop->AddBusy(action_source);
    loop->AddBusy(view_source);

    loop->Run();

  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }

  LOG_INFO("%s Exit", config::APP_NAME);
  app->activity->vm->DetachCurrentThread();
}
