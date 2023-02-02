#include <HydraRuntime/BitSet.h>

namespace Hydra::Runtime
{
  BitSet::BitSet() = default;

  BitSet::~BitSet()
  {
    if (m_blockCapacity > 1)
    {
      DeallocateBlocks(m_externalData);
      m_blockCapacity = 1;
      m_externalData = nullptr;
    }
  }

  void BitSet::Clear()
  {
    ClearBlocks(GetDataPtr(), m_blockCount);

    m_blockCount = 0;
    m_blockStartOffset = 0;
  }

  void BitSet::Reserve(uint16_t newBlockStart, uint16_t newBlockCount)
  {
    if ((m_blockCount == 0 || m_blockStartOffset == newBlockStart) && m_blockCapacity >= newBlockCount)
    {
      m_blockStartOffset = newBlockStart;
      m_blockCount = newBlockCount;
      return;
    }

    const uint32_t oldCapacity = m_blockCapacity;
    uint32_t newCapacity = oldCapacity;
    if (newBlockCount > m_blockCapacity)
    {
      newCapacity += (oldCapacity / 2);

      constexpr uint32_t CAPACITY_ALIGNMENT = 4;
      newCapacity = std::max<uint32_t>(newBlockCount, newCapacity);
      newCapacity = (newCapacity + (CAPACITY_ALIGNMENT - 1)) & ~(CAPACITY_ALIGNMENT - 1);
      newCapacity = std::min<uint32_t>(newCapacity, 0xFFFFu);
    }

    BlockType* oldData = GetDataPtr();
    m_blockCapacity = newCapacity;

    if (newCapacity > 1)
    {
      // new external storage
      BlockType* newData = AllocateBlocks(newCapacity);
      if (m_blockCount > 0)
      {
        assert(m_blockStartOffset >= newBlockStart);
        const uint32_t blockCopyOffset = m_blockStartOffset - newBlockStart;
        CopyBlocks(newData + blockCopyOffset, oldData, m_blockCount);
      }
      m_externalData = newData;
    }
    else
    {
      // Re-use inplace storage
      CopyBlocks(GetDataPtr(), oldData, m_blockCount);
    }

    if (oldCapacity > 1)
    {
      DeallocateBlocks(oldData);
    }

    m_blockCount = newBlockCount;
    m_blockStartOffset = newBlockStart;
  }

  void BitSet::EnsureAllocatedRange(uint16_t startIndex, uint16_t numBits /*= 1*/)
  {
    const uint16_t blockIndex = GetBlockIndex(startIndex);
    assert(GetBitIndex(startIndex) + numBits <= BITS_PER_BLOCK); // check that everything fits within one block

    if (m_blockCount == 0)
    {
      Reserve(blockIndex, 1);
    }
    else if (IsInAllocatedRange(blockIndex) == false)
    {
      const uint16_t newBlockStart = std::min(blockIndex, m_blockStartOffset);
      const uint16_t newBlockEnd = std::max<uint32_t>(blockIndex + 1, GetBlockEndOffset());
      const uint16_t newBlockCount = (newBlockEnd - newBlockStart);

      Reserve(newBlockStart, newBlockCount);
    }
  }

  void BitSet::CopyFrom(const BitSet& other)
  {
    const uint32_t oldCount = m_blockCount;
    const uint32_t newCount = other.m_blockCount;

    if (newCount > oldCount)
    {
      Reserve(other.m_blockStartOffset, newCount);
      CopyBlocks(GetDataPtr(), other.GetDataPtr(), newCount);
    }
    else
    {
      CopyBlocks(GetDataPtr(), other.GetDataPtr(), newCount);
      ClearBlocks(GetDataPtr() + newCount, oldCount - newCount);
    }

    m_blockCount = other.m_blockCount;
    m_blockStartOffset = other.m_blockStartOffset;
  }

  void BitSet::MoveFrom(BitSet&& other)
  {
    Clear();

    if (other.m_blockCapacity > 1)
    {
      if (m_blockCapacity > 1)
      {
        DeallocateBlocks(m_externalData);
      }

      m_blockCapacity = other.m_blockCapacity;
      m_externalData = other.m_externalData;
    }
    else
    {
      CopyBlocks(GetDataPtr(), other.GetDataPtr(), other.m_blockCount);
    }

    m_blockCount = other.m_blockCount;
    m_blockStartOffset = other.m_blockStartOffset;

    // reset the other set to not reference the data anymore
    other.m_externalData = nullptr;
    other.m_blockCount = 0;
    other.m_blockCapacity = 0;
    other.m_blockStartOffset = 0;
  }

} // namespace Hydra::Runtime
