#include "pch.h"

#include "logger.h"
#include "openxr-action-source.h"
#include "openxr-util.h"

namespace zen::display_system::oculus {

OpenXRActionSource::~OpenXRActionSource()
{
  if (grab_action_ != XR_NULL_HANDLE) {
    xrDestroyAction(grab_action_);
  }

  if (vibrate_action_ != XR_NULL_HANDLE) {
    xrDestroyAction(vibrate_action_);
  }

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
}

bool
OpenXRActionSource::Init()
{
  // Create an action set
  {
    XrActionSetCreateInfo action_set_create_info{
        XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(action_set_create_info.actionSetName, "zen");
    strcpy(action_set_create_info.localizedActionSetName, "zen");
    action_set_create_info.priority = 0;
    IF_XR_FAILED (err, xrCreateActionSet(context_->instance(),
                           &action_set_create_info, &action_set_)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Get the XrPath for the left and right hands - we will use them as subaction
  // paths.
  {
    IF_XR_FAILED (err, xrStringToPath(context_->instance(), "/user/hand/left",
                           &hand_subaction_path_[(int)Hand::kLeft])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err, xrStringToPath(context_->instance(), "/user/hand/right",
                           &hand_subaction_path_[(int)Hand::kRight])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Create actions
  {
    XrActionCreateInfo action_create_info{XR_TYPE_ACTION_CREATE_INFO};

    action_create_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(action_create_info.actionName, "grab");
    strcpy(action_create_info.localizedActionName, "Grab");
    action_create_info.countSubactionPaths =
        uint32_t(hand_subaction_path_.size());
    action_create_info.subactionPaths = hand_subaction_path_.data();
    IF_XR_FAILED (err,
        xrCreateAction(action_set_, &action_create_info, &grab_action_)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    action_create_info.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy(action_create_info.actionName, "vibrate_hand");
    strcpy(action_create_info.localizedActionName, "Vibrate Hand");
    action_create_info.countSubactionPaths =
        uint32_t(hand_subaction_path_.size());
    action_create_info.subactionPaths = hand_subaction_path_.data();
    IF_XR_FAILED (err,
        xrCreateAction(action_set_, &action_create_info, &vibrate_action_)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Create binding paths
  std::array<XrPath, (size_t)Hand::kCount> squeeze_value_path;
  std::array<XrPath, (size_t)Hand::kCount> haptic_path;
  {
    IF_XR_FAILED (err, xrStringToPath(context_->instance(),
                           "/user/hand/left/input/squeeze/value",
                           &squeeze_value_path[(int)Hand::kLeft])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err, xrStringToPath(context_->instance(),
                           "/user/hand/right/input/squeeze/value",
                           &squeeze_value_path[(int)Hand::kRight])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err,
        xrStringToPath(context_->instance(), "/user/hand/left/output/haptic",
            &haptic_path[(int)Hand::kLeft])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err,
        xrStringToPath(context_->instance(), "/user/hand/right/output/haptic",
            &haptic_path[(int)Hand::kRight])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Bind actions
  {
    XrPath oculus_interaction_path;
    IF_XR_FAILED (err, xrStringToPath(context_->instance(),
                           "/interaction_profiles/oculus/touch_controller",
                           &oculus_interaction_path)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    std::vector<XrActionSuggestedBinding> bindings{{
        {grab_action_, squeeze_value_path[(int)Hand::kLeft]},
        {grab_action_, squeeze_value_path[(int)Hand::kRight]},
        {vibrate_action_, haptic_path[(int)Hand::kLeft]},
        {vibrate_action_, haptic_path[(int)Hand::kRight]},
    }};

    XrInteractionProfileSuggestedBinding suggested_bindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested_bindings.interactionProfile = oculus_interaction_path;
    suggested_bindings.countSuggestedBindings = (uint32_t)bindings.size();
    suggested_bindings.suggestedBindings = bindings.data();
    IF_XR_FAILED (err, xrSuggestInteractionProfileBindings(
                           context_->instance(), &suggested_bindings)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Attach the action set to the session
  {
    XrSessionActionSetsAttachInfo attach_info{
        XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach_info.countActionSets = 1;
    attach_info.actionSets = &action_set_;
    IF_XR_FAILED (err,
        xrAttachSessionActionSets(context_->session(), &attach_info)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  return true;
}

void
OpenXRActionSource::Process()
{
  if (context_->is_session_running() == false) return;

  const XrActiveActionSet active_action_set{action_set_, XR_NULL_PATH};
  XrActionsSyncInfo sync_info{XR_TYPE_ACTIONS_SYNC_INFO};
  sync_info.countActiveActionSets = 1;
  sync_info.activeActionSets = &active_action_set;
  IF_XR_FAILED (err, xrSyncActions(context_->session(), &sync_info)) {
    LOG_ERROR("%s", err.c_str());
    loop_->Terminate();
    return;
  }

  // When you hold a controller, that controller vibrates.
  for (auto hand : {Hand::kLeft, Hand::kRight}) {
    XrActionStateGetInfo get_info{XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = grab_action_;
    get_info.subactionPath = hand_subaction_path_[(int)hand];

    XrActionStateFloat grab_value{XR_TYPE_ACTION_STATE_FLOAT};
    IF_XR_FAILED (err,
        xrGetActionStateFloat(context_->session(), &get_info, &grab_value)) {
      LOG_WARN("%s", err.c_str());
      continue;
    }

    if (grab_value.isActive == XR_TRUE && grab_value.currentState > 0.9f) {
      XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
      vibration.amplitude = 0.5;
      vibration.duration = XR_MIN_HAPTIC_DURATION;
      vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

      XrHapticActionInfo haptic_action_info{XR_TYPE_HAPTIC_ACTION_INFO};
      haptic_action_info.action = vibrate_action_;
      haptic_action_info.subactionPath = hand_subaction_path_[(int)hand];
      IF_XR_FAILED (err,
          xrApplyHapticFeedback(context_->session(), &haptic_action_info,
              (XrHapticBaseHeader*)&vibration)) {
        LOG_WARN("%s", err.c_str());
        continue;
      }
    }
  }
}

}  // namespace zen::display_system::oculus
