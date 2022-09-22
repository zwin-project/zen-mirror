#include "pch.h"

#include "logger.h"
#include "openxr-action.h"
#include "openxr-util.h"

namespace zen::display_system::oculus {

bool
OpenXRAction::Init()
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

  // Create actions
  {
    XrActionCreateInfo action_create_info{XR_TYPE_ACTION_CREATE_INFO};
    action_create_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy(action_create_info.actionName, "quit_session");
    strcpy(action_create_info.localizedActionName, "Quit Session");
    action_create_info.countSubactionPaths = 0;
    action_create_info.subactionPaths = nullptr;
    IF_XR_FAILED (err,
        xrCreateAction(action_set_, &action_create_info, &quit_action_)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }
  }

  // Bind actions
  {
    std::array<XrPath, 2> menu_click_path;  // 0: right, 1: left
    XrPath khr_simple_interaction_profile_path;
    IF_XR_FAILED (err,
        xrStringToPath(context_->instance(),
            "/user/hand/right/input/menu/click", &menu_click_path[0])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err,
        xrStringToPath(context_->instance(), "/user/hand/left/input/menu/click",
            &menu_click_path[1])) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    IF_XR_FAILED (err, xrStringToPath(context_->instance(),
                           "/interaction_profiles/khr/simple_controller",
                           &khr_simple_interaction_profile_path)) {
      LOG_ERROR("%s", err.c_str());
      return false;
    }

    std::vector<XrActionSuggestedBinding> bindings{{
        {quit_action_, menu_click_path[0]},
        {quit_action_, menu_click_path[1]},
    }};

    XrInteractionProfileSuggestedBinding suggested_bindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested_bindings.interactionProfile = khr_simple_interaction_profile_path;
    suggested_bindings.suggestedBindings = bindings.data();
    suggested_bindings.countSuggestedBindings = bindings.size();

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

}  // namespace zen::display_system::oculus
