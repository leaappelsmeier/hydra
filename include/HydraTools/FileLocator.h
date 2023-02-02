#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Hydra::Tools
{
  class FileCache;

  /// Used to search for a file, whose path is given in some context.
  ///
  /// This is mainly needed to resolve #include's and such, where paths are given in the forms:
  ///   "Relative/To/Current/File.h"
  ///   <Relative/To/Include/Directories.h>
  ///
  /// Other engines may of course use different methods.
  class FileLocator
  {
  public:
    virtual std::optional<std::string> FindFile(const FileCache& fileCache, std::string_view parentPath, std::string_view relativePath) const = 0;
  };

  /// Default implementation of the FileLocator that searches for files relative to the parent path or include directories,
  /// depending on whether the path is given in quotes (") or brackets (< >).
  class FileLocatorStd : public FileLocator
  {
  public:
    void AddIncludeDirectory(std::string_view path);

    virtual std::optional<std::string> FindFile(const FileCache& fileCache, std::string_view parentPath, std::string_view relativePath) const override;

  protected:
    std::optional<std::string> FindFileInIncludeDirectories(const FileCache& fileCache, std::string_view relativePath) const;
    std::optional<std::string> FindFileRelativeToParent(const FileCache& fileCache, std::string_view parentPath, std::string_view relativePath) const;

    std::vector<std::string> m_includePaths;
  };

} // namespace Hydra::Tools
