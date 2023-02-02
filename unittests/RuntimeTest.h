#pragma once

#include "thirdparty/munit.h"

namespace RuntimeTests
{
  MunitResult BitSetTest(const MunitParameter params[], void* fixture);
  MunitResult PermutationTest(const MunitParameter params[], void* fixture);
  MunitResult PerformanceTest(const MunitParameter params[], void* fixture);

  static MunitTest tests[] = {
    {.name = "/BitSet", .test = &BitSetTest},
    {.name = "/Permutation", .test = &PermutationTest},
    {.name = "/Performance", .test = &PerformanceTest},
    {.test = nullptr},
  };

  static const MunitSuite suite = {
    .prefix = "/Runtime",
    .tests = tests,
    .iterations = 1,
  };
} // namespace RuntimeTests
