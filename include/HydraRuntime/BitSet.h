#pragma once

#include <HydraRuntime/Core.h>

#include <algorithm>
#include <bit>
#include <cassert>

namespace Hydra
{
  namespace Runtime
  {
    // TODO: move to own header
    constexpr unsigned floorlog2(unsigned x)
    {
      return x == 1 ? 0 : 1 + floorlog2(x >> 1);
    }

    constexpr unsigned ceillog2(unsigned x)
    {
      return x == 1 ? 0 : floorlog2(x - 1) + 1;
    }

    inline uint32_t firstBitLow(uint64_t value)
    {
      return static_cast<uint32_t>(std::countr_zero(value));
    }

    //////////////////////////////////////////////////////////////////////////

    class BitSet
    {
    public:
      using BlockType = uint64_t; // TODO: could go bigger with SSE

      enum
      {
        BITS_PER_BLOCK = sizeof(BlockType) * 8,
        BLOCK_SHIFT = ceillog2(BITS_PER_BLOCK),
        BIT_INDEX_MASK = BITS_PER_BLOCK - 1
      };

      BitSet();
      ~BitSet();
      BitSet(const BitSet& other);
      BitSet(BitSet&& other);
      void operator=(const BitSet& other);
      void operator=(BitSet&& other);

      bool operator==(const BitSet& other) const;
      bool operator!=(const BitSet& other) const;

      void SetBitValue(uint32_t index, bool value);

      void SetBitValues(uint32_t startIndex, uint32_t numBits, BlockType values);

      void SetBitOnes(uint32_t startIndex, uint32_t numBits);

      bool GetBitValue(uint32_t index) const;

      BlockType GetBitValues(uint32_t startIndex, uint32_t numBits) const;

      BlockType* GetDataPtr() { return m_blockCapacity <= 1 ? &m_internalData : m_externalData; }
      const BlockType* GetDataPtr() const { return m_blockCapacity <= 1 ? &m_internalData : m_externalData; }

      BlockType GetBlockOrEmpty(uint32_t blockIndex) const;
      uint16_t GetBlockCount() const { return m_blockCount; }
      uint16_t GetBlockStartOffset() const { return m_blockStartOffset; }
      uint16_t GetBlockEndOffset() const { return m_blockStartOffset + m_blockCount; }

      void Clear();
      void Reserve(uint16_t newBlockStart, uint16_t newBlockCount);

      uint32_t Hash() const;

    private:
      static BlockType* AllocateBlocks(uint32_t numBlocks);
      static void DeallocateBlocks(BlockType* blocks);
      static void CopyBlocks(BlockType* dest, const BlockType* src, uint32_t numBlocks);
      static void ClearBlocks(BlockType* blocks, uint32_t numBlocks);

      void CopyFrom(const BitSet& other);
      void MoveFrom(BitSet&& other);

      bool IsInAllocatedRange(uint32_t blockIndex) const;
      void EnsureAllocatedRange(uint16_t startIndex, uint16_t numBits = 1);

      static uint16_t GetBlockIndex(uint32_t index);
      static uint16_t GetBitIndex(uint32_t index);

      uint16_t m_blockCount = 0;
      uint16_t m_blockCapacity = 1;
      uint16_t m_blockStartOffset = 0;

      union
      {
        BlockType m_internalData;
        BlockType* m_externalData = nullptr;
      };
    };
  } // namespace Runtime
} // namespace Hydra

#include <HydraRuntime/BitSet.inl>
