#include "arena.h"

bool IsPowerOfTwo(u64 x)
{
    return (x & (x - 1)) == 0;
}

u64 ForwardAlign(u64 ptr, u64 alignment)
{
    assert(IsPowerOfTwo(alignment));

    u64 modulo = ptr & (alignment - 1);

    if (modulo != 0)
    {
        ptr += alignment - modulo;
    }

    return ptr;
}

Arena ArenaInit(void *backing, u64 backing_size)
{
    Arena arena = {
        .memory = (u8 *)backing,
        .offset = 0,
        .size = backing_size,
    };
    return arena;
}

void *ArenaPushAlign(Arena *arena, u64 push_size, u64 alignment)
{
    u64 current_address = (u64)arena->memory + arena->offset;

    u64 offset = ForwardAlign(current_address, alignment);

    offset -= (u64)arena->memory;

    assert(arena->offset + push_size <= arena->size);

    void *ptr = &arena->memory[offset];
    arena->offset = offset + push_size;
    memset(ptr, 0, push_size);
    return ptr;
}

void *ArenaPush(Arena *arena, u64 push_size)
{
    return ArenaPushAlign(arena, push_size, DEFAULT_ALIGNMENT);
}

void ArenaReset(Arena *arena)
{
    arena->offset = 0;
}
