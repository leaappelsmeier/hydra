#pragma once

#include <HydraRuntime/Result.h>
#include <string>

namespace Hydra::Runtime
{
  class ILoggingInterface;
  class PermutationManager;
} // namespace Hydra::Runtime

namespace Hydra::Tools
{
  class FileCache;
  class FileLocator;

  /// Helper class for loading permutation variables from a json file and registering them with a permutation manager
  ///
  /// This class is thread-safe.
  class PermutationVariableLoader
  {
  public:
    PermutationVariableLoader(Runtime::ILoggingInterface* logger);
    ~PermutationVariableLoader();

    void SetFileCache(FileCache* cache);
    void SetFileLocator(FileLocator* locator);

    /// Loads permutation variables from the given json file and registers them with the given permutation manager.
    Runtime::Result RegisterVariablesFromJsonFile(Runtime::PermutationManager& permMgr, std::string_view path, bool ignoreComments = false);

  private:
    Runtime::ILoggingInterface* m_logger = nullptr;
    FileCache* m_fileCache = nullptr;
    FileLocator* m_fileLocator = nullptr;
  };

} // namespace Hydra::Tools
