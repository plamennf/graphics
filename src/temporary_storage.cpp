#include "pch.h"

static bool temporary_storage_initted;
static Memory_Arena temporary_storage_arena;

void init_temporary_storage(s64 size) {
    Assert(!temporary_storage_initted);

    ma_init(&temporary_storage_arena, size);
    
    temporary_storage_initted  = true;
}

void reset_temporary_storage() {
    ma_reset(&temporary_storage_arena);
}

bool is_temporary_storage_initialized() {
    return temporary_storage_initted;
}

s64 get_temporary_storage_mark() {
    return temporary_storage_arena.occupied;
}

void set_temporary_storage_mark(s64 mark) {
    Assert(mark >= 0);
    Assert(mark < temporary_storage_arena.size);
    temporary_storage_arena.occupied = mark;
}

void *talloc(s64 size, s64 alignment) {
    return ma_alloc(&temporary_storage_arena, size, alignment);
}
