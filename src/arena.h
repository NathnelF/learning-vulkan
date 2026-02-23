#pragma once

#include "types.h"

struct Arena
{
    u8 *memory;
    u64 offset;
    u64 size;
};

#define DEFAULT_ALIGNMENT 16

bool IsPowerOfTwo(u64 x);
u64 ForwardAlign(u64 ptr, u64 alignment);
Arena ArenaInit(void *backing, u64 backing_size);
void *ArenaPushAligned(Arena *arena, u64 push_size, u64 alignment);
void *ArenaPush(Arena *arena, u64 push_size);
void ArenaReset(Arena *arena);
