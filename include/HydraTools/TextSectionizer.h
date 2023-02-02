#pragma once

#include <string>
#include <vector>

namespace Hydra::Tools
{
  /// A utility to determine where in a text sections start and end.
  ///
  /// A section starts with a unique given keyword, that shouldn't appear anywhere else in the entire text.
  /// Which sections are expected, is registered before using 'AddSection()'.
  /// After calling Process() the start and end of each section is known.
  /// The text to be processed is not copied, so make sure the string stays valid while using the TextSectionizer.
  /// Afterward one can retrieve a string_view to each section.
  class TextSectionizer
  {
  public:
    /// Registers a section keyword that is expected to be found in the text.
    void AddSection(std::string_view name);

    /// Searches the given text for all the registered sections.
    void Process(std::string_view text);

    /// Retrieves the string_view to the found section.
    /// out_firstLine is the line number of where the section starts.
    std::string_view GetSectionContent(uint32_t sectionIdx, uint32_t* out_firstLine = nullptr) const;

  private:
    struct Section
    {
      Section(std::string_view name)
      {
        m_name = name;
      }

      void Reset()
      {
        m_sectionStart = nullptr;
        m_content = {};
        m_firstLine = 0;
      }

      std::string m_name;
      const char* m_sectionStart = nullptr;
      std::string_view m_content;
      uint32_t m_firstLine = 0;
    };

    std::string_view m_fullText;
    std::vector<Section> m_sections;
  };
} // namespace Hydra::Tools
