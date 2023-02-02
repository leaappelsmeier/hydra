#pragma once

#include <HydraRuntime/Result.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Hydra::Runtime
{
  struct ILoggingInterface;
}

namespace Hydra::Tools
{
  using PermutationVariableValues = std::map<std::string, int, std::less<>>; // The std::less<> is needed to allow for lookups with std::string_view

  struct PermutableTextPiece
  {
    enum class Type : uint8_t
    {
      Unconditional,
      If,
      Elif,
      Else,
      Endif,
    };

    Type m_type = Type::Unconditional;
    std::string_view m_text;
  };

  class PermutableText
  {
  public:
    /// Sets the text that should be permutable.
    ///
    /// Scans the text for occurrences of #[if] etc and prepares it to be be permuted.
    void SetText(const std::string& text);

    /// Returns the original text that was set, without any permutation.
    std::string_view GetOriginalText() const;

    /// Generates a permutation of the text, as described by the state of the permutation variables.
    std::optional<std::string> GenerateTextPermutation(const PermutationVariableValues& permutationVariables, Runtime::ILoggingInterface* logger) const;

    /// Checks all conditional pieces for which permutation variables they may read. No duplicate values are returned.
    Runtime::Result DetermineUsedPermutationVariables(std::vector<std::string>& foundVars, Runtime::ILoggingInterface* logger);

  private:
    Runtime::Result EnterBlock(const PermutationVariableValues& permutationVariables, size_t& blockIdx, std::string& output, Runtime::ILoggingInterface* logger) const;
    Runtime::Result SkipBlock(size_t& blockIdx, Runtime::ILoggingInterface* logger) const;

    std::string m_text;
    std::vector<PermutableTextPiece> m_pieces;
  };

} // namespace Hydra::Tools
