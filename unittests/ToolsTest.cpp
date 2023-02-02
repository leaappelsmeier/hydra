#include "ToolsTest.h"

#include <HydraRuntime/Logger.h>
#include <HydraTools/Evaluator.h>
#include <HydraTools/Tokenizer.h>
#include <optional>
#include <span>


namespace
{
  struct LoggingStats
  {
    uint32_t numInfos = 0;
    uint32_t numWarnings = 0;
    uint32_t numErrors = 0;
  };

  static LoggingStats s_loggingStats;

  void ResetLoggingStats()
  {
    s_loggingStats = {};
  }

  struct TestLoggingImpl : public Hydra::Runtime::ILoggingInterface
  {
    TestLoggingImpl() { ResetLoggingStats(); }

    void LogInfo(const char* message) override
    {
      ++s_loggingStats.numInfos;
    }

    void LogWarning(const char* message) override
    {
      ++s_loggingStats.numWarnings;
    }

    void LogError(const char* message) override
    {
      ++s_loggingStats.numErrors;
    }
  };

} // namespace

MunitResult ToolsTests::TokenizerTest(const MunitParameter params[], void* fixture)
{
  TestLoggingImpl logger;
  Hydra::Tools::Tokenizer tokenizer(&logger);

  using Type = Hydra::Tools::Token::Type;

  auto ConfirmTokenTypes = [](const Hydra::Tools::Tokenizer& tokenizer, const std::vector<Type>& types)
  {
    const Hydra::Tools::TokenStream& tokens = tokenizer.GetResult();
    if (tokens.size() != types.size())
      return false;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
      if (tokens[i].m_type != types[i])
        return false;
    }
    return true;
  };

  // Test token types
  tokenizer.Tokenize("A");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier}));
  tokenizer.Tokenize(":");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::NonIdentifier}));
  tokenizer.Tokenize("1");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Integer}));
  tokenizer.Tokenize("0x10");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Integer}));
  tokenizer.Tokenize("0X10");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Integer}));
  tokenizer.Tokenize("\n");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::NewLine}));
  tokenizer.Tokenize("// line comment");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::LineComment}));
  tokenizer.Tokenize("/* block comment */");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::BlockComment}));

  // Test identifier concatenation and corner cases
  tokenizer.Tokenize("A::B");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier}));
  tokenizer.Tokenize("A::B::C");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier}));
  tokenizer.Tokenize("A:B");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::Identifier}));
  tokenizer.Tokenize("::B");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::NonIdentifier, Type::NonIdentifier, Type::Identifier}));
  tokenizer.Tokenize("A::");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::NonIdentifier}));

  // Whitespace removal
  tokenizer.Tokenize("A:B:C");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::Identifier, Type::NonIdentifier, Type::Identifier}));
  tokenizer.Tokenize(" A:B :C");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::Identifier, Type::NonIdentifier, Type::Identifier}));
  tokenizer.Tokenize("A :B:  C  ");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::Identifier, Type::NonIdentifier, Type::Identifier}));
  tokenizer.Tokenize("A: B: C");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::Identifier, Type::NonIdentifier, Type::Identifier, Type::NonIdentifier, Type::Identifier}));

  // No errors so far
  munit_assert_uint32(s_loggingStats.numErrors, ==, 0);

  // Failure case - open block comment
  ResetLoggingStats();
  tokenizer.Tokenize("/* open block comment");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::BlockComment}));
  munit_assert_uint32(s_loggingStats.numWarnings, ==, 1);

  // Line comment termination
  ResetLoggingStats();
  tokenizer.Tokenize("// Line comment \n // Another line comment\n// And another");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::LineComment, Type::NewLine, Type::LineComment, Type::NewLine, Type::LineComment}));
  munit_assert_uint32(s_loggingStats.numErrors, ==, 0);

  // Block comment enclosing line comments
  tokenizer.Tokenize("/* Block comment // Line comment \n // Another line comment\n// And another */");
  munit_assert_true(ConfirmTokenTypes(tokenizer, {Type::BlockComment}));
  munit_assert_uint32(s_loggingStats.numErrors, ==, 0);

  return MUNIT_OK;
}

MunitResult ToolsTests::EvaluatorTest(const MunitParameter params[], void* fixture)
{
  TestLoggingImpl logger;

  using namespace Hydra::Tools;
  Hydra::Tools::Evaluator evaluator(&logger);

  ValueTable values;
  values["A"] = 1;
  values["B"] = 2;
  values["C"] = -3;
  values["D"] = -4;
  values["value"] = 10;
  values["A1"] = 15;
  values["Foo::Bar"] = 42;

  auto EvaluateAndCheck = [&evaluator, &values](std::string_view expression, std::optional<int> expectedValue, bool lenientParse = false)
  {
    int value = 0;
    Hydra::Runtime::Result result = evaluator.EvaluateCondition(expression, values, value, lenientParse ? Evaluator::Mode::Lenient : Evaluator::Mode::Strict);
    if (result.Succeeded() && expectedValue.has_value())
    {
      return expectedValue.value() == value;
    }
    else if (result.Failed() && !expectedValue.has_value())
    {
      return true;
    }

    return false;
  };

  auto CompareValue = [](std::optional<int> lhs, int rhs)
  {
    if (lhs.has_value() && (lhs.value() == rhs))
      return true;
    else
      return false;
  };

  // Values / expressions
  munit_assert_true(EvaluateAndCheck("true", 1));
  munit_assert_true(EvaluateAndCheck("false", 0));
  munit_assert_true(EvaluateAndCheck("20", 20));
  munit_assert_true(EvaluateAndCheck("0x20", 0x20));
  munit_assert_true(EvaluateAndCheck("0X20", 0x20));
  munit_assert_true(EvaluateAndCheck("0x010", 0x10));
  munit_assert_true(EvaluateAndCheck("-2", -2));
  munit_assert_true(EvaluateAndCheck("-0x1", -0x1));
  munit_assert_true(EvaluateAndCheck("0xabcde", 0xabcde));
  munit_assert_true(EvaluateAndCheck("0x10 | 0x01", 0x11));
  munit_assert_true(EvaluateAndCheck("0x7 & 0x13", 0x7 & 0x13));
  munit_assert_true(EvaluateAndCheck("value", 10));
  munit_assert_true(EvaluateAndCheck("no_value", std::nullopt));
  munit_assert_true(EvaluateAndCheck("A||B", 1));
  munit_assert_true(EvaluateAndCheck("(A||B)", 1));
  munit_assert_true(EvaluateAndCheck("A == B", 0));
  munit_assert_true(EvaluateAndCheck("A < B", 1));
  munit_assert_true(EvaluateAndCheck("A>B", 0));
  munit_assert_true(EvaluateAndCheck("A1 < 20", 1));
  munit_assert_true(EvaluateAndCheck("A < B", 1));
  munit_assert_true(EvaluateAndCheck("A < B", 1));
  munit_assert_true(EvaluateAndCheck("C < D", 0));
  munit_assert_true(EvaluateAndCheck("C >= D", 1));
  munit_assert_true(EvaluateAndCheck("-20 < D", 1));
  munit_assert_true(EvaluateAndCheck("(A<B) || (C<D)", 1));
  munit_assert_true(EvaluateAndCheck("(A >= B) && (C > D)", 0));
  munit_assert_true(EvaluateAndCheck("Foo::Bar", 42));

  // TODO: Used value evaluation

  return MUNIT_OK;
}
