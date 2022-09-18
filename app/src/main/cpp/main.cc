#include "pch.h"

#include "logger.h"

using namespace zen::display_system::oculus;

void
android_main(struct android_app *app)
{
  InitializeLogger();
  LOG_DEBUG("Boost Version %d", BOOST_VERSION);
  (void)app;
}
