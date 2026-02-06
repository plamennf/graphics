#pragma once

void init_temporary_storage(s64 size);
void reset_temporary_storage();

bool is_temporary_storage_initialized();

s64 get_temporary_storage_mark();
void set_temporary_storage_mark(s64 mark);

#define TAllocStruct(Type, ...) (Type *)talloc(sizeof(Type), __VA_ARGS__)
#define TAllocArray(Type, Count, ...) (Type *)talloc((Count) * sizeof(Type), __VA_ARGS__)

void *talloc(s64 size, s64 alignment = MEMORY_ARENA_DEFAULT_ALIGNMENT);
