#include <HydraRuntime/Logger.h>
#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>
#include <HydraTools/StringUtils.h>

namespace Hydra::Tools
{
  bool StartsWith(std::string_view text, std::string_view search)
  {
    return text.rfind(search, 0) == 0;
  }

  std::string_view GetNextLine(std::string_view& text)
  {
    const char* lineStart = text.data();

    while (!text.empty())
    {
      const char c = text[0];
      text.remove_prefix(1);

      if (c == '\n')
        break;
    }

    return std::string_view(lineStart, text.data() - lineStart);
  }

  bool Accept(std::string_view& text, char c)
  {
    if (text.empty())
      return false;

    if (text[0] == c)
    {
      text.remove_prefix(1);
      return true;
    }

    return false;
  }

  bool Accept(std::string_view& text, const char* prefix)
  {
    if (text.empty())
      return false;

    if (StartsWith(text, prefix))
    {
      text.remove_prefix(strlen(prefix));
      return true;
    }

    return false;
  }

  void SkipWhitespace(std::string_view& text)
  {
    while (Accept(text, ' ') || Accept(text, '\t') || Accept(text, '\r') || Accept(text, '\n'))
    {
    }
  }

  void TrimWhitespaceAtEnd(std::string_view& text)
  {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n'))
    {
      text.remove_suffix(1);
    }
  }

  std::string ReplaceHashIncludes(std::string_view parentPath, std::string_view original, std::set<std::string>& alreadyIncluded, const FileLocator& fileLocator, FileCache& fileCache, Runtime::ILoggingInterface* logger)
  {
    std::string result;

    while (!original.empty())
    {
      std::string_view line = GetNextLine(original);
      std::string_view lineCopy = line;

      SkipWhitespace(lineCopy);
      if (Accept(lineCopy, '#'))
      {
        SkipWhitespace(lineCopy);

        if (Accept(lineCopy, "include"))
        {
          // found a #include
          SkipWhitespace(lineCopy);
          TrimWhitespaceAtEnd(lineCopy);

          bool globalInclude = true;

          if (auto targetFile = fileLocator.FindFile(fileCache, parentPath, lineCopy); targetFile.has_value())
          {
            if (!alreadyIncluded.contains(targetFile.value()))
            {
              alreadyIncluded.insert(targetFile.value());

              const std::string& targetFileContent = fileCache.GetFileContent(targetFile.value());

              // recursively replace #includes in the dependent files
              result += ReplaceHashIncludes(targetFile.value(), targetFileContent, alreadyIncluded, fileLocator, fileCache, logger);
            }

            continue;
          }
          else
          {
            Runtime::Log::Error(logger, "Couldn't locate file to #include: '%s'", std::string(lineCopy).c_str());
            // fall through -> this will append the original #include statement to the output
          }
        }
      }

      result += line;
    }

    return result;
  }

} // namespace Hydra::Tools
