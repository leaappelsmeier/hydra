
namespace Hydra::Runtime
{
  inline BitSet::BitSet(const BitSet& other)
  {
    CopyFrom(other);
  }

  inline BitSet::BitSet(BitSet&& other)
  {
    MoveFrom(std::move(other));
  }

  inline void BitSet::operator=(const BitSet& other)
  {
    CopyFrom(other);
  }

  inline void BitSet::operator=(BitSet&& other)
  {
    MoveFrom(std::move(other));
  }

  inline bool BitSet::operator==(const BitSet& other) const
  {
    if (m_blockCount != other.m_blockCount || m_blockStartOffset != other.m_blockStartOffset)
      return false;

    return memcmp(GetDataPtr(), other.GetDataPtr(), m_blockCount * sizeof(BlockType)) == 0;
  }

  inline bool BitSet::operator!=(const BitSet& other) const
  {
    return !(*this == other);
  }

  inline void BitSet::SetBitValue(uint32_t index, bool value)
  {
    EnsureAllocatedRange(index);

    const BlockType bitMask = 1ull << GetBitIndex(index);

    BlockType& bitBlock = GetDataPtr()[GetBlockIndex(index) - m_blockStartOffset];
    bitBlock = value ? bitBlock | bitMask : bitBlock & ~bitMask;
  }

  inline void BitSet::SetBitValues(uint32_t startIndex, uint32_t numBits, BlockType values)
  {
    EnsureAllocatedRange(startIndex, numBits);

    const uint32_t bitIndex = GetBitIndex(startIndex);
    const BlockType mask = ((1ull << numBits) - 1) << bitIndex;
    const BlockType maskedValues = (values << bitIndex) & mask;

    BlockType& bitBlock = GetDataPtr()[GetBlockIndex(startIndex) - m_blockStartOffset];
    bitBlock = (bitBlock & ~mask) | maskedValues;
  }

  inline void BitSet::SetBitOnes(uint32_t startIndex, uint32_t numBits)
  {
    EnsureAllocatedRange(startIndex, numBits);

    const uint32_t bitIndex = GetBitIndex(startIndex);
    const BlockType onesMask = ((1ull << numBits) - 1) << bitIndex;

    BlockType& bitBlock = GetDataPtr()[GetBlockIndex(startIndex) - m_blockStartOffset];
    bitBlock |= onesMask;
  }

  inline bool BitSet::GetBitValue(uint32_t index) const
  {
    const uint32_t blockIndex = GetBlockIndex(index);
    assert(IsInAllocatedRange(blockIndex));

    BlockType result = GetDataPtr()[blockIndex - m_blockStartOffset];
    result = (result >> GetBitIndex(index)) & 1;
    return result;
  }

  inline BitSet::BlockType BitSet::GetBitValues(uint32_t startIndex, uint32_t numBits = 1) const
  {
    const uint32_t blockIndex = GetBlockIndex(startIndex);
    assert(IsInAllocatedRange(blockIndex));

    BlockType result = GetDataPtr()[blockIndex - m_blockStartOffset];
    const BlockType mask = (1ull << numBits) - 1;
    result = (result >> GetBitIndex(startIndex)) & mask;
    return result;
  }

  inline BitSet::BlockType BitSet::GetBlockOrEmpty(uint32_t blockIndex) const
  {
    return IsInAllocatedRange(blockIndex) ? GetDataPtr()[blockIndex - m_blockStartOffset] : BlockType(0);
  }

  inline uint32_t BitSet::Hash() const
  {
    return Core::Hash(GetDataPtr(), m_blockCount * sizeof(BlockType));
  }

  // static
  inline BitSet::BlockType* BitSet::AllocateBlocks(uint32_t numBlocks)
  {
    BlockType* blocks = (BlockType*)Core::Allocate(numBlocks * sizeof(BlockType));
    ClearBlocks(blocks, numBlocks);
    return blocks;
  }

  // static
  inline void BitSet::DeallocateBlocks(BlockType* blocks)
  {
    Core::Deallocate(blocks);
  }

  // static
  inline void BitSet::CopyBlocks(BlockType* dest, const BlockType* src, uint32_t numBlocks)
  {
    memcpy(dest, src, numBlocks * sizeof(BlockType));
  }

  // static
  inline void BitSet::ClearBlocks(BlockType* blocks, uint32_t numBlocks)
  {
    memset(blocks, 0, numBlocks * sizeof(BlockType));
  }

  inline bool BitSet::IsInAllocatedRange(uint32_t blockIndex) const
  {
    return blockIndex >= m_blockStartOffset && blockIndex < GetBlockEndOffset();
  }

  // static
  inline uint16_t BitSet::GetBlockIndex(uint32_t index)
  {
    return index >> BLOCK_SHIFT;
  }

  // static
  inline uint16_t BitSet::GetBitIndex(uint32_t index)
  {
    return index & BIT_INDEX_MASK;
  }

} // namespace Hydra::Runtime
