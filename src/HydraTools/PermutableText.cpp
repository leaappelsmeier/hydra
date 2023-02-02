#include <HydraRuntime/Logger.h>
#include <HydraRuntime/Result.h>
#include <HydraTools/Evaluator.h>
#include <HydraTools/PermutableText.h>
#include <HydraTools/StringUtils.h>
#include <cassert>

namespace Hydra::Tools
{

  static std::string_view GetNextPiece(std::string_view& text, std::string_view& nextCondition)
  {
    const char* pieceStart = text.data();

    std::string_view piece = text;

    while (!piece.empty())
    {
      const char* pieceEnd = piece.data();

      std::string_view line = GetNextLine(piece);

      SkipWhitespace(line);
      if (Accept(line, '#'))
      {
        SkipWhitespace(line);
        if (Accept(line, '['))
        {
          // line starts with '#[' (plus optional whitespace)

          nextCondition = line;

          const size_t pieceLength = pieceEnd - pieceStart;

          text = piece;

          return std::string_view(pieceStart, pieceLength);
        }
      }
    }


    nextCondition = {};
    piece = text;
    text = {};

    return piece;
  }

  static PermutableTextPiece::Type DeterminePieceType(std::string_view& line)
  {
    SkipWhitespace(line);
    PermutableTextPiece::Type result = PermutableTextPiece::Type::Unconditional;

    if (Accept(line, "if"))
    {
      result = PermutableTextPiece::Type::If;
    }

    if (Accept(line, "elif"))
    {
      result = PermutableTextPiece::Type::Elif;
    }

    if (Accept(line, "else"))
    {
      result = PermutableTextPiece::Type::Else;
    }

    if (Accept(line, "endif"))
    {
      result = PermutableTextPiece::Type::Endif;
    }

    SkipWhitespace(line);
    TrimWhitespaceAtEnd(line);

    // remove expected ']' at end
    // TODO: currently not an error, if the ] is missing
    if (!line.empty() && line.back() == ']')
    {
      line.remove_suffix(1);
    }

    TrimWhitespaceAtEnd(line);

    return result;
  }

  void PermutableText::SetText(const std::string& fullText)
  {
    m_text = fullText;

    std::string_view text = m_text;

    while (!text.empty())
    {
      std::string_view nextCondition;
      std::string_view piece = GetNextPiece(text, nextCondition);

      if (!piece.empty())
      {
        PermutableTextPiece cb;
        cb.m_type = PermutableTextPiece::Type::Unconditional;
        cb.m_text = piece;
        m_pieces.push_back(cb);
      }

      if (!nextCondition.empty())
      {
        PermutableTextPiece cb;
        cb.m_type = DeterminePieceType(nextCondition);
        cb.m_text = nextCondition;
        m_pieces.push_back(cb);
      }
    }
  }

  std::string_view PermutableText::GetOriginalText() const
  {
    return m_text;
  }

  Runtime::Result PermutableText::EnterBlock(const ValueTable& permutationVariables, size_t& blockIdx, std::string& output, Runtime::ILoggingInterface* logger) const
  {
    bool foundIf = false;
    bool foundTrueCondition = false;

    Evaluator evaluator(logger);

    while (blockIdx < m_pieces.size())
    {
      const PermutableTextPiece& block = m_pieces[blockIdx];

      switch (block.m_type)
      {
        case PermutableTextPiece::Type::Unconditional:
        {
          output += block.m_text;
          ++blockIdx;
          break;
        }

        case PermutableTextPiece::Type::If:

          if (foundIf)
          {
            Runtime::Log::Error(logger, "Permutable text structure is malformed.");
            return Runtime::HYDRA_FAILURE;
          }

          foundIf = true;
          foundTrueCondition = false;
          [[fallthrough]];

        case PermutableTextPiece::Type::Elif:
        {
          if (!foundIf)
          {
            return Runtime::HYDRA_SUCCESS;
          }

          int conditionValue = 0;
          if (!foundTrueCondition)
          {
            if (evaluator.EvaluateCondition(block.m_text, permutationVariables, conditionValue).Failed())
            {
              return Runtime::HYDRA_FAILURE;
            }
          }

          if (!foundTrueCondition && conditionValue != 0)
          {
            foundTrueCondition = true;

            ++blockIdx;
            if (EnterBlock(permutationVariables, blockIdx, output, logger).Failed())
            {
              return Runtime::HYDRA_FAILURE;
            }
          }
          else
          {
            ++blockIdx;
            if (SkipBlock(blockIdx, logger).Failed())
            {
              return Runtime::HYDRA_FAILURE;
            }
          }

          break;
        }

        case PermutableTextPiece::Type::Else:
        {
          if (!foundIf)
          {
            return Runtime::HYDRA_SUCCESS;
          }

          if (!foundTrueCondition)
          {
            ++blockIdx;
            if (EnterBlock(permutationVariables, blockIdx, output, logger).Failed())
            {
              return Runtime::HYDRA_FAILURE;
            }
          }
          else
          {
            ++blockIdx;
            if (SkipBlock(blockIdx, logger).Failed())
            {
              return Runtime::HYDRA_FAILURE;
            }
          }

          break;
        }

        case PermutableTextPiece::Type::Endif:
        {
          if (foundIf)
          {
            ++blockIdx;
            foundIf = false;
            foundTrueCondition = false;
            break;
          }
          else
          {
            return Runtime::HYDRA_SUCCESS;
          }
        }
      }
    }

    if (foundIf)
    {
      Runtime::Log::Error(logger, "Permutable text structure is not finished properly.");
      return Runtime::HYDRA_FAILURE;
    }

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutableText::SkipBlock(size_t& blockIdx, Runtime::ILoggingInterface* logger) const
  {
    int32_t nesting = 0;

    for (; blockIdx < m_pieces.size(); ++blockIdx)
    {
      const PermutableTextPiece& block = m_pieces[blockIdx];

      switch (block.m_type)
      {
        case PermutableTextPiece::Type::If:
          ++nesting;
          break;

        case PermutableTextPiece::Type::Endif:

          if (nesting == 0)
          {
            return Runtime::HYDRA_SUCCESS;
          }

          --nesting;
          break;

        case PermutableTextPiece::Type::Elif:
        case PermutableTextPiece::Type::Else:

          if (nesting == 0)
          {
            return Runtime::HYDRA_SUCCESS;
          }

          break;
      }
    }

    if (nesting == 0)
    {
      return Runtime::HYDRA_SUCCESS;
    }

    Runtime::Log::Error(logger, "Permutable text structure is malformed.");
    return Runtime::HYDRA_FAILURE;
  }

  std::optional<std::string> PermutableText::GenerateTextPermutation(const PermutationVariableValues& permutationVariables, Runtime::ILoggingInterface* logger) const
  {
    std::string result;
    size_t blockIdx = 0;

    while (blockIdx < m_pieces.size())
    {
      if (EnterBlock(permutationVariables, blockIdx, result, logger).Failed())
      {
        Runtime::Log::Error(logger, "Failed to generate text permutation.");
        return {};
      }
    }

    return result;
  }

  Runtime::Result PermutableText::DetermineUsedPermutationVariables(std::vector<std::string>& foundVars, Runtime::ILoggingInterface* logger)
  {
    Runtime::Result result = Runtime::HYDRA_SUCCESS;

    // NOTE: only the variables used in this module are extracted
    // to get the full set, one would need to load all dependent modules and gather their variables as well

    ValueList evaluatedVars;

    Evaluator evaluator(logger);

    for (const PermutableTextPiece& piece : m_pieces)
    {
      if (piece.m_type == PermutableTextPiece::Type::If || piece.m_type == PermutableTextPiece::Type::Elif)
      {
        int result = 0;
        if (evaluator.EvaluateCondition(piece.m_text, {}, result, Evaluator::Mode::Lenient, &evaluatedVars).Succeeded())
        {
          result = Runtime::HYDRA_FAILURE;
        }
      }
    }

    for (std::string_view varName : evaluatedVars)
    {
      foundVars.push_back(std::string(varName));
    }

    return result;
  }

} // namespace Hydra::Tools
