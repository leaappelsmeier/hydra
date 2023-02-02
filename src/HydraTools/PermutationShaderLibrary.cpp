#include <HydraRuntime/Logger.h>
#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>
#include <HydraTools/PermutationShaderLibrary.h>

namespace Hydra::Tools
{
  PermutationShaderLibrary::PermutationShaderLibrary() = default;

  void PermutationShaderLibrary::SetLogger(Runtime::ILoggingInterface* logger)
  {
    std::scoped_lock<std::recursive_mutex> lk(m_mutex);
    m_logger = logger;
  }

  void PermutationShaderLibrary::SetFileCache(FileCache* cache)
  {
    std::scoped_lock<std::recursive_mutex> lk(m_mutex);
    m_fileCache = cache;
  }

  void PermutationShaderLibrary::SetFileLocator(FileLocator* locator)
  {
    std::scoped_lock<std::recursive_mutex> lk(m_mutex);
    m_fileLocator = locator;
  }

  const Hydra::Tools::PermutationShader* PermutationShaderLibrary::GetLoadedPermutationShader(std::string_view path) const
  {
    if (m_fileCache == nullptr || m_fileLocator == nullptr)
    {
      Runtime::Log::Error(m_logger, "PermutationShaderLibrary: FileCache and FileLocator are not set up.");
      return nullptr;
    }

    std::string finalPath;
    finalPath = path;

    m_fileCache->NormalizeFilePath(finalPath);
    std::optional<std::string> filePath = m_fileLocator->FindFile(*m_fileCache, "", finalPath);

    if (!filePath.has_value())
    {
      // file not found -> not an error here
      Runtime::Log::Info(m_logger, "PermutationShaderLibrary::GetLoadedPermutationShader: File '%s' does not exist.", finalPath.c_str());
      return nullptr;
    }

    finalPath = filePath.value();

    std::scoped_lock<std::recursive_mutex> lk(m_mutex);

    std::map<std::string, PermutationShader>::const_iterator it = m_loadedShaders.find(finalPath);

    if (it != m_loadedShaders.end())
    {
      return &it->second;
    }

    Runtime::Log::Info(m_logger, "PermutationShaderLibrary::GetLoadedPermutationShader: File '%s' has not been loaded before.", finalPath.c_str());
    return nullptr;
  }

} // namespace Hydra::Tools
