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
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);

/* Convert XrVersion to a human readable version string */
inline std::string
GetXrVersionString(XrVersion version)
{
  std::ostringstream oss;
  oss << XR_VERSION_MAJOR(version) << ".";
  oss << XR_VERSION_MINOR(version) << ".";
  oss << XR_VERSION_PATCH(version);
  return oss.str();
}

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
ToXr(glm::quat quat)
{
  return XrQuaternionf{quat.x, quat.y, quat.z, quat.w};
}

inline glm::quat
ToGlm(XrQuaternionf quat)
{
  return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

inline XrVector3f
ToXr(glm::vec3 vec)
{
  return XrVector3f{vec.x, vec.y, vec.z};
}

inline glm::vec3
ToGlm(XrVector3f vec)
{
  return glm::vec3(vec.x, vec.y, vec.z);
}

inline XrPosef
ToXrPosef(glm::vec3 position, glm::quat orientation)
{
  return XrPosef{
      ToXr(orientation),
      ToXr(position),
  };
}

inline glm::mat4
ToProjectionMatrix(XrFovf fov, float near, float far)
{
  const float tanLeft = tanf(fov.angleLeft);
  const float tanRight = tanf(fov.angleRight);
  const float tanDown = tanf(fov.angleDown);
  const float tanUp = tanf(fov.angleUp);

  const float tanWidth = tanRight - tanLeft;
  const float tanHeight = tanUp - tanDown;

  const float a = 2 / tanWidth;
  const float b = (tanRight + tanLeft) / tanWidth;
  const float c = 2 / tanHeight;
  const float d = (tanUp + tanDown) / tanHeight;
  const float e = far / (near - far);
  const float f = e * near;

  // [0, 1] Z clip space, col major
  return glm::mat4(  //
      a, 0, 0, 0,    //
      0, c, 0, 0,    //
      b, d, e, -1,   //
      0, 0, f, 0     //
  );
}

}  // namespace Math

}  // namespace

}  // namespace zen::display_system::oculus
