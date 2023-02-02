#include "RuntimeTest.h"

#include <HydraRuntime/PermutationManager.h>

#include <string>
#include <type_traits>

namespace
{
  static uint64_t s_numAllocs = 0;

  void* TestAlloc(size_t numBytes)
  {
    ++s_numAllocs;
    return ::malloc(numBytes);
  }

  void TestDealloc(void* ptr)
  {
    --s_numAllocs;
    ::free(ptr);
  }

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

  struct ExpectedVar
  {
    std::string name;
    std::string valueString;
    int value;
  };

  template <typename T>
  void CheckExpectedVars(const T& permutationVars, std::span<ExpectedVar> expectedVars)
  {
    uint32_t varIndex = 0;
    permutationVars.Iterate([&](const Hydra::Runtime::PermutationVariableEntry& variable, int valueInt, const char* valueString)
      {
        munit_assert_uint32(varIndex, <, expectedVars.size());

        const ExpectedVar& expectedVar = expectedVars[varIndex];
        munit_assert_string_equal(variable.m_name.c_str(), expectedVar.name.c_str());
        munit_assert_string_equal(valueString, expectedVar.valueString.c_str());
        munit_assert_int(valueInt, ==, expectedVar.value);

        varIndex++; });

    munit_assert_uint32(varIndex, ==, expectedVars.size());
  }

  void CheckExpectedVars(const Hydra::Runtime::PermutationVariableSet& permutationVars, std::span<ExpectedVar> expectedVars)
  {
    uint32_t varIndex = 0;
    permutationVars.Iterate([&](const Hydra::Runtime::PermutationVariableEntry& variable)
      {
        munit_assert_uint32(varIndex, <, expectedVars.size());

        const ExpectedVar& expectedVar = expectedVars[varIndex];
        munit_assert_string_equal(variable.m_name.c_str(), expectedVar.name.c_str());

        varIndex++; });

    munit_assert_uint32(varIndex, ==, expectedVars.size());
  }

} // namespace

MunitResult RuntimeTests::BitSetTest(const MunitParameter params[], void* fixture)
{
  Hydra::Runtime::Core::SetCustomFunctions(&TestAlloc, &TestDealloc, nullptr);

  constexpr uint32_t totalNumBits = 1000;
  std::vector<bool> bitValues;
  bitValues.resize(totalNumBits);

  for (uint32_t i = 0; i < totalNumBits; ++i)
  {
    bitValues[i] = (i % 3) != 0;
  }

  // Setting bits, clear, reserve
  if (true)
  {
    Hydra::Runtime::BitSet s;
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      s.SetBitValue(i, bitValues[i]);
    }

    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      auto block = s.GetBitValues(i);
      munit_assert_true(block == 0 || block == 1);
      munit_assert_true((block != 0) == bitValues[i]);

      bool value = s.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i]);
    }

    uint32_t oldBlockCount = s.GetBlockCount();
    s.Clear();
    munit_assert_uint32(s.GetBlockStartOffset(), ==, 0u);
    munit_assert_uint32(s.GetBlockCount(), ==, 0u);
    for (uint32_t i = 0; i < oldBlockCount; i++)
    {
      auto block = s.GetDataPtr()[i];
      munit_assert_uint64(block, ==, 0);
    }

    s.Reserve(17, 2);
    munit_assert_uint32(s.GetBlockStartOffset(), ==, 17u);
    munit_assert_uint32(s.GetBlockCount(), ==, 2u);
  }
  munit_assert_uint32(s_numAllocs, ==, 0);

  // Setting bits in reverse
  if (true)
  {
    Hydra::Runtime::BitSet s;
    for (uint32_t i = totalNumBits; i-- > 0;)
    {
      s.SetBitValue(i, bitValues[i]);
    }

    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      bool value = s.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i]);
    }
  }
  munit_assert_uint32(s_numAllocs, ==, 0);

  // Copy
  if (true)
  {
    Hydra::Runtime::BitSet a;
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      a.SetBitValue(i, bitValues[i]);
    }

    Hydra::Runtime::BitSet b = a;
    munit_assert_uint32(b.GetBlockCount(), ==, a.GetBlockCount());
    munit_assert_true(b == a);
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      bool value = b.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i]);
    }

    Hydra::Runtime::BitSet c;
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      c.SetBitValue(i + totalNumBits, bitValues[i]);
      c.SetBitValue(i + totalNumBits * 2, bitValues[i]);
    }

    c = a;
    munit_assert_uint32(c.GetBlockCount(), ==, a.GetBlockCount());
    munit_assert_uint32(c.GetBlockStartOffset(), ==, a.GetBlockStartOffset());
    munit_assert_true(c == a);
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      bool value = b.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i]);
    }
  }
  munit_assert_uint32(s_numAllocs, ==, 0);

  // Move
  if (true)
  {
    Hydra::Runtime::BitSet a;
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      a.SetBitValue(i, bitValues[i]);
    }

    Hydra::Runtime::BitSet b = std::move(a);
    munit_assert_uint32(a.GetBlockCount(), ==, 0);
    munit_assert_uint32(b.GetBlockCount(), ==, 16);
    for (uint32_t i = 0; i < totalNumBits; ++i)
    {
      bool value = b.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i]);
    }

    Hydra::Runtime::BitSet c;
    for (uint32_t i = 0; i < 16; ++i)
    {
      c.SetBitValue(i, bitValues[i + Hydra::Runtime::BitSet::BITS_PER_BLOCK]);
    }

    Hydra::Runtime::BitSet d;
    d = std::move(c);
    for (uint32_t i = 0; i < 16; ++i)
    {
      bool value = d.GetBitValue(i);
      munit_assert_uint8(value, ==, bitValues[i + Hydra::Runtime::BitSet::BITS_PER_BLOCK]);
    }
  }

  Hydra::Runtime::Core::SetDefaultFunctions();
  return MUNIT_OK;
}

MunitResult RuntimeTests::PermutationTest(const MunitParameter params[], void* fixture)
{
  std::vector<int> intValues = {0, 2, 4, 8};

  std::vector<std::pair<std::string, int>> enumValues;
  enumValues.push_back({"VAL0", 0});
  enumValues.push_back({"VAL1", 1});
  enumValues.push_back({"VAL2", 2});
  enumValues.push_back({"VAL3", 3});
  enumValues.push_back({"VAL4", 4});

  TestLoggingImpl logger;

  if (true)
  {
    Hydra::Runtime::PermutationManager permManager(&logger);

    // Register bool vars
    auto boolAVar = permManager.RegisterVariable("BOOL_A");
    auto boolBVar = permManager.RegisterVariable("BOOL_B", false);
    auto boolCVar = permManager.RegisterVariable("BOOL_C", true);
    munit_assert_ptr_not_null(boolAVar);
    munit_assert_ptr_not_null(boolBVar);
    munit_assert_ptr_not_null(boolCVar);
    munit_assert_true(boolAVar->m_type == Hydra::Runtime::PermutationVariableEntry::Type::Bool);

    // Test no allowed values
    ResetLoggingStats();
    auto intVar = permManager.RegisterVariable("INT", std::span<int>());
    munit_assert_ptr_null(intVar);
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);

    // Test invalid default value
    ResetLoggingStats();
    intVar = permManager.RegisterVariable("INT", intValues, 7);
    munit_assert_ptr_null(intVar);
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);

    // Register valid int var
    intVar = permManager.RegisterVariable("INT", intValues, 4);
    munit_assert_ptr_not_null(intVar);
    munit_assert_true(intVar->m_type == Hydra::Runtime::PermutationVariableEntry::Type::Int);

    // Register enum var
    auto enumVar = permManager.RegisterVariable("ENUM", enumValues);
    munit_assert_ptr_not_null(enumVar);
    munit_assert_true(enumVar->m_type == Hydra::Runtime::PermutationVariableEntry::Type::Enum);

    // Re-register with wrong type
    ResetLoggingStats();
    auto nullVar = permManager.RegisterVariable("BOOL_A", intValues, 4);
    munit_assert_ptr_null(nullVar);
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);

    // Re-register with different allowed values
    ResetLoggingStats();
    std::vector<int> intValues2 = {0, 1, 2, 3};
    nullVar = permManager.RegisterVariable("INT", intValues2);
    munit_assert_ptr_null(nullVar);
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);

    // Re-register with different default value
    ResetLoggingStats();
    nullVar = permManager.RegisterVariable("INT", intValues, 8);
    munit_assert_ptr_null(nullVar);
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);
  }

  if (true)
  {
    Hydra::Runtime::PermutationManager permManager(&logger);

    auto boolAVar = permManager.RegisterVariable("BOOL_A");
    auto boolBVar = permManager.RegisterVariable("BOOL_B", false);
    auto boolCVar = permManager.RegisterVariable("BOOL_C", true);

    auto intVar = permManager.RegisterVariable("INT", intValues, 4);
    auto enumVar = permManager.RegisterVariable("ENUM", enumValues);

    Hydra::Runtime::PermutationVariableState vars;
    munit_assert_true(vars.SetVariable(*boolBVar, true).Succeeded());
    munit_assert_true(vars.SetVariable(*intVar, 1).Failed());
    munit_assert_true(vars.SetVariable(*intVar, 8).Succeeded());
    munit_assert_true(vars.SetVariable(*enumVar, "BLUBB").Failed());
    munit_assert_true(vars.SetVariable(*enumVar, "VAL3").Succeeded());

    {
      ExpectedVar expectedVars[] = {
        {"BOOL_B", "TRUE", 1},
        {"INT", "8", 8},
        {"ENUM", "VAL3", 3},
      };

      CheckExpectedVars(vars, expectedVars);
    }

    Hydra::Runtime::PermutationVariableSet usedVarsSet;
    usedVarsSet.AddVariable(*boolAVar);
    usedVarsSet.AddVariable(*boolBVar);
    usedVarsSet.AddVariable(*boolCVar);
    usedVarsSet.AddVariable(*intVar);
    usedVarsSet.AddVariable(*enumVar);

    {
      ExpectedVar expectedVars[] = {
        {"BOOL_A"},
        {"BOOL_B"},
        {"BOOL_C"},
        {"INT"},
        {"ENUM"},
      };

      CheckExpectedVars(usedVarsSet, expectedVars);
    }

    // Try to finalize with missing var
    ResetLoggingStats();
    Hydra::Runtime::PermutationVariableSelection selection;
    munit_assert_true(permManager.FinalizeState(vars, usedVarsSet, selection).Failed());
    munit_assert_uint32(s_loggingStats.numErrors, ==, 1);

    // Add missing var
    munit_assert_true(vars.SetVariable(*boolAVar, false).Succeeded());
    munit_assert_true(permManager.FinalizeState(vars, usedVarsSet, selection).Succeeded());

    {
      ExpectedVar expectedVars[] = {
        {"BOOL_A", "FALSE", 0},
        {"BOOL_B", "TRUE", 1},
        {"BOOL_C", "TRUE", 1},
        {"INT", "8", 8},
        {"ENUM", "VAL3", 3},
      };

      CheckExpectedVars(selection, expectedVars);
    }
  }

  if (true)
  {
    Hydra::Runtime::PermutationManager permManager(&logger);

    auto boolAVar = permManager.RegisterVariable("BOOL_A");
    auto boolBVar = permManager.RegisterVariable("BOOL_B", false);
    auto boolCVar = permManager.RegisterVariable("BOOL_C", true);

    auto intVar = permManager.RegisterVariable("INT", intValues, 4);
    auto enumVar = permManager.RegisterVariable("ENUM", enumValues);

    Hydra::Runtime::PermutationVariableState varsA;
    munit_assert_true(varsA.SetVariable(*boolAVar, true).Succeeded());
    munit_assert_true(varsA.SetVariable(*boolBVar, true).Succeeded());
    munit_assert_true(varsA.SetVariable(*enumVar, 2).Succeeded());

    Hydra::Runtime::PermutationVariableState varsB;
    munit_assert_true(varsA.SetVariable(*boolBVar, false).Succeeded());
    munit_assert_true(varsB.SetVariable(*boolCVar, true).Succeeded());
    munit_assert_true(varsB.SetVariable(*intVar, 4).Succeeded());
    munit_assert_true(varsB.SetVariable(*enumVar, 4).Succeeded());

    Hydra::Runtime::PermutationVariableSet usedVarsSet;
    usedVarsSet.AddVariable(*boolAVar);
    usedVarsSet.AddVariable(*boolBVar);
    usedVarsSet.AddVariable(*intVar);
    usedVarsSet.AddVariable(*enumVar);

    Hydra::Runtime::PermutationVariableState varsMerged;
    munit_assert_true(Hydra::Runtime::PermutationVariableState::MergeBontoA(varsA, varsB, usedVarsSet, varsMerged).Succeeded());

    {
      ExpectedVar expectedVars[] = {
        {"BOOL_A", "TRUE", 1},
        {"BOOL_B", "FALSE", 0},
        {"INT", "4", 4},
        {"ENUM", "VAL4", 4},
      };

      CheckExpectedVars(varsMerged, expectedVars);
    }
  }

  return MUNIT_OK;
}

MunitResult RuntimeTests::PerformanceTest(const MunitParameter params[], void* fixture)
{
  TestLoggingImpl logger;

  if (true)
  {
    Hydra::Runtime::PermutationManager permManager(&logger);

    constexpr uint32_t totalNumVars = 30000;
    std::vector<const Hydra::Runtime::PermutationVariableEntry*> varEntries;
    varEntries.reserve(totalNumVars);

    std::string name;
    for (uint32_t i = 0; i < totalNumVars; ++i)
    {
      name = "BOOL_" + std::to_string(i);
      varEntries.push_back(permManager.RegisterVariable(name.c_str()));
    }

    Hydra::Runtime::PermutationVariableState varsA;
    for (uint32_t i = 0; i < (totalNumVars / 3 * 2); ++i)
    {
      varsA.SetVariable(*varEntries[i], i % 7 == 3).IgnoreResult();
    }

    Hydra::Runtime::PermutationVariableState varsB;
    for (uint32_t i = (totalNumVars / 3); i < totalNumVars; ++i)
    {
      varsB.SetVariable(*varEntries[i], i % 7 == 5).IgnoreResult();
    }

    Hydra::Runtime::PermutationVariableSet usedVarsSet;
    for (uint32_t i = (totalNumVars / 6); i < (totalNumVars / 6 * 5); ++i)
    {
      usedVarsSet.AddVariable(*varEntries[i]);
    }

    Hydra::Runtime::PermutationVariableState varsMerged;
    munit_assert_true(Hydra::Runtime::PermutationVariableState::MergeBontoA(varsA, varsB, usedVarsSet, varsMerged).Succeeded());

    {
      std::string name;
      std::vector<ExpectedVar> expectedVars;
      for (uint32_t i = (totalNumVars / 6); i < (totalNumVars / 6 * 5); ++i)
      {
        name = "BOOL_" + std::to_string(i);
        bool boolValue = (i < (totalNumVars / 3)) ? (i % 7 == 3) : (i % 7 == 5);
        expectedVars.push_back({name, boolValue ? "TRUE" : "FALSE", boolValue ? 1 : 0});
      }

      CheckExpectedVars(varsMerged, expectedVars);
    }
  }

  return MUNIT_OK;
}
