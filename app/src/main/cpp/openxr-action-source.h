#pragma once

#include "loop.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class OpenXRActionSource : public Loop::ISource {
 public:
  DISABLE_MOVE_AND_COPY(OpenXRActionSource);
  OpenXRActionSource(
      std::shared_ptr<OpenXRContext> context, std::shared_ptr<Loop> loop)
      : context_(std::move(context)), loop_(std::move(loop))
  {
  }
  ~OpenXRActionSource();

  bool Init();
  void Process() override;

 private:
  enum class Hand { kLeft = 0, kRight = 1, kCount = 2 };

  std::shared_ptr<OpenXRContext> context_;
  std::shared_ptr<Loop> loop_;
  XrActionSet action_set_{XR_NULL_HANDLE};
  XrAction grab_action_{XR_NULL_HANDLE};
  XrAction vibrate_action_{XR_NULL_HANDLE};
  std::array<XrPath, (size_t)Hand::kCount> hand_subaction_path_;
};

}  // namespace zen::display_system::oculus
