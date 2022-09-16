#include "pch.h"

void
android_main(struct android_app *app)
{
  __android_log_print(
      ANDROID_LOG_INFO, "ZEN", "Boost version %d", BOOST_VERSION);
  (void)app;
}
