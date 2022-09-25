#include "pch.h"

#include "remote-log-sink.h"

namespace zen::display_system::oculus {

void
RemoteLogSink::Sink(remote::log::Severity remote_severity,
    const char* pretty_function, const char* file, int line, const char* format,
    va_list vp)
{
  ILogger::Severity severity = ILogger::Severity::INFO;
  switch (remote_severity) {
    case remote::log::DEBUG:
      severity = ILogger::DEBUG;
      break;
    case remote::log::INFO:
      severity = ILogger::INFO;
      break;
    case remote::log::WARN:
      severity = ILogger::WARN;
      break;
    case remote::log::ERROR:
      severity = ILogger::ERROR;
      break;
    case remote::log::FATAL:
      severity = ILogger::FATAL;
      break;
    default:
      break;
  }

  ILogger::instance->Printv(
      severity, "ZEN[Remote]", pretty_function, file, line, format, vp);
}

}  // namespace zen::display_system::oculus
