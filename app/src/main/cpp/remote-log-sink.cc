#include "pch.h"

#include "remote-log-sink.h"

namespace zen::mirror {

void
RemoteLogSink::Sink(remote::Severity remote_severity,
    const char* pretty_function, const char* file, int line, const char* format,
    va_list vp)
{
  ILogger::Severity severity = ILogger::Severity::INFO;
  switch (remote_severity) {
    case remote::Severity::DEBUG:
      severity = ILogger::DEBUG;
      break;
    case remote::Severity::INFO:
      severity = ILogger::INFO;
      break;
    case remote::Severity::WARN:
      severity = ILogger::WARN;
      break;
    case remote::Severity::ERROR:
      severity = ILogger::ERROR;
      break;
    case remote::Severity::FATAL:
      severity = ILogger::FATAL;
      break;
    default:
      break;
  }

  ILogger::instance->Printv(
      severity, "ZEN[Remote]", pretty_function, file, line, format, vp);
}

}  // namespace zen::mirror
