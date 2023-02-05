#include "pch.h"

#include "logger.h"
#include "remote-loop.h"

namespace zen::mirror {

namespace {

int
LooperCallback(int fd, int events, void *data)
{
  auto source = static_cast<remote::FdSource *>(data);
  uint32_t mask = 0;

  if (events & ALOOPER_EVENT_INPUT) {
    mask |= remote::FdSource::kReadable;
  }
  if (events & ALOOPER_EVENT_OUTPUT) {
    mask |= remote::FdSource::kWritable;
  }
  if (events & ALOOPER_EVENT_HANGUP) {
    mask |= remote::FdSource::kHangup;
  }
  if (events & ALOOPER_EVENT_ERROR) {
    mask |= remote::FdSource::kError;
  }

  try {
    source->callback(fd, mask);
  } catch (const std::exception &e) {
    LOG_ERROR("%s", e.what());
    auto that = static_cast<RemoteLoop *>(data);
    that->Terminate();
  }

  return 1;
}

}  // namespace

void
RemoteLoop::AddFd(remote::FdSource *source)
{
  auto looper = ALooper_forThread();
  int events = 0;

  if (source->mask & remote::FdSource::kReadable) {
    events |= ALOOPER_EVENT_INPUT;
  }
  if (source->mask & remote::FdSource::kWritable) {
    events |= ALOOPER_EVENT_OUTPUT;
  }
  if (source->mask & remote::FdSource::kHangup) {
    events |= ALOOPER_EVENT_HANGUP;
  }
  if (source->mask & remote::FdSource::kError) {
    events |= ALOOPER_EVENT_ERROR;
  }

  ALooper_addFd(looper, source->fd, ALOOPER_POLL_CALLBACK, events,
      LooperCallback, source);
  source->data = this;
}

void
RemoteLoop::RemoveFd(remote::FdSource *source)
{
  auto looper = ALooper_forThread();
  ALooper_removeFd(looper, source->fd);
  source->data = nullptr;
}

void
RemoteLoop::Terminate()
{
  main_loop_->Terminate();
};

}  // namespace zen::mirror
