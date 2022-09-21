#pragma once

#include "common.h"

namespace zen::display_system::oculus {

class EglInstance {
 public:
  DISABLE_MOVE_AND_COPY(EglInstance);
  EglInstance() = default;
  ~EglInstance() = default;

  bool Initialize();

 private:
  EGLDisplay display_;
  EGLConfig config_;
  EGLContext context_;
  EGLSurface surface_;
};

}  // namespace zen::display_system::oculus
