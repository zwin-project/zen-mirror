#pragma once

#include "common.h"

namespace zen::display_system::oculus {

class EglInstance {
 public:
  DISABLE_MOVE_AND_COPY(EglInstance);
  EglInstance() = default;
  ~EglInstance() = default;

  bool Initialize();

  inline EGLDisplay display();
  inline EGLDisplay context();

 private:
  EGLDisplay display_;
  EGLConfig config_;
  EGLContext context_;
  EGLSurface surface_;
};

inline EGLDisplay
EglInstance::display()
{
  return display_;
}

inline EGLContext
EglInstance::context()
{
  return context_;
}

}  // namespace zen::display_system::oculus
