#include <HydraTools/FileCache.h>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Hydra::Tools
{
  bool FileCache::ExistsFile(std::string_view normalizedPath) const
  {
    std::scoped_lock<std::recursive_mutex> lock(m_mutex);

    auto it = m_fileContents.find(normalizedPath);
    if (it != m_fileContents.end())
    {
      return true;
    }

    return ExistsFileOnDisk(normalizedPath);
  }

  const std::string& FileCache::GetFileContent(std::string_view normalizedPath)
  {
    std::scoped_lock<std::recursive_mutex> lock(m_mutex);

    auto it = m_fileContents.find(normalizedPath);
    if (it != m_fileContents.end())
    {
      return it->second;
    }

    std::string& content = m_fileContents[std::string(normalizedPath)]; // TODO: can we not convert to std::string ?

    content = std::move(ReadFileFromDisk(normalizedPath));

    return content;
  }

  void FileCache::ClearCache()
  {
    std::scoped_lock<std::recursive_mutex> lock(m_mutex);
    m_fileContents.clear();
  }

  //////////////////////////////////////////////////////////////////////////
  // FileCacheStdFileSystem

  void FileCacheStdFileSystem::NormalizeFilePath(std::string& path) const
  {
    // remove redundant "..", double separators, replace separators by the preferred OS one, etc.

    std::filesystem::path normPath(path);
    path = normPath.lexically_normal().string();
  }

  bool FileCacheStdFileSystem::ExistsFileOnDisk(std::string_view normalizedPath) const
  {
    std::filesystem::path fspath(normalizedPath);
    if (!fspath.is_absolute()) // we expect the given path to be absolute at this point
      return false;

    return std::filesystem::exists(fspath);
  }

  std::string FileCacheStdFileSystem::ReadFileFromDisk(std::string_view normalizedPath)
  {
    const auto fileSize = std::filesystem::file_size(normalizedPath);

    std::ifstream file(std::string(normalizedPath), std::ios::in | std::ios::binary);

    assert(file.is_open()); // should not fail because CanReadFile was supposed to be checked first.

    std::string content(fileSize, '\0');
    file.read(content.data(), fileSize);

    if (content.empty() || content.back() != '\n')
    {
      // make sure the file ends with a newline
      content += '\n';
    }

    return std::move(content);
  }

} // namespace Hydra::Tools
