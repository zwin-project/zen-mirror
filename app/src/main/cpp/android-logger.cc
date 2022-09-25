#include "pch.h"

#include "logger.h"

namespace zen::display_system::oculus {

namespace {

class AndroidLogger : public ILogger {
  virtual void Printv(Severity severity, const char* tag,
      const char* /*pretty_function*/, const char* /*file*/, int /*line*/,
      const char* format, va_list args) final
  {
    android_LogPriority priority = ANDROID_LOG_INFO;
    switch (severity) {
      case ILogger::DEBUG:
        priority = ANDROID_LOG_DEBUG;
        break;
      case ILogger::INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case ILogger::WARN:
        priority = ANDROID_LOG_WARN;
        break;
      case ILogger::ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case ILogger::FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
      default:
        break;
    }

    __android_log_vprint(priority, tag, format, args);
  }
};

}  // namespace

std::unique_ptr<ILogger> ILogger::instance;

void
InitializeLogger()
{
  ILogger::instance = std::make_unique<AndroidLogger>();
}

}  // namespace zen::display_system::oculus
