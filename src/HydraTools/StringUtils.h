#pragma once

#include <set>
#include <string>

namespace Hydra::Runtime
{
  struct ILoggingInterface;
}

namespace Hydra::Tools
{
  class FileLocator;
  class FileCache;

  /// Returns whether 'text' starts with 'search'
  bool StartsWith(std::string_view text, std::string_view search);

  /// Returns the view from the beginning of 'text' up until (and including) the next '\n'
  /// Modifies 'text' such that the returned line is then excluded from it.
  std::string_view GetNextLine(std::string_view& text);

  /// Returns true if 'text' starts with 'c'.
  /// Removes the first character from 'text'.
  /// If 'text' does not start with 'c', returns false and does not modify 'text'.
  bool Accept(std::string_view& text, char c);

  /// Variant of Accept() that checks for a longer prefix.
  bool Accept(std::string_view& text, const char* prefix);

  /// Modifies 'text' to not start with any whitespace.
  void SkipWhitespace(std::string_view& text);

  /// Modifies 'text' to not end with any whitespace.
  void TrimWhitespaceAtEnd(std::string_view& text);

  std::string ReplaceHashIncludes(std::string_view parentPath, std::string_view original, std::set<std::string>& alreadyIncluded, const FileLocator& fileLocator, FileCache& fileCache, Runtime::ILoggingInterface* logger);

} // namespace Hydra::Tools
