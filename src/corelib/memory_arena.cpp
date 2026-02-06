#include "corelib.h"

// From https://www.bytesbeneath.com/p/the-arena-custom-memory-allocators

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

void ma_init(Memory_Arena *arena, s64 size) {
#ifdef PLATFORM_WINDOWS
    arena->data     = (u8 *)VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
#endif
    arena->size     = size;
    arena->occupied = 0;
}

void *ma_alloc(Memory_Arena *arena, s64 size, s64 alignment) {
    size = (size + alignment - 1) & ~(alignment - 1);

    if (arena->occupied + size > arena->size) {
        Assert(!"Memory arena is full!");
        return 0;
    }
    
    void *result     = (void *)(arena->data + arena->occupied);
    arena->occupied += size;
    
    return result;
}

void ma_reset(Memory_Arena *arena) {
    arena->occupied = 0;
}
