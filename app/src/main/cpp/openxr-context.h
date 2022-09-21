#pragma once

struct OpenXRContext {
  XrInstance instance{XR_NULL_HANDLE};
  XrSystemId system_id{XR_NULL_SYSTEM_ID};
};
