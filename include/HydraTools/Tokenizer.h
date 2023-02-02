#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydra::Runtime
{
  struct ILoggingInterface;
}

namespace Hydra::Tools
{
  struct Token
  {
  public:
    enum class Type
    {
      Unknown = 0,
      Identifier,
      NonIdentifier,
      Integer,
      NewLine,
      LineComment,
      BlockComment,
    };

    Type m_type = Type::Unknown;
    std::string_view m_token;
  };

  using TokenStream = std::vector<Token>;

  class Tokenizer
  {
  public:
    Tokenizer(Hydra::Runtime::ILoggingInterface* logger = nullptr);
    ~Tokenizer();

    const TokenStream& GetResult() const { return m_tokenStream; }

    void Tokenize(std::string_view input);

  private:
    Token::Type EvaluateTokenType();
    Token HandleIdentifier();
    Token HandleNonIdentifierAndNewLine();
    Token HandleInteger();
    Token HandleLineComment();
    Token HandleBlockComment();
    Token GetToken(uint32_t startPos);

    TokenStream m_tokenStream;
    std::string_view m_currentInput;
    uint32_t m_currentPos = 0;
    Token::Type m_currentType = Token::Type::Unknown;

    Hydra::Runtime::ILoggingInterface* m_logger = nullptr;
  };
} // namespace Hydra::Tools
