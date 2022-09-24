#pragma once

#include "common.h"
#include "loop.h"
#include "openxr-context.h"

namespace zen::display_system::oculus {

class RenderSource : public Loop::ISource {
 public:
  DISABLE_MOVE_AND_COPY(RenderSource);
  RenderSource(
      std::shared_ptr<OpenXRContext> context, std::shared_ptr<Loop> loop)
      : context_(std::move(context)), loop_(std::move(loop))
  {
  }
  ~RenderSource() = default;

  void Process() override;

 private:
  std::shared_ptr<OpenXRContext> context_;
  std::shared_ptr<Loop> loop_;
};

}  // namespace zen::display_system::oculus
