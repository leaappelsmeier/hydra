#pragma once

#include "thirdparty/munit.h"

namespace ToolsTests
{
  MunitResult TokenizerTest(const MunitParameter params[], void* fixture);
  MunitResult EvaluatorTest(const MunitParameter params[], void* fixture);

  static MunitTest tests[] = {
    {.name = "/Tokenizer", .test = &TokenizerTest},
    {.name = "/Evaluator", .test = &EvaluatorTest},
    {.test = nullptr},
  };

  static const MunitSuite suite = {
    .prefix = "/Tools",
    .tests = tests,
    .iterations = 1,
  };
} // namespace ToolsTests
