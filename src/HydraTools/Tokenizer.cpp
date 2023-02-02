#include <HydraTools/Tokenizer.h>

#include <HydraRuntime/HydraRuntime.h>

namespace
{
  bool isIdentifierDelimiter(char curChar)
  {
    if ((curChar >= 'a') && (curChar <= 'z'))
      return false;
    if ((curChar >= 'A') && (curChar <= 'Z'))
      return false;
    if ((curChar >= '0') && (curChar <= '9'))
      return false;
    if (curChar == '_')
      return false;

    return true;
  }
} // namespace

namespace Hydra::Tools
{
  Tokenizer::Tokenizer(Hydra::Runtime::ILoggingInterface* logger)
    : m_logger(logger)
  {
  }

  Tokenizer::~Tokenizer() = default;

  void Tokenizer::Tokenize(std::string_view input)
  {
    m_currentInput = input;
    m_currentPos = 0;
    m_currentType = Token::Type::Unknown;
    m_tokenStream.clear();

    while (m_currentPos < input.size())
    {
      m_currentType = EvaluateTokenType();

      switch (m_currentType)
      {
        case Token::Type::Identifier:
          m_tokenStream.push_back(HandleIdentifier());
          break;

        case Token::Type::NonIdentifier:
          m_tokenStream.push_back(HandleNonIdentifierAndNewLine());
          break;

        case Token::Type::Integer:
          m_tokenStream.push_back(HandleInteger());
          break;

        case Token::Type::LineComment:
          m_tokenStream.push_back(HandleLineComment());
          break;

        case Token::Type::BlockComment:
          m_tokenStream.push_back(HandleBlockComment());
          break;
      }
    }
  }

  Token::Type Tokenizer::EvaluateTokenType()
  {
    char curChar = m_currentInput[m_currentPos];
    char nextChar = m_currentPos + 1 < m_currentInput.size() ? m_currentInput[m_currentPos + 1] : '\0';

    if (curChar == '/' && nextChar == '/')
    {
      return Token::Type::LineComment;
    }

    if (curChar == '/' && nextChar == '*')
    {
      return Token::Type::BlockComment;
    }

    if (curChar == ' ' || curChar == '\t')
    {
      ++m_currentPos;
      return Token::Type::Unknown;
    }

    if (std::isdigit(curChar))
    {
      return Token::Type::Integer;
    }

    if (!isIdentifierDelimiter(curChar))
    {
      return Token::Type::Identifier;
    }

    return Token::Type::NonIdentifier;
  }

  Hydra::Tools::Token Tokenizer::HandleIdentifier()
  {
    uint32_t startPos = m_currentPos;
    ++m_currentPos;
    char curChar;
    while (m_currentPos < m_currentInput.size())
    {
      if (isIdentifierDelimiter(m_currentInput[m_currentPos]))
      {
        // Check if we have an expression of type <Identifier>::<Identifier>, which we concatenate and treat as a single identifier
        if (m_currentInput.substr(m_currentPos).starts_with("::") && (m_currentPos + 2 < m_currentInput.size()) && !isIdentifierDelimiter(m_currentInput[m_currentPos + 2]) && !std::isdigit(m_currentInput[m_currentPos + 2]))
        {
          m_currentPos += 2;
        }
        else
        {
          break;
        }
      }

      ++m_currentPos;
    }

    return GetToken(startPos);
  }

  Token Tokenizer::HandleNonIdentifierAndNewLine()
  {
    uint32_t startPos = m_currentPos;
    if (m_currentInput[startPos] == '\n')
    {
      m_currentType = Token::Type::NewLine;
    }
    else if (m_currentInput[startPos] == '\r' && (startPos + 1 < m_currentInput.size()) && m_currentInput[startPos + 1] == '\n')
    {
      ++m_currentPos;
      m_currentType = Token::Type::NewLine;
    }

    ++m_currentPos;
    return GetToken(startPos);
  }

  Hydra::Tools::Token Tokenizer::HandleInteger()
  {
    uint32_t startPos = m_currentPos;

    if (m_currentInput[startPos] == '0' && (startPos + 2 < m_currentInput.size()) && (m_currentInput[startPos + 1] == 'x' || m_currentInput[startPos + 1] == 'X') && std::isxdigit(m_currentInput[startPos + 2]))
    {
      m_currentPos = startPos + 2;
      do
      {
        ++m_currentPos;
      } while (m_currentPos < m_currentInput.size() && std::isxdigit(m_currentInput[m_currentPos]));
    }
    else
    {
      do
      {
        ++m_currentPos;
      } while (m_currentPos < m_currentInput.size() && std::isdigit(m_currentInput[m_currentPos]));
    }

    return GetToken(startPos);
  }

  Hydra::Tools::Token Tokenizer::HandleLineComment()
  {
    uint32_t startPos = m_currentPos;
    m_currentPos += 2;
    while (m_currentPos < m_currentInput.size())
    {
      if (m_currentInput[m_currentPos] == '\n' || m_currentInput[m_currentPos] == '\r')
      {
        break;
      }
      ++m_currentPos;
    }

    return GetToken(startPos);
  }

  Hydra::Tools::Token Tokenizer::HandleBlockComment()
  {
    uint32_t startPos = m_currentPos;
    m_currentPos += 2;
    while (m_currentPos + 1 < m_currentInput.size())
    {
      if (m_currentInput[m_currentPos] == '*' && m_currentInput[m_currentPos + 1] == '/')
      {
        m_currentPos += 2;
        return GetToken(startPos);
      }
      ++m_currentPos;
    }

    // Consume the last remaining character if necessary
    m_currentPos = std::min<size_t>(m_currentPos + 1, m_currentInput.size());

    Hydra::Runtime::Log::Warning(m_logger, "Unclosed block comment: '%s'", std::string(m_currentInput.substr(startPos)).c_str());

    return GetToken(startPos);
  }

  Hydra::Tools::Token Tokenizer::GetToken(uint32_t startPos)
  {
    Token result{m_currentType, m_currentInput.substr(startPos, m_currentPos - startPos)};
    m_currentType = Token::Type::Unknown;
    return result;
  }

} // namespace Hydra::Tools
