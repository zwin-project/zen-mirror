#pragma once

#include "common.h"

namespace zen::display_system::oculus {

class Loop {
 public:
  struct ISource;

  DISABLE_MOVE_AND_COPY(Loop);
  Loop(struct android_app *app) : app_(app) {}
  ~Loop() = default;

  void Run();
  void Terminate();

  /* Add a busy loop sources to be processed at each loop iteration */
  void AddBusy(std::shared_ptr<Loop::ISource> source);

 private:
  /**
   * @returns true if there may still be events to process.
   */
  bool HandleAndroidPollEvent(int res, void *data);

  std::vector<std::shared_ptr<Loop::ISource>> busy_sources_;
  struct android_app *app_;
  bool running_;
};

struct Loop::ISource {
  DISABLE_MOVE_AND_COPY(ISource);
  ISource() = default;
  virtual ~ISource() = default;

  virtual void Process() = 0;
};

}  // namespace zen::display_system::oculus
