#pragma once

#include <cstddef>
#include <stdint.h>

namespace Hydra
{
  namespace Runtime
  {
    class Core
    {
    public:
      using AllocateFunc = void* (*)(size_t numBytes);
      using DeallocateFunc = void (*)(void* ptr);
      using HashFunc = uint32_t (*)(const void* ptr, size_t numBytes);

      static void* Allocate(size_t numBytes) { return s_allocateFunc(numBytes); }
      static void Deallocate(void* ptr) { s_deallocateFunc(ptr); }
      static uint32_t Hash(const void* ptr, size_t numBytes) { return s_hashFunc(ptr, numBytes); }

      static void SetDefaultFunctions();
      static void SetCustomFunctions(AllocateFunc allocateFunc, DeallocateFunc deallocateFunc, HashFunc hashFunc);

    private:
      static AllocateFunc s_allocateFunc;
      static DeallocateFunc s_deallocateFunc;
      static HashFunc s_hashFunc;
    };
  } // namespace Runtime
} // namespace Hydra
