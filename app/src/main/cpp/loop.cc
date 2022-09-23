#include "pch.h"

#include "logger.h"
#include "loop.h"

namespace zen::display_system::oculus {

void
Loop::Run()
{
  running_ = true;
  while (!app_->destroyRequested && running_) {
    for (;;) {
      void *data;

      int result = ALooper_pollAll(0, nullptr, nullptr, &data);

      if (HandleAndroidPollEvent(result, data) == false) {
        break;
      }
    }

    for (auto &source : busy_sources_) {
      if (running_ == false) break;
      source->Process();
    }
  }
}

void
Loop::Terminate()
{
  running_ = false;
}

void
Loop::AddBusy(std::shared_ptr<Loop::ISource> source)
{
  busy_sources_.push_back(std::move(source));
}

bool
Loop::HandleAndroidPollEvent(int result, void *data)
{
  switch (result) {
    case ALOOPER_POLL_WAKE:
    case ALOOPER_POLL_TIMEOUT:
      return false;

    case ALOOPER_POLL_ERROR:
      LOG_ERROR("ALooper_PollAll failed");
      Terminate();
      return false;

    default:
      if (result == LOOPER_ID_MAIN || result == LOOPER_ID_INPUT) {
        auto source = reinterpret_cast<struct android_poll_source *>(data);
        source->process(app_, source);
        return true;
      } else {
        LOG_ERROR("Unknown loop identifier");
        Terminate();
        return false;
      }
      break;
  }

  return false;
}

}  // namespace zen::display_system::oculus
