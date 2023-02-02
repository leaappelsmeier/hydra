#include <HydraTools/TextSectionizer.h>

namespace Hydra::Tools
{
  void TextSectionizer::AddSection(std::string_view name)
  {
    m_sections.push_back(Section(name));
  }

  void TextSectionizer::Process(std::string_view text)
  {
    for (auto& section : m_sections)
    {
      section.Reset();
    }

    m_fullText = text;

    for (auto& section : m_sections)
    {
      size_t foundPos = m_fullText.find(section.m_name.c_str());

      if (foundPos != std::string::npos)
      {
        section.m_sectionStart = &m_fullText[foundPos];

        const char* start = section.m_sectionStart + section.m_name.length();

        section.m_content = std::string_view(start, text.data() + text.size() - start);
      }
    }

    for (size_t s1 = 0; s1 < m_sections.size(); ++s1)
    {
      Section& section1 = m_sections[s1];

      if (section1.m_sectionStart == nullptr)
        continue;

      uint32_t lineNumber = 1;

      const char* sz = m_fullText.data();
      while (sz < section1.m_sectionStart)
      {
        if (*sz == '\n')
        {
          ++lineNumber;
        }

        ++sz;
      }

      section1.m_firstLine = lineNumber;

      for (size_t s2 = 0; s2 < m_sections.size(); ++s2)
      {
        if (s1 == s2)
          continue;

        Section& section2 = m_sections[s2];

        if ((section2.m_sectionStart > section1.m_sectionStart) ||
            ((section2.m_sectionStart == section1.m_sectionStart) && (section1.m_name.empty()))) // this allows a single section at the start without a header (containing everything before the first section)
        {
          const char* contentStart = section1.m_content.data();
          const char* sectionEnd = std::min(section1.m_content.data() + section1.m_content.size(), section2.m_sectionStart);

          section1.m_content = std::string_view(contentStart, sectionEnd - contentStart);
        }
      }
    }
  }

  std::string_view TextSectionizer::GetSectionContent(uint32_t sectionIdx, uint32_t* out_firstLine /*= nullptr*/) const
  {
    if (out_firstLine)
    {
      *out_firstLine = m_sections[sectionIdx].m_firstLine;
    }

    return m_sections[sectionIdx].m_content;
  }

} // namespace Hydra::Tools
