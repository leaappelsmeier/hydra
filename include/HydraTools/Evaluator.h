#pragma once
#include <HydraRuntime/Result.h>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hydra::Runtime
{
  struct ILoggingInterface;
}

namespace Hydra::Tools
{
  struct Token;
  using TokenStream = std::vector<Token>;

  using ValueTable = std::map<std::string, int, std::less<>>; // The std::less<> is needed to allow for lookups with std::string_view
  using ValueList = std::unordered_set<std::string_view>;

  class Evaluator
  {
  public:
    enum class Mode
    {
      Strict,  // Require all used variables to be defined
      Lenient, // Assume value 0 for undefined variables
    };

    Evaluator(Hydra::Runtime::ILoggingInterface* logger = nullptr);
    ~Evaluator();

    Hydra::Runtime::Result EvaluateCondition(std::string_view input,
      const ValueTable& values,
      int& result_out,
      Mode mode = Mode::Strict,
      ValueList* usedValues_out = nullptr);
    Hydra::Runtime::Result EvaluateCondition(const TokenStream& input,
      const ValueTable& values,
      int& result_out,
      Mode mode = Mode::Strict,
      ValueList* usedValues_out = nullptr);

  private:
    Hydra::Runtime::ILoggingInterface* m_logger = nullptr;
  };

  void TestEvaluator(Hydra::Runtime::ILoggingInterface* logger = nullptr);

} // namespace Hydra::Tools
