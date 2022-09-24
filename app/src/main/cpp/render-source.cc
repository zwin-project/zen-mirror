#include "pch.h"

#include "logger.h"
#include "openxr-util.h"
#include "render-source.h"

namespace zen::display_system::oculus {

void
RenderSource::Process()
{
  if (context_->is_session_running() == false) return;

  XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frame_state{XR_TYPE_FRAME_STATE};
  IF_XR_FAILED (err,
      xrWaitFrame(context_->session(), &frame_wait_info, &frame_state)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }

  XrFrameBeginInfo frame_begin_info{XR_TYPE_FRAME_BEGIN_INFO};
  IF_XR_FAILED (err, xrBeginFrame(context_->session(), &frame_begin_info)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }

  std::vector<XrCompositionLayerBaseHeader*> layers;
  if (frame_state.shouldRender == XR_TRUE) {
    // TODO: render
  }

  XrFrameEndInfo frame_end_info{XR_TYPE_FRAME_END_INFO};
  frame_end_info.displayTime = frame_state.predictedDisplayTime;
  frame_end_info.environmentBlendMode = context_->environment_blend_mode();
  frame_end_info.layerCount = (uint32_t)layers.size();
  frame_end_info.layers = layers.data();
  IF_XR_FAILED (err, xrEndFrame(context_->session(), &frame_end_info)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }
}

}  // namespace zen::display_system::oculus
