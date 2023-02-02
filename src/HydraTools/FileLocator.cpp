#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>
#include <cassert>

namespace Hydra::Tools
{

  std::optional<std::string> FileLocatorStd::FindFile(const FileCache& fileCache, std::string_view parentPath, std::string_view relativePath) const
  {
    if (relativePath.empty())
      return {};

    if (fileCache.ExistsFile(relativePath))
    {
      // if it is already an absolute path, we don't need to search the file
      return std::string(relativePath);
    }

    const bool relativeInclude = relativePath[0] == '"';

    if (relativePath[0] == '"' || relativePath[0] == '<')
      relativePath.remove_prefix(1);

    if (relativePath.back() == '"' || relativePath.back() == '>')
      relativePath.remove_suffix(1);

    if (relativeInclude)
    {
      return FindFileRelativeToParent(fileCache, parentPath, relativePath);
    }
    else
    {
      return FindFileInIncludeDirectories(fileCache, relativePath);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // FileLocatorStd

  void FileLocatorStd::AddIncludeDirectory(std::string_view path)
  {
    std::string pathWithSeparator = std::string(path);

    // TODO: only append a separator if necessary
    pathWithSeparator.append("/");

    m_includePaths.push_back(std::move(pathWithSeparator));
  }

  std::optional<std::string> FileLocatorStd::FindFileRelativeToParent(const FileCache& fileCache, std::string_view parentPath, std::string_view relativePath) const
  {
    // remove the 'filename' part using the '..'
    std::string fullpath = std::string(parentPath) + "/../" + std::string(relativePath);

    fileCache.NormalizeFilePath(fullpath);

    if (fileCache.ExistsFile(fullpath))
      return fullpath;

    return {};
  }

  std::optional<std::string> FileLocatorStd::FindFileInIncludeDirectories(const FileCache& fileCache, std::string_view relativePath) const
  {
    std::optional<std::string> result;

    std::string fullpath;

    // last added directory has highest priority
    for (size_t i = m_includePaths.size(); i > 0; --i)
    {
      const size_t idx = i - 1;

      fullpath = m_includePaths[idx];
      fullpath.append(relativePath);

      fileCache.NormalizeFilePath(fullpath);

      if (fileCache.ExistsFile(fullpath))
        return fullpath;
    }

    return {};
  }


} // namespace Hydra::Tools
