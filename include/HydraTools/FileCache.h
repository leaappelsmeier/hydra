#pragma once

#include <map>
#include <mutex>
#include <string>

namespace Hydra::Tools
{
  /// Used for all file accesses. Repeated access to the same file returns a cached result.
  ///
  /// This class is thread-safe.
  class FileCache
  {
  public:
    /// Modifies the path such that different paths to the same file end up being identical.
    ///
    /// Typically this means that the path becomes an absolute path.
    virtual void NormalizeFilePath(std::string& path) const = 0;

    /// Checks whether there exists a file on disk with the given normalized path.
    bool ExistsFile(std::string_view normalizedPath) const;

    /// Returns the content of the file with the given normalized path.
    ///
    /// Expects that the user made sure that the file exists beforehand.
    /// Repeated calls to the same file will return a cached result.
    const std::string& GetFileContent(std::string_view normalizedPath);

    /// Removes all cached data. Future accesses will thus re-read files from disk.
    void ClearCache();

  protected:
    /// Checks whether the file with the given path is generally readable (exists).
    virtual bool ExistsFileOnDisk(std::string_view normalizedPath) const = 0;

    /// Reads the file and returns its entire content.
    virtual std::string ReadFileFromDisk(std::string_view normalizedPath) = 0;

    mutable std::recursive_mutex m_mutex;
    std::map<std::string, std::string, std::less<>> m_fileContents;
  };

  /// A default implementation of FileCache, using std::filesystem.
  class FileCacheStdFileSystem : public FileCache
  {
  public:
    virtual void NormalizeFilePath(std::string& path) const override;

  protected:
    virtual bool ExistsFileOnDisk(std::string_view normalizedPath) const override;
    virtual std::string ReadFileFromDisk(std::string_view normalizedPath) override;
  };

} // namespace Hydra::Tools
