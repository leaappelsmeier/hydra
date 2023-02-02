#pragma once

namespace Hydra
{
  namespace Runtime
  {
    struct ILoggingInterface
    {
      virtual void LogInfo(const char* message) = 0;
      virtual void LogWarning(const char* message) = 0;
      virtual void LogError(const char* message) = 0;
    };

    class Log
    {
    public:
      static void Info(ILoggingInterface* logger, const char* formatStr, ...);
      static void Warning(ILoggingInterface* logger, const char* formatStr, ...);
      static void Error(ILoggingInterface* logger, const char* formatStr, ...);
    };
  } // namespace Runtime
} // namespace Hydra
