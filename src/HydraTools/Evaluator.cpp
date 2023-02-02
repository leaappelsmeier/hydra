#include <HydraTools/Evaluator.h>
#include <HydraTools/Tokenizer.h>

#include <HydraRuntime/HydraRuntime.h>

#include <assert.h>
#include <charconv>
#include <iostream>
#include <string_view>


using namespace Hydra::Runtime;

namespace
{
  using namespace Hydra::Tools;
  using namespace std::literals;

  enum class Comparison
  {
    None,
    Equal,
    Unequal,
    LessThan,
    GreaterThan,
    LessThanEqual,
    GreaterThanEqual
  };

  void SkipComments(const TokenStream& input, uint32_t& currentToken)
  {
    while (currentToken < input.size() &&
           (input[currentToken].m_type == Token::Type::LineComment || input[currentToken].m_type == Token::Type::BlockComment))
    {
      ++currentToken;
    }
  }

  bool Accept(const TokenStream& input, uint32_t& currentToken, std::string_view token)
  {
    SkipComments(input, currentToken);

    if (currentToken >= input.size())
      return false;

    if (input[currentToken].m_token == token)
    {
      ++currentToken;
      return true;
    }

    return false;
  }

  bool Accept(const TokenStream& input, uint32_t& currentToken, std::string_view token1, std::string_view token2)
  {
    SkipComments(input, currentToken);

    if (currentToken + 1 >= input.size())
    {
      return false;
    }

    if (input[currentToken].m_token == token1 && input[currentToken + 1].m_token == token2)
    {
      currentToken += 2;
      return true;
    }

    return false;
  }

  bool AcceptUnless(const TokenStream& input, uint32_t& currentToken, std::string_view token1, std::string_view token2)
  {
    SkipComments(input, currentToken);

    if (currentToken + 1 >= input.size())
      return false;

    if (input[currentToken].m_token == token1 && input[currentToken + 1].m_token != token2)
    {
      currentToken += 1;
      return true;
    }

    return false;
  }

  bool Accept(const TokenStream& input, uint32_t& currentToken, Token::Type tokenType, uint32_t* pAccepted)
  {
    SkipComments(input, currentToken);

    if (currentToken >= input.size())
      return false;

    if (input[currentToken].m_type == tokenType)
    {
      if (pAccepted)
        *pAccepted = currentToken;

      ++currentToken;
      return true;
    }

    return false;
  }

  Result Expect(const TokenStream& input, uint32_t& currentToken, std::string_view token, ILoggingInterface* logger)
  {
    if (Accept(input, currentToken, token))
      return HYDRA_SUCCESS;

    Log::Error(logger, "Evaluator expected token '%s' instead of '%s'",
      std::string(token).c_str(),
      std::string(input[std::min<size_t>(currentToken, input.size() - 1)].m_token).c_str());

    return HYDRA_FAILURE;
  }

  Result ExpectEndOfLineOrInput(const TokenStream& input, uint32_t& currentToken, ILoggingInterface* logger)
  {
    SkipComments(input, currentToken);

    if (currentToken >= input.size())
      return HYDRA_SUCCESS;

    if (Accept(input, currentToken, Token::Type::NewLine, nullptr))
      return HYDRA_SUCCESS;

    Log::Error(logger,
      "Evaluator expected end-of-line token or end of input instead of token '%s'",
      std::string(input[std::min<size_t>(currentToken, input.size() - 1)].m_token).c_str());

    return HYDRA_FAILURE;
  }

  struct ParseInput
  {
    const TokenStream& input;
    const ValueTable& values;
    Evaluator::Mode mode;
    ValueList* usedValues_out;
    ILoggingInterface* logger;
  };

  Result ParseExpressionOr(const ParseInput& pi, uint32_t& currentToken, int64_t& result);

  Result ParseFactor(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    while (Accept(pi.input, currentToken, "+"))
    {
    }

    if (Accept(pi.input, currentToken, "-"))
    {
      if (ParseFactor(pi, currentToken, result).Failed())
        return HYDRA_FAILURE;

      result = -result;
      return HYDRA_SUCCESS;
    }

    if (Accept(pi.input, currentToken, "~"))
    {
      if (ParseFactor(pi, currentToken, result).Failed())
        return HYDRA_FAILURE;

      result = ~result;
      return HYDRA_SUCCESS;
    }

    if (Accept(pi.input, currentToken, "!"))
    {
      if (ParseFactor(pi, currentToken, result).Failed())
        return HYDRA_FAILURE;

      result = (result != 0) ? 0 : 1;
      return HYDRA_SUCCESS;
    }

    uint32_t valueToken = 0;
    if (Accept(pi.input, currentToken, Token::Type::Identifier, &valueToken))
    {
      using namespace std::literals;
      std::string_view token = pi.input[valueToken].m_token;
      if (token == "true")
      {
        result = 1;
      }
      else if (token == "false")
      {
        result = 0;
      }
      else
      {
        if (pi.usedValues_out)
        {
          pi.usedValues_out->insert(token);
        }

        // Try to find variable name in value table...
        if (auto it = pi.values.find(token); it != pi.values.end())
        {
          result = it->second;
        }
        else if (pi.mode == Evaluator::Mode::Lenient)
        {
          result = 0;
        }
        else
        {
          Log::Error(pi.logger, "No value specified for identifier '%s'", std::string(token).c_str());
          result = 0;
          return HYDRA_FAILURE;
        }
      }

      return HYDRA_SUCCESS;
    }
    else if (Accept(pi.input, currentToken, Token::Type::Integer, &valueToken))
    {
      std::string_view token = pi.input[valueToken].m_token;
      int value = 0;
      if (token.starts_with("0x") || token.starts_with("0X"))
      {
        auto ret = std::from_chars(token.data() + 2, token.data() + token.size(), value, 16);
        assert(ret.ec != std::errc::invalid_argument);
      }
      else
      {
        auto ret = std::from_chars(token.data(), token.data() + token.size(), value);
        assert(ret.ec != std::errc::invalid_argument);
      }
      result = static_cast<int64_t>(value);
      return HYDRA_SUCCESS;
    }
    else if (Accept(pi.input, currentToken, "("))
    {
      if (ParseExpressionOr(pi, currentToken, result).Failed())
        return HYDRA_FAILURE;

      return Expect(pi.input, currentToken, ")", pi.logger);
    }

    Log::Error(pi.logger,
      "Syntax error - expected identifier, number, or '(' instead of '%s'",
      std::string(pi.input[std::min<size_t>(currentToken, pi.input.size() - 1)].m_token).c_str());

    return HYDRA_FAILURE;
  }

  Result ParseExpressionMul(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseFactor(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (true)
    {
      if (Accept(pi.input, currentToken, "*"))
      {
        int64_t nextValue = 0;
        if (ParseFactor(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result *= nextValue;
      }
      else if (Accept(pi.input, currentToken, "/"))
      {
        int64_t nextValue = 0;
        if (ParseFactor(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result /= nextValue;
      }
      else if (Accept(pi.input, currentToken, "%"))
      {
        int64_t nextValue = 0;
        if (ParseFactor(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result %= nextValue;
      }
      else
        break;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseExpressionPlus(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionMul(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (true)
    {
      if (Accept(pi.input, currentToken, "+"))
      {
        int64_t nextValue = 0;
        if (ParseExpressionMul(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result += nextValue;
      }
      else if (Accept(pi.input, currentToken, "-"))
      {
        int64_t nextValue = 0;
        if (ParseExpressionMul(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result -= nextValue;
      }
      else
        break;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseExpressionShift(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionPlus(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (true)
    {
      if (Accept(pi.input, currentToken, ">", ">"))
      {
        int64_t nextValue = 0;
        if (ParseExpressionPlus(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result >>= nextValue;
      }
      else if (Accept(pi.input, currentToken, "<", "<"))
      {
        int64_t nextValue = 0;
        if (ParseExpressionPlus(pi, currentToken, nextValue).Failed())
          return HYDRA_FAILURE;

        result <<= nextValue;
      }
      else
        break;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseCondition(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    int64_t result1 = 0;
    if (ParseExpressionShift(pi, currentToken, result1).Failed())
      return HYDRA_FAILURE;

    Comparison Operator = Comparison::None;

    if (Accept(pi.input, currentToken, "=", "="))
      Operator = Comparison::Equal;
    else if (Accept(pi.input, currentToken, "!", "="))
      Operator = Comparison::Unequal;
    else if (Accept(pi.input, currentToken, ">", "="))
      Operator = Comparison::GreaterThanEqual;
    else if (Accept(pi.input, currentToken, "<", "="))
      Operator = Comparison::LessThanEqual;
    else if (AcceptUnless(pi.input, currentToken, ">", ">"))
      Operator = Comparison::GreaterThan;
    else if (AcceptUnless(pi.input, currentToken, "<", "<"))
      Operator = Comparison::LessThan;
    else
    {
      result = result1;
      return HYDRA_SUCCESS;
    }

    int64_t result2 = 0;
    if (ParseExpressionShift(pi, currentToken, result2).Failed())
      return HYDRA_FAILURE;

    switch (Operator)
    {
      case Comparison::Equal:
        result = (result1 == result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::GreaterThan:
        result = (result1 > result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::GreaterThanEqual:
        result = (result1 >= result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::LessThan:
        result = (result1 < result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::LessThanEqual:
        result = (result1 <= result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::Unequal:
        result = (result1 != result2) ? 1 : 0;
        return HYDRA_SUCCESS;
      case Comparison::None:
        Log::Error(pi.logger,
          "Unknown operator '%s'",
          std::string(pi.input[std::min<size_t>(currentToken, pi.input.size() - 1)].m_token).c_str());
        return HYDRA_FAILURE;
    }

    return HYDRA_FAILURE;
  }

  Result ParseExpressionBitAnd(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseCondition(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (AcceptUnless(pi.input, currentToken, "&", "&"))
    {
      int64_t nextValue = 0;
      if (ParseCondition(pi, currentToken, nextValue).Failed())
        return HYDRA_FAILURE;

      result &= nextValue;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseExpressionBitXor(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionBitAnd(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (Accept(pi.input, currentToken, "^"))
    {
      int64_t nextValue = 0;
      if (ParseExpressionBitAnd(pi, currentToken, nextValue).Failed())
        return HYDRA_FAILURE;

      result ^= nextValue;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseExpressionBitOr(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionBitXor(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (AcceptUnless(pi.input, currentToken, "|", "|"))
    {
      int64_t nextValue = 0;
      if (ParseExpressionBitXor(pi, currentToken, nextValue).Failed())
        return HYDRA_FAILURE;

      result |= nextValue;
    }

    return HYDRA_SUCCESS;
  }


  Result ParseExpressionAnd(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionBitOr(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (Accept(pi.input, currentToken, "&", "&"))
    {
      int64_t nextValue = 0;
      if (ParseExpressionBitOr(pi, currentToken, nextValue).Failed())
        return HYDRA_FAILURE;

      result = (result != 0 && nextValue != 0) ? 1 : 0;
    }

    return HYDRA_SUCCESS;
  }

  Result ParseExpressionOr(const ParseInput& pi, uint32_t& currentToken, int64_t& result)
  {
    if (ParseExpressionAnd(pi, currentToken, result).Failed())
      return HYDRA_FAILURE;

    while (Accept(pi.input, currentToken, "|", "|"))
    {
      int64_t nextValue = 0;
      if (ParseExpressionAnd(pi, currentToken, nextValue).Failed())
        return HYDRA_FAILURE;

      result = (result != 0 || nextValue != 0) ? 1 : 0;
    }

    return HYDRA_SUCCESS;
  }
} // namespace

namespace Hydra
{
  namespace Tools
  {

    Evaluator::Evaluator(Hydra::Runtime::ILoggingInterface* logger)
      : m_logger(logger)
    {
    }

    Evaluator::~Evaluator() = default;

    Result Evaluator::EvaluateCondition(
      const TokenStream& input, const ValueTable& values, int& result_out, Mode mode, ValueList* usedValues_out)
    {
      int64_t result = 0;
      uint32_t currentToken = 0;

      SkipComments(input, currentToken);
      if (currentToken >= input.size())
      {
        if (mode == Mode::Strict)
        {
          Log::Error(m_logger, "Empty expression");
          return HYDRA_FAILURE;
        }
      }
      else
      {
        ParseInput state = {input, values, mode, usedValues_out, m_logger};
        if (ParseExpressionOr(state, currentToken, result).Failed())
        {
          return HYDRA_FAILURE;
        }
      }

      result_out = static_cast<int>(result);
      return ExpectEndOfLineOrInput(input, currentToken, m_logger);
    }

    Result Evaluator::EvaluateCondition(
      std::string_view input, const ValueTable& values, int& result_out, Mode mode, ValueList* usedValues_out)
    {
      Tokenizer tokenizer;
      tokenizer.Tokenize(input);
      return EvaluateCondition(tokenizer.GetResult(), values, result_out, mode, usedValues_out);
    }


    // Rough test function ----------------------------------------------
    void TestEvaluator(Hydra::Runtime::ILoggingInterface* logger)
    {
      if (true)
      {
        std::vector<std::string> testStrings = {"10",
          "20",
          "0x20",
          "-10",
          "-0x20"};
        for (const auto& item : testStrings)
        {
          int val = 0;
          std::from_chars_result res;
          if (item.starts_with("0x") || item.starts_with("0X"))
          {
            res = std::from_chars(item.data() + 2, item.data() + item.size(), val, 16);
          }
          else
          {
            res = std::from_chars(item.data(), item.data() + item.size(), val);
          }
          std::cout << item << " --> " << val << " - ec: " << static_cast<uint32_t>(res.ec) << std::endl;
        }
      }

      ValueTable values;
      values["A"] = 1;
      values["B"] = 2;
      values["C"] = -3;
      values["D"] = -4;
      values["SetValue"] = 10;
      values["A10"] = 15;
      values["A::B"] = 42;

      std::vector<std::string> conditions = {
        "SetValue",
        "UnsetValue",
        "UnsetValue1 || UnsetValue2",
        "true",
        "20",
        "0x20",
        "0X20",
        "0x010",
        "-0x20",
        "0x10 | 0x01",
        "0x7 & 0x13",
        "0xABCD",
        "false",
        "A||B",
        "(A||B)",
        "A==B",
        "A<B",
        "A > B",
        "A10 < 20",
        "A1B != 2B\n Not Quite Right",
        "Lots\n of \r\nnewlines\n\n",
        "C < D",
        "C >= D",
        "(A<B) || (C<D)",
        "(A >= B) && (C > D)",
        "-20 < D",
        "-0x10 < D",
        "0x10 < D",
        "Invalid Expression",
        " // line comment",
        "A // line comment",
        "B // line comment 2 \n // next line",
        "C // line comment 3 \r\n//next line",
        "A /* block comment */",
        "A /* comment */ || /* more \ncomment */ B",
        "A::B",
        "A:B",
        "A::",
        "::B",
        "A /* unclosed block comment",
      };

      std::cout << "Value table:" << std::endl;
      for (auto& entry : values)
      {
        std::cout << "  " << entry.first << " --> " << entry.second << std::endl;
      }
      std::cout << std::endl;

      Tokenizer tokenizer(logger);
      Evaluator evaluator(logger);
      for (const std::string& condition : conditions)
      {
        tokenizer.Tokenize(condition);

        int val1 = 0, val2 = 0;
        ValueList valueList;
        Result res1 = evaluator.EvaluateCondition(condition, values, val1);
        Result res2 = evaluator.EvaluateCondition(tokenizer.GetResult(), values, val2, Evaluator::Mode::Strict, &valueList);
        std::cout << condition << std::endl;
        std::cout << "  tokens: ";
        for (const Token& t : tokenizer.GetResult())
        {
          std::cout << "'" << (t.m_type == Token::Type::NewLine ? "\\n" : t.m_token) << "'(" << static_cast<int>(t.m_type) << ")  ";
        }
        std::cout << std::endl
                  << "  results: ";
        if (res1.Succeeded())
          std::cout << val1 << "  ";
        else
          std::cout << "(failure)  ";
        if (res2.Succeeded())
          std::cout << val2 << std::endl;
        else
          std::cout << "(failure)" << std::endl;

        auto printValues = [](const ValueList& valueList)
        {
          for (const auto& var : valueList)
          {
            std::cout << var << "  ";
          }
          std::cout << std::endl;
        };
        std::cout << "  used values: ";
        printValues(valueList);

        if (res2.Failed())
        {
          res2 = evaluator.EvaluateCondition(tokenizer.GetResult(), values, val2, Evaluator::Mode::Lenient, &valueList);
          if (res2.Succeeded())
            std::cout << "  reparse success - values: ";
          else
            std::cout << "  reparse failed - values: ";
          printValues(valueList);
        }
        std::cout << std::endl;
      }
    }

  } // namespace Tools
} // namespace Hydra
