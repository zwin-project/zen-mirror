#pragma once

#include "loop.h"

namespace zen::mirror {

class RemoteLoop : public remote::ILoop {
 public:
  RemoteLoop(std::shared_ptr<Loop> loop) : main_loop_(std::move(loop)) {}

  void AddFd(remote::FdSource *source) override;

  void RemoveFd(remote::FdSource *source) override;

  void Terminate() override;

 private:
  std::shared_ptr<Loop> main_loop_;
};

}  // namespace zen::mirror
