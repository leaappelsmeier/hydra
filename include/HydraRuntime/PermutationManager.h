#pragma once

#include <HydraRuntime/Logger.h>
#include <HydraRuntime/PermutationSets.h>

#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Hydra
{
  namespace Runtime
  {
    class PermutationManager;

    struct PermutationVariableEntry
    {
      enum class Type : uint8_t
      {
        Unknown,
        Bool,
        Int,
        Enum
      };

      std::string m_name;

      uint32_t m_startBitIndex = 0;
      uint16_t m_numBits = 0;

      Type m_type = Type::Unknown;
      bool m_hasDefaultValue = false;
      int m_defaultValue = 0;

      std::vector<std::pair<std::string, int>> m_allowedValues;
      const PermutationManager& m_manager;

      PermutationVariableEntry(const PermutationManager& manager)
        : m_manager(manager)
      {
      }

      Result GetEncodedValue(int value, uint32_t& out_encodedValue) const;
      Result GetEncodedValue(const char* value, uint32_t& out_encodedValue) const;

      const char* GetValueString(uint32_t encodedValue) const
      {
        return (m_type == Type::Bool) ? (encodedValue != 0 ? "TRUE" : "FALSE") : m_allowedValues[encodedValue].first.c_str();
      }

      int GetValueInt(uint32_t encodedValue) const
      {
        return (m_type == Type::Bool) ? encodedValue : m_allowedValues[encodedValue].second;
      }
    };

    class PermutationManager
    {
    public:
      PermutationManager(ILoggingInterface* logger = nullptr);

      const PermutationVariableEntry* RegisterVariable(const char* name, std::optional<bool> defaultValue = std::nullopt);
      const PermutationVariableEntry* RegisterVariable(const char* name, std::span<int> allowedValues, std::optional<int> defaultValue = std::nullopt);
      const PermutationVariableEntry* RegisterVariable(const char* name, std::span<std::pair<std::string, int>> allowedValues, std::optional<int> defaultValue = std::nullopt);

      const PermutationVariableEntry* GetVariable(const char* name) const;
      const PermutationVariableEntry* GetVariable(uint32_t bitIndex) const;

      Result FinalizeState(const PermutationVariableState& state, const PermutationVariableSet& usedVariablesSet, PermutationVariableSelection& out_selection) const;

    private:
      const PermutationVariableEntry* RegisterVariableInternal(const char* name, std::span<std::pair<std::string, int>> allowedValues, std::optional<int> defaultValue, PermutationVariableEntry::Type type);
      uint32_t GetFreeBitIndex(uint32_t numBitsNeeded = 1);

      std::deque<PermutationVariableEntry> m_variableStorage;
      std::map<std::string, PermutationVariableEntry*> m_variableNameToVariable;
      std::vector<PermutationVariableEntry*> m_bitIndexToVariable;

      struct BlockAllocation
      {
        uint32_t m_remainingBits = 0;
        uint32_t m_blockIndex = 0;

        bool operator<(const BlockAllocation& other) const
        {
          if (m_remainingBits != other.m_remainingBits)
            return m_remainingBits < other.m_remainingBits;

          return m_blockIndex < other.m_blockIndex;
        }
      };
      std::vector<BlockAllocation> m_blockAllocations;
      uint32_t m_nextBlockIndex = 0;

      PermutationVariableState m_defaultState;

      ILoggingInterface* m_logger = nullptr;
    };
  } // namespace Runtime
} // namespace Hydra
