#pragma once

#define DISABLE_MOVE_AND_COPY(Class)        \
  Class(const Class &) = delete;            \
  Class(Class &&) = delete;                 \
  Class &operator=(const Class &) = delete; \
  Class &operator=(Class &&) = delete

#define TO_STRING(x) #x
#define MACRO_TO_STRING(x) TO_STRING(x)
#define FILE_AND_LINE __FILE__ ":" MACRO_TO_STRING(__LINE__)

[[noreturn]] inline void
Throw(std::string failure_message, const char *originator = nullptr,
    const char *source_location = nullptr)
{
  std::ostringstream err;
  err << failure_message;
  if (originator != nullptr) {
    err << std::endl << "    Origin: " << originator;
  }

  if (source_location != nullptr) {
    err << std::endl << "    Source: " << source_location;
  }

  throw std::logic_error(err.str());
}

#define CHECK(exp)                                \
  {                                               \
    if (!(exp)) {                                 \
      Throw("Check failed", #exp, FILE_AND_LINE); \
    }                                             \
  }
