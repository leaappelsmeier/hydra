#include <HydraRuntime/PermutationManager.h>
#include <HydraRuntime/PermutationSets.h>

#ifdef _MSC_VER
#  include <Windows.h>
#endif

namespace Hydra::Runtime
{
  template <typename Functor>
  void ForEachVariable(const PermutationManager& manager, const BitSet& mask, Functor func)
  {
    uint32_t bitBaseIndex = mask.GetBlockStartOffset() * BitSet::BITS_PER_BLOCK;

    const uint32_t numBlocks = mask.GetBlockCount();
    for (uint32_t blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
    {
      BitSet::BlockType block = mask.GetDataPtr()[blockIndex];
      while (block > 0)
      {
        const uint32_t i = firstBitLow(block);

        const uint32_t bitIndex = bitBaseIndex + i;
        auto variable = manager.GetVariable(bitIndex);

        func(*variable);

        const BitSet::BlockType mask = ((1ull << variable->m_numBits) - 1) << i;
        block &= ~mask;
      }

      bitBaseIndex += BitSet::BITS_PER_BLOCK;
    }
  }

  //////////////////////////////////////////////////////////////////////////

  PermutationVariableSet::PermutationVariableSet() = default;
  PermutationVariableSet::~PermutationVariableSet() = default;

  bool PermutationVariableSet::operator==(const PermutationVariableSet& other) const
  {
    return m_manager == other.m_manager && m_mask == other.m_mask;
  }

  bool PermutationVariableSet::operator!=(const PermutationVariableSet& other) const
  {
    return !(*this == other);
  }

  void PermutationVariableSet::AddVariable(const PermutationVariableEntry& variable)
  {
    assert(m_manager == nullptr || m_manager == &variable.m_manager);
    m_manager = &variable.m_manager;
    m_mask.SetBitOnes(variable.m_startBitIndex, variable.m_numBits);
  }

  void PermutationVariableSet::Iterate(IterateCallback callback) const
  {
    if (m_manager != nullptr)
    {
      ForEachVariable(*m_manager, m_mask, callback);
    }
  }

  void PermutationVariableSet::DumpToDebugOut() const
  {
    Iterate([](const PermutationVariableEntry& variable)
      {
#ifdef _MSC_VER
        std::string line = variable.m_name + "\n";
        OutputDebugString(line.c_str());
#endif
      });
  }

  void PermutationVariableSet::DumpToLog(ILoggingInterface* logger) const
  {
    Iterate([&](const PermutationVariableEntry& variable)
      { Log::Info(logger, "%s", variable.m_name.c_str()); });
  }

  void PermutationVariableSet::Clear()
  {
    m_manager = nullptr;
    m_mask.Clear();
  }

  //////////////////////////////////////////////////////////////////////////

  PermutationVariableState::PermutationVariableState() = default;
  PermutationVariableState::~PermutationVariableState() = default;

  bool PermutationVariableState::operator==(const PermutationVariableState& other) const
  {
    return m_manager == other.m_manager &&
           m_values == other.m_values &&
           m_valuesMask == other.m_valuesMask;
  }

  bool PermutationVariableState::operator!=(const PermutationVariableState& other) const
  {
    return !(*this == other);
  }

  Result PermutationVariableState::SetVariable(const PermutationVariableEntry& variable, bool value)
  {
    if (variable.m_type != PermutationVariableEntry::Type::Bool)
      return HYDRA_FAILURE;

    SetVariableInternal(variable, value ? 1 : 0);
    return HYDRA_SUCCESS;
  }

  Result PermutationVariableState::SetVariable(const PermutationVariableEntry& variable, int value)
  {
    if (variable.m_type != PermutationVariableEntry::Type::Int && variable.m_type != PermutationVariableEntry::Type::Enum)
      return HYDRA_FAILURE;

    uint32_t encodedValue = 0;
    if (variable.GetEncodedValue(value, encodedValue).Failed())
      return HYDRA_FAILURE;

    SetVariableInternal(variable, encodedValue);
    return HYDRA_SUCCESS;
  }

  Result PermutationVariableState::SetVariable(const PermutationVariableEntry& variable, const char* value)
  {
    uint32_t encodedValue = 0;
    if (variable.GetEncodedValue(value, encodedValue).Failed())
      return HYDRA_FAILURE;

    SetVariableInternal(variable, encodedValue);
    return HYDRA_SUCCESS;
  }

  void PermutationVariableState::Iterate(IterateValuesCallback callback) const
  {
    if (m_manager != nullptr)
    {
      ForEachVariable(*m_manager, m_valuesMask,
        [&](const PermutationVariableEntry& var)
        {
          auto encodedValue = m_values.GetBitValues(var.m_startBitIndex, var.m_numBits);
          callback(var, var.GetValueInt(encodedValue), var.GetValueString(encodedValue));
        });
    }
  }

  void PermutationVariableState::DumpToDebugOut() const
  {
    Iterate([](const PermutationVariableEntry& variable, int valueInt, const char* valueString)
      {
#ifdef _MSC_VER
        std::string line = variable.m_name + "=" + valueString + "\n";
        OutputDebugString(line.c_str());
#endif
      });
  }

  void PermutationVariableState::DumpToLog(ILoggingInterface* logger) const
  {
    Iterate([&](const PermutationVariableEntry& variable, int valueInt, const char* valueString)
      { Log::Info(logger, "%s=%s", variable.m_name.c_str(), valueString); });
  }

  void PermutationVariableState::Clear()
  {
    m_manager = nullptr;
    m_values.Clear();
    m_valuesMask.Clear();
  }

  Result PermutationVariableState::MergeBontoA(const PermutationVariableState& stateA, const PermutationVariableState& stateB, const PermutationVariableSet& usedVarsSet, PermutationVariableState& out_resultState)
  {
    if (MergeInternal(stateA, stateB, usedVarsSet, out_resultState.m_values, out_resultState.m_valuesMask).Failed())
      return HYDRA_FAILURE;

    out_resultState.m_manager = usedVarsSet.m_manager;
    return HYDRA_SUCCESS;
  }

  void PermutationVariableState::SetVariableInternal(const PermutationVariableEntry& variable, uint32_t encodedValue)
  {
    assert(m_manager == nullptr || m_manager == &variable.m_manager);
    m_manager = &variable.m_manager;

    m_values.SetBitValues(variable.m_startBitIndex, variable.m_numBits, encodedValue);
    m_valuesMask.SetBitOnes(variable.m_startBitIndex, variable.m_numBits);
  }

  Result PermutationVariableState::MergeInternal(const PermutationVariableState& stateA, const PermutationVariableState& stateB, const PermutationVariableSet& usedVarsSet, BitSet& out_values, BitSet& out_valuesMask, MissingValuesCallback missingValuesCallback /*= nullptr*/)
  {
    assert((stateA.m_manager == stateB.m_manager && stateA.m_manager == usedVarsSet.m_manager) || (stateA.m_manager == nullptr) || (stateB.m_manager == nullptr));

    uint32_t blockIndex = usedVarsSet.m_mask.GetBlockStartOffset();
    const uint32_t maskBlockCount = usedVarsSet.m_mask.GetBlockCount();
    const uint32_t maskBlockEnd = usedVarsSet.m_mask.GetBlockEndOffset();

    out_values.Clear();
    out_values.Reserve(blockIndex, maskBlockCount);

    out_valuesMask.Clear();
    out_valuesMask.Reserve(blockIndex, maskBlockCount);

    const BitSet::BlockType* maskBlock = usedVarsSet.m_mask.GetDataPtr();
    BitSet::BlockType* resultValuesBlock = out_values.GetDataPtr();
    BitSet::BlockType* resultMaskBlock = out_valuesMask.GetDataPtr();

    for (; blockIndex < maskBlockEnd; ++blockIndex)
    {
      const BitSet::BlockType valuesA = stateA.m_values.GetBlockOrEmpty(blockIndex);
      const BitSet::BlockType valuesB = stateB.m_values.GetBlockOrEmpty(blockIndex);

      const BitSet::BlockType maskA = stateA.m_valuesMask.GetBlockOrEmpty(blockIndex);
      const BitSet::BlockType maskB = stateB.m_valuesMask.GetBlockOrEmpty(blockIndex);

      *resultValuesBlock = (valuesB | (valuesA & ~maskB)) & *maskBlock;
      *resultMaskBlock = (maskA | maskB) & *maskBlock;

      if (missingValuesCallback && *resultMaskBlock != *maskBlock)
      {
        const uint32_t baseBitIndex = blockIndex * BitSet::BITS_PER_BLOCK;
        const BitSet::BlockType missingBits = ~(*resultMaskBlock) & *maskBlock;
        missingValuesCallback(baseBitIndex, missingBits);

        return HYDRA_FAILURE;
      }

      ++maskBlock;
      ++resultValuesBlock;
      ++resultMaskBlock;
    }

    return HYDRA_SUCCESS;
  }

  //////////////////////////////////////////////////////////////////////////

  PermutationVariableSelection::PermutationVariableSelection() = default;
  PermutationVariableSelection::~PermutationVariableSelection() = default;

  bool PermutationVariableSelection::operator==(const PermutationVariableSelection& other) const
  {
    return m_manager == other.m_manager &&
           m_values == other.m_values &&
           m_valuesMask == other.m_valuesMask;
  }

  bool PermutationVariableSelection::operator!=(const PermutationVariableSelection& other) const
  {
    return !(*this == other);
  }

  void PermutationVariableSelection::Iterate(IterateValuesCallback callback) const
  {
    if (m_manager != nullptr)
    {
      ForEachVariable(*m_manager, m_valuesMask,
        [&](const PermutationVariableEntry& var)
        {
          auto encodedValue = m_values.GetBitValues(var.m_startBitIndex, var.m_numBits);
          callback(var, var.GetValueInt(encodedValue), var.GetValueString(encodedValue));
        });
    }
  }

  void PermutationVariableSelection::DumpToDebugOut() const
  {
    Iterate([](const PermutationVariableEntry& variable, int valueInt, const char* valueString)
      {
#ifdef _MSC_VER
        std::string line = variable.m_name + "=" + valueString + "\n";
        OutputDebugString(line.c_str());
#endif
      });
  }

  void PermutationVariableSelection::DumpToLog(ILoggingInterface* logger) const
  {
    Iterate([&](const PermutationVariableEntry& variable, int valueInt, const char* valueString)
      { Log::Info(logger, "%s=%s", variable.m_name.c_str(), valueString); });
  }

  void PermutationVariableSelection::Clear()
  {
    m_manager = nullptr;
    m_values.Clear();
    m_valuesMask.Clear();
    m_hash = 0;
  }

  void PermutationVariableSelection::CalculateHash()
  {
    m_hash = m_values.Hash();
  }

} // namespace Hydra::Runtime
