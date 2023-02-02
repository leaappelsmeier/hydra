#pragma once

#include <HydraRuntime/BitSet.h>
#include <HydraRuntime/Result.h>

#include <functional>

namespace Hydra
{
  namespace Runtime
  {
    struct ILoggingInterface;
    struct PermutationVariableEntry;
    class PermutationManager;

    using IterateCallback = std::function<void(const PermutationVariableEntry& variable)>;
    using IterateValuesCallback = std::function<void(const PermutationVariableEntry& variable, int valueInt, const char* valueString)>;

    class PermutationVariableSet
    {
    public:
      PermutationVariableSet();
      ~PermutationVariableSet();

      bool operator==(const PermutationVariableSet& other) const;
      bool operator!=(const PermutationVariableSet& other) const;

      void AddVariable(const PermutationVariableEntry& variable);

      void Iterate(IterateCallback callback) const;
      void DumpToDebugOut() const;
      void DumpToLog(ILoggingInterface* logger) const;

      void Clear();

    private:
      friend class PermutationManager;
      friend class PermutationVariableState;

      const PermutationManager* m_manager = nullptr;
      BitSet m_mask;
    };

    class PermutationVariableState
    {
    public:
      PermutationVariableState();
      ~PermutationVariableState();

      bool operator==(const PermutationVariableState& other) const;
      bool operator!=(const PermutationVariableState& other) const;

      Result SetVariable(const PermutationVariableEntry& variable, bool value);
      Result SetVariable(const PermutationVariableEntry& variable, int value);
      Result SetVariable(const PermutationVariableEntry& variable, const char* value);

      void Iterate(IterateValuesCallback callback) const;
      void DumpToDebugOut() const;
      void DumpToLog(ILoggingInterface* logger) const;

      void Clear();

      /// \brief Values in stateA are overwritten by values in stateB if they are set in both
      static Result MergeBontoA(const PermutationVariableState& stateA, const PermutationVariableState& stateB, const PermutationVariableSet& usedVarsSet, PermutationVariableState& out_resultState);

    private:
      friend class PermutationManager;

      void SetVariableInternal(const PermutationVariableEntry& variable, uint32_t encodedValue);

      using MissingValuesCallback = std::function<void(uint32_t baseBitIndex, BitSet::BlockType missingBits)>;

      static Result MergeInternal(const PermutationVariableState& stateA, const PermutationVariableState& stateB, const PermutationVariableSet& usedVarsSet, BitSet& out_values, BitSet& out_valuesMask, MissingValuesCallback missingValuesCallback = nullptr);

      const PermutationManager* m_manager = nullptr;
      BitSet m_values;
      BitSet m_valuesMask;
    };

    class PermutationVariableSelection
    {
    public:
      PermutationVariableSelection();
      ~PermutationVariableSelection();

      bool operator==(const PermutationVariableSelection& other) const;
      bool operator!=(const PermutationVariableSelection& other) const;

      void Iterate(IterateValuesCallback callback) const;
      void DumpToDebugOut() const;
      void DumpToLog(ILoggingInterface* logger) const;

      void Clear();

      uint32_t Hash() const { return m_hash; }

    private:
      friend class PermutationManager;

      void CalculateHash();

      const PermutationManager* m_manager = nullptr;
      BitSet m_values;
      BitSet m_valuesMask;
      uint32_t m_hash = 0;
    };

  } // namespace Runtime
} // namespace Hydra
