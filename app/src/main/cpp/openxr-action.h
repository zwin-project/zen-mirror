#pragma once

namespace zen::display_system::oculus {

struct OpenXRAction {
  friend class OpenXRProgram;

 private:
  XrActionSet action_set{XR_NULL_HANDLE};
  XrAction quit_action{XR_NULL_HANDLE};
};

}  // namespace zen::display_system::oculus
