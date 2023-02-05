#include "pch.h"

#include "common.h"
#include "logger.h"
#include "openxr-event-source.h"
#include "openxr-util.h"

namespace zen::mirror {

void
OpenXREventSource::Process()
{
  XrEventDataBuffer event;
  while (TryReadNextEvent(&event)) {
    switch (event.type) {
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
        auto loss_pending_event =
            reinterpret_cast<XrEventDataInstanceLossPending*>(&event);
        LOG_WARN("XrEventDataInstanceLossPending by %" PRId64,
            loss_pending_event->lossTime);
        loop_->Terminate();
        break;
      }

      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        HandleSessionStateChangedEvent(
            reinterpret_cast<XrEventDataSessionStateChanged*>(&event));
        break;
      }

      default:
        LOG_DEBUG("Ignoring event type %d", event.type);
        break;
    }
  }
}

bool
OpenXREventSource::TryReadNextEvent(XrEventDataBuffer* event)
{
  event->type = XR_TYPE_EVENT_DATA_BUFFER;
  const XrResult res = xrPollEvent(context_->instance(), event);
  if (res == XR_SUCCESS) {
    return true;
  } else if (res == XR_EVENT_UNAVAILABLE) {
    return false;
  }

  LOG_ERROR("xrPollEvent failed");
  loop_->Terminate();
  return false;
}

void
OpenXREventSource::HandleSessionStateChangedEvent(
    XrEventDataSessionStateChanged* event)
{
  if ((event->session != XR_NULL_HANDLE) &&
      (event->session != context_->session())) {
    LOG_ERROR("XrEventDataSessionStateChanged for unknown session");
    loop_->Terminate();
    return;
  }

  context_->UpdateSessionState(event->state, event->time);
}

}  // namespace zen::mirror
