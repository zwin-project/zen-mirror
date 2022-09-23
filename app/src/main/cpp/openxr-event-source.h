#pragma once

#include "common.h"
#include "loop.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXREventSource : public Loop::ISource {
 public:
  DISABLE_MOVE_AND_COPY(OpenXREventSource);
  OpenXREventSource(
      std::shared_ptr<OpenXRContext> context, std::shared_ptr<Loop> loop)
      : context_(std::move(context)), loop_(std::move(loop))
  {
  }

  void Process() override;

 private:
  /**
   * @returns false if no event found
   */
  bool TryReadNextEvent(XrEventDataBuffer* event);

  void HandleSessionStateChangedEvent(XrEventDataSessionStateChanged* event);

  std::shared_ptr<OpenXRContext> context_;
  std::shared_ptr<Loop> loop_;
};

}  // namespace zen::display_system::oculus
