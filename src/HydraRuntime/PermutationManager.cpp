#include <HydraRuntime/PermutationManager.h>

#include <string>

namespace Hydra::Runtime
{
  Result PermutationVariableEntry::GetEncodedValue(int value, uint32_t& out_encodedValue) const
  {
    if (m_type == Type::Bool)
    {
      if (value == 0 || value == 1)
      {
        out_encodedValue = value;
        return HYDRA_SUCCESS;
      }

      return HYDRA_FAILURE;
    }

    for (uint32_t i = 0; i < m_allowedValues.size(); ++i)
    {
      if (m_allowedValues[i].second == value)
      {
        out_encodedValue = i;
        return HYDRA_SUCCESS;
      }
    }

    return HYDRA_FAILURE;
  }

  static std::string s_TrueValue = "TRUE";
  static std::string s_FalseValue = "FALSE";

  Result PermutationVariableEntry::GetEncodedValue(const char* value, uint32_t& out_encodedValue) const
  {
    if (m_type == Type::Bool)
    {
      if (s_TrueValue == value)
      {
        out_encodedValue = 1;
        return HYDRA_SUCCESS;
      }
      else if (s_FalseValue == value)
      {
        out_encodedValue = 0;
        return HYDRA_SUCCESS;
      }

      return HYDRA_FAILURE;
    }

    for (uint32_t i = 0; i < m_allowedValues.size(); ++i)
    {
      if (m_allowedValues[i].first == value)
      {
        out_encodedValue = i;
        return HYDRA_SUCCESS;
      }
    }

    return HYDRA_FAILURE;
  }

  const char* ToString(PermutationVariableEntry::Type type)
  {
    switch (type)
    {
      case PermutationVariableEntry::Type::Unknown:
        return "Unknown";
      case PermutationVariableEntry::Type::Bool:
        return "Bool";
      case PermutationVariableEntry::Type::Int:
        return "Int";
      case PermutationVariableEntry::Type::Enum:
        return "Enum";
      default:
        assert(false);
        return "N/A";
    }
  }

  //////////////////////////////////////////////////////////////////////////

  PermutationManager::PermutationManager(ILoggingInterface* logger /*= nullptr*/)
    : m_logger(logger)
  {
  }

  const PermutationVariableEntry* PermutationManager::RegisterVariable(const char* name, std::optional<bool> defaultValue /*= std::nullopt*/)
  {
    std::optional<int> intDefaultValue;
    if (defaultValue.has_value())
    {
      intDefaultValue = *defaultValue ? 1 : 0;
    }
    return RegisterVariableInternal(name, std::span<std::pair<std::string, int>>(), intDefaultValue, PermutationVariableEntry::Type::Bool);
  }

  const PermutationVariableEntry* PermutationManager::RegisterVariable(const char* name, std::span<int> allowedValues, std::optional<int> defaultValue /*= std::nullopt*/)
  {
    std::vector<std::pair<std::string, int>> namedAllowedValues;
    for (int allowedValue : allowedValues)
    {
      namedAllowedValues.push_back({std::to_string(allowedValue), allowedValue});
    }

    return RegisterVariableInternal(name, namedAllowedValues, defaultValue, PermutationVariableEntry::Type::Int);
  }

  const PermutationVariableEntry* PermutationManager::RegisterVariable(const char* name, std::span<std::pair<std::string, int>> allowedValues, std::optional<int> defaultValue /*= std::nullopt*/)
  {
    return RegisterVariableInternal(name, allowedValues, defaultValue, PermutationVariableEntry::Type::Enum);
  }

  const PermutationVariableEntry* PermutationManager::GetVariable(const char* name) const
  {
    auto varIt = m_variableNameToVariable.find(name);
    if (varIt != m_variableNameToVariable.end())
    {
      return varIt->second;
    }

    return nullptr;
  }

  const PermutationVariableEntry* PermutationManager::GetVariable(uint32_t bitIndex) const
  {
    if (bitIndex < m_bitIndexToVariable.size())
    {
      return m_bitIndexToVariable[bitIndex];
    }

    return nullptr;
  }

  Result PermutationManager::FinalizeState(const PermutationVariableState& state, const PermutationVariableSet& usedVariablesSet, PermutationVariableSelection& out_selection) const
  {
    out_selection.Clear();

    auto missingValuesCallback = [&](uint32_t baseBitIndex, BitSet::BlockType missingBits)
    {
      while (missingBits > 0)
      {
        const uint32_t i = firstBitLow(missingBits);

        const uint32_t bitIndex = baseBitIndex + i;
        auto variable = GetVariable(bitIndex);

        Log::Error(m_logger, "Permutation variable '%s' is not set in state and has no default value", variable->m_name.c_str());

        const BitSet::BlockType mask = ((1ull << variable->m_numBits) - 1) << i;
        missingBits &= ~mask;
      }
    };

    if (PermutationVariableState::MergeInternal(m_defaultState, state, usedVariablesSet, out_selection.m_values, out_selection.m_valuesMask, missingValuesCallback).Failed())
      return HYDRA_FAILURE;

    out_selection.m_manager = this;
    out_selection.CalculateHash();
    return HYDRA_SUCCESS;
  }

  const PermutationVariableEntry* PermutationManager::RegisterVariableInternal(const char* name, std::span<std::pair<std::string, int>> allowedValues, std::optional<int> defaultValue, PermutationVariableEntry::Type type)
  {
    if (type != PermutationVariableEntry::Type::Bool && allowedValues.empty())
    {
      Log::Error(m_logger, "A set of allowed values must be specified for int vars");
      return nullptr;
    }

    if (auto existingEntry = GetVariable(name))
    {
      if (existingEntry->m_type != type)
      {
        Log::Error(m_logger, "Variable '%s' of type '%s' already exists as '%s'", name, ToString(type), ToString(existingEntry->m_type));
        return nullptr;
      }

      if (std::equal(existingEntry->m_allowedValues.begin(), existingEntry->m_allowedValues.end(), allowedValues.begin(), allowedValues.end()) == false)
      {
        Log::Error(m_logger, "Variable '%s' already exists with different allowed values", name);
        return nullptr;
      }

      if (defaultValue.has_value() && existingEntry->m_defaultValue != *defaultValue)
      {
        Log::Error(m_logger, "Variable '%s' already exists with different default value %d. Given default value is %d", name, existingEntry->m_defaultValue, *defaultValue);
        return nullptr;
      }

      return existingEntry;
    }

    const uint32_t numBits = (type != PermutationVariableEntry::Type::Bool) ? ceillog2(allowedValues.size()) : 1;
    const uint32_t bitIndex = GetFreeBitIndex(numBits);

    PermutationVariableEntry newVariableEntry(*this);
    newVariableEntry.m_name = name;
    newVariableEntry.m_startBitIndex = bitIndex;
    newVariableEntry.m_numBits = numBits;
    newVariableEntry.m_type = type;

    if (allowedValues.size() > 0)
    {
      newVariableEntry.m_allowedValues.assign(allowedValues.begin(), allowedValues.end());
    }

    if (defaultValue.has_value())
    {
      newVariableEntry.m_hasDefaultValue = true;
      newVariableEntry.m_defaultValue = *defaultValue;

      uint32_t encodedValue = 0;
      if (newVariableEntry.GetEncodedValue(*defaultValue, encodedValue).Failed())
      {
        Log::Error(m_logger, "%d is not a valid default value for permutation variable '%s'", *defaultValue, name);
        return nullptr;
      }

      m_defaultState.SetVariableInternal(newVariableEntry, encodedValue);
    }

    m_variableStorage.push_back(std::move(newVariableEntry));
    auto& variableEntry = m_variableStorage.back();

    m_variableNameToVariable.insert({name, &variableEntry});

    if (m_bitIndexToVariable.size() <= bitIndex)
    {
      m_bitIndexToVariable.resize(bitIndex + 1);
    }
    m_bitIndexToVariable[bitIndex] = &variableEntry;

    return &variableEntry;
  }

  uint32_t PermutationManager::GetFreeBitIndex(uint32_t numBitsNeeded /*= 1*/)
  {
    uint32_t bitIndex = uint32_t(-1);

    for (auto it = m_blockAllocations.begin(); it != m_blockAllocations.end(); ++it)
    {
      auto& blockAllocation = *it;

      if (blockAllocation.m_remainingBits >= numBitsNeeded)
      {
        bitIndex = (blockAllocation.m_blockIndex + 1) * BitSet::BITS_PER_BLOCK - blockAllocation.m_remainingBits;
        blockAllocation.m_remainingBits -= numBitsNeeded;

        if (blockAllocation.m_remainingBits == 0)
        {
          m_blockAllocations.erase(it);
        }

        break;
      }
    }

    if (bitIndex == uint32_t(-1))
    {
      bitIndex = m_nextBlockIndex * BitSet::BITS_PER_BLOCK;

      m_blockAllocations.push_back({BitSet::BITS_PER_BLOCK - numBitsNeeded, m_nextBlockIndex});
      ++m_nextBlockIndex;
    }

    std::sort(m_blockAllocations.begin(), m_blockAllocations.end());

    return bitIndex;
  }

} // namespace Hydra::Runtime
