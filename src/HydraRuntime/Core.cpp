#include <HydraRuntime/Core.h>

#include <stdlib.h>

namespace Hydra::Runtime
{
  void* DefaultAlloc(size_t numBytes)
  {
    return ::malloc(numBytes);
  }

  void DefaultDealloc(void* ptr)
  {
    ::free(ptr);
  }

  // MurmurHash3_x86_32 :
  // https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp

  inline uint32_t rotl32(uint32_t x, int8_t r)
  {
    return (x << r) | (x >> (32 - r));
  }

  uint32_t DefaultHash(const void* ptr, size_t numBytes)
  {
    const uint8_t* data = (const uint8_t*)ptr;
    const int numBlocks = static_cast<int>(numBytes) / 4;

    uint32_t h1 = 0;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t* blocks = (const uint32_t*)(data + numBlocks * 4);

    for (int i = -numBlocks; i; i++)
    {
      uint32_t k1 = blocks[i];

      k1 *= c1;
      k1 = rotl32(k1, 15);
      k1 *= c2;

      h1 ^= k1;
      h1 = rotl32(h1, 13);
      h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // tail

    const uint8_t* tail = (const uint8_t*)(data + numBlocks * 4);

    uint32_t k1 = 0;

    switch (numBytes & 3)
    {
      case 3:
        k1 ^= tail[2] << 16;
      case 2:
        k1 ^= tail[1] << 8;
      case 1:
        k1 ^= tail[0];
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= numBytes;

    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
  }

  //////////////////////////////////////////////////////////////////////////

  Core::AllocateFunc Core::s_allocateFunc = DefaultAlloc;
  Core::DeallocateFunc Core::s_deallocateFunc = DefaultDealloc;
  Core::HashFunc Core::s_hashFunc = DefaultHash;

  void Core::SetDefaultFunctions()
  {
    s_allocateFunc = DefaultAlloc;
    s_deallocateFunc = DefaultDealloc;
    s_hashFunc = DefaultHash;
  }

  void Core::SetCustomFunctions(AllocateFunc allocateFunc, DeallocateFunc deallocateFunc, HashFunc hashFunc)
  {
    s_allocateFunc = allocateFunc;
    s_deallocateFunc = deallocateFunc;
    s_hashFunc = hashFunc;
  }

} // namespace Hydra::Runtime
