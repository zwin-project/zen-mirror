#pragma once

namespace zen::display_system::oculus {

namespace {

#define ENUM_CASE_STR(name, val) \
  case name:                     \
    return #name;

#define MAKE_TO_STRING_FUNC(enumType)            \
  inline const char *to_string(enumType e)       \
  {                                              \
    switch (e) {                                 \
      XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
                                                 \
          default : return "Unknown " #enumType; \
    }                                            \
  }

MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrFormFactor);
MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);

inline std::string
CheckXrResult(XrResult res, const char *originator, const char *source_location)
{
  std::ostringstream err;
  if (XR_FAILED(res)) {
    err << "XrResult failure [" << to_string(res) << "]" << std::endl;
    err << "    Origin: " << originator << std::endl;
    err << "    Source: " << source_location;
  }

  return err.str();
}

#define IF_XR_FAILED(err, cmd) \
  if (std::string err = CheckXrResult(cmd, #cmd, FILE_AND_LINE); !err.empty())

namespace Math {

inline XrQuaternionf
ToXrQuaternionf(glm::quat orientation)
{
  return XrQuaternionf{
      orientation.x, orientation.y, orientation.z, orientation.w};
}

inline XrVector3f
ToXrVector3f(glm::vec3 position)
{
  return XrVector3f{position.x, position.y, position.z};
}

inline XrPosef
ToXrPosef(glm::vec3 position, glm::quat orientation)
{
  return XrPosef{
      ToXrQuaternionf(orientation),
      ToXrVector3f(position),
  };
}

}  // namespace Math

}  // namespace

}  // namespace zen::display_system::oculus
