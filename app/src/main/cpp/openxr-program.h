#pragma once

#include "common.h"

namespace zen::display_system::oculus {

struct OpenXRProgram {
  DISABLE_MOVE_AND_COPY(OpenXRProgram);
  OpenXRProgram(struct android_app *app) : app_(app){};
  ~OpenXRProgram() = default;

  bool Initialize() const;

 private:
  bool InitializeLoader() const;

  struct android_app *app_;
};

}  // namespace zen::display_system::oculus
