#include "pch.h"

#include "logger.h"
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

    auto program = std::make_unique<OpenXRProgram>(app);

    if (!program->Initialize()) LOG_ERROR("Failed to initialize the program");

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
  } catch (...) {
    LOG_ERROR("Unkown Error");
  }
}
