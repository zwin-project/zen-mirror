#pragma once

#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRAction {
 public:
  DISABLE_MOVE_AND_COPY(OpenXRAction);
  OpenXRAction(std::shared_ptr<OpenXRContext> context) : context_(context) {}
  ~OpenXRAction() = default;

  bool Init();

 private:
  std::shared_ptr<OpenXRContext> context_;
  XrActionSet action_set_{XR_NULL_HANDLE};
  XrAction quit_action_{XR_NULL_HANDLE};
};

}  // namespace zen::display_system::oculus
