#pragma once

// From https://www.bytesbeneath.com/p/the-arena-custom-memory-allocators

#define MEMORY_ARENA_DEFAULT_ALIGNMENT (2 * sizeof(void *))

struct Memory_Arena {
    s64 size;
    s64 occupied;
    u8 *data;
};

void ma_init(Memory_Arena *arena, s64 size);
void ma_reset(Memory_Arena *arena);

#define MaAllocStruct(arena, Type, ...) (Type *)ma_alloc(arena, sizeof(Type), __VA_ARGS__)
#define MaAllocArray(arena, Type, Count, ...) (Type *)ma_alloc(arena, (Count) * sizeof(Type), __VA_ARGS__)

void *ma_alloc(Memory_Arena *arena, s64 size, s64 alignment = MEMORY_ARENA_DEFAULT_ALIGNMENT);
