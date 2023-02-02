#include <HydraRuntime/Logger.h>

#include <cstdarg>
#include <cstdio>

namespace Hydra::Runtime
{
  void Log::Info(ILoggingInterface* logger, const char* formatStr, ...)
  {
    if (logger != nullptr)
    {
      char msgBuf[1024];

      va_list args;
      va_start(args, formatStr);
#ifdef _MSC_VER
      vsnprintf_s(msgBuf, sizeof(msgBuf), formatStr, args);
#else
      vsnprintf(msgBuf, sizeof(msgBuf), formatStr, args);
#endif
      va_end(args);

      logger->LogInfo(msgBuf);
    }
  }

  void Log::Warning(ILoggingInterface* logger, const char* formatStr, ...)
  {
    if (logger != nullptr)
    {
      char msgBuf[1024];

      va_list args;
      va_start(args, formatStr);
#ifdef _MSC_VER
      vsnprintf_s(msgBuf, sizeof(msgBuf), formatStr, args);
#else
      vsnprintf(msgBuf, sizeof(msgBuf), formatStr, args);
#endif
      va_end(args);

      logger->LogWarning(msgBuf);
    }
  }

  void Log::Error(ILoggingInterface* logger, const char* formatStr, ...)
  {
    if (logger != nullptr)
    {
      char msgBuf[1024];

      va_list args;
      va_start(args, formatStr);
#ifdef _MSC_VER
      vsnprintf_s(msgBuf, sizeof(msgBuf), formatStr, args);
#else
      vsnprintf(msgBuf, sizeof(msgBuf), formatStr, args);
#endif
      va_end(args);

      logger->LogError(msgBuf);
    }
  }

} // namespace Hydra::Runtime
