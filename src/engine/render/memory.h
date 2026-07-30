#ifndef __ENGINE_RENDER_MEMORY_H
#define __ENGINE_RENDER_MEMORY_H

/* Size of memory pool blocks, in mebibytes (MUST be a multiple of 64) */
#define MEMORY_POOL_SIZE 64LU

/* Minimum alignment of buffer suballocations, in bytes */
#define MEMORY_DEFAULT_ALIGNMENT 16LU

/* Returned by allocation helpers when no block fits */
#define MEMORY_ALLOC_FAILED (~(u64)0)

#include "render_state.h"

typedef struct FreeList {
  struct FreeList *next;
  u64 offset;
  u64 size;
} FreeList;

typedef struct MemoryPool {
  struct MemoryPool *next;
  FreeList *free;
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceSize size;
} MemoryPool;

typedef struct ImageMemoryPool {
  struct ImageMemoryPool *next;
  FreeList *free;
  VkDeviceMemory memory;
  VkDeviceSize size;
} ImageMemoryPool;

/* A completed (sub)allocation, deduplicated by content CRC. Stores the
 * allocation's own offset/size — never pointers into the live free list. */
typedef struct MemoryComponent {
  MemoryPool *pool;
  ImageMemoryPool *image_pool;
  u64 offset;
  u64 size;
  u64 crc;
} MemoryComponent;

typedef struct MemoryManager {
  MemoryPool *pools;
  ImageMemoryPool *image_pools;
  MemoryComponent *components;
} MemoryManager;

typedef struct FreeMemoryBlock {
  MemoryPool *pool;
  u64 offset;
  u8 newblock;
} FreeMemoryBlock;

typedef struct FreeImageMemoryBlock {
  ImageMemoryPool *pool;
  u64 offset;
  u8 newblock;
} FreeImageMemoryBlock;

/**
 * @brief Creates a memory manager.
 *
 * @param state The render state.
 * @param m The memory manager struct to write to.
 */
void memory_create_manager(RenderState *state, MemoryManager *m);

/**
 * @brief Releases all GPU memory and host bookkeeping owned by the manager.
 *
 * Must be called before the device is destroyed. Does not free @p m itself.
 */
void memory_destroy_manager(RenderState *state, MemoryManager *m);

void memory_allocate_pool(RenderState *state, MemoryPool *m);
void memory_extend_pool(RenderState *state, MemoryPool *pool);
void memory_allocate_image_pool(RenderState *state, ImageMemoryPool *m);
void memory_extend_image_pool(RenderState *state, ImageMemoryPool *pool);

/**
 * @brief Reserves @p size bytes of buffer pool memory.
 *
 * If @p crc matches a previous allocation, that allocation is returned
 * instead (newblock == 0) and no new memory is reserved. New allocations
 * are carved out of the free list immediately; callers must not adjust
 * the free list themselves.
 */
void memory_find_free_block(
  RenderState *state, MemoryManager *m, const u64 crc, const u64 size, FreeMemoryBlock *block
);

/**
 * @brief Reserves @p size bytes of image pool memory at @p alignment.
 *
 * Same contract as memory_find_free_block().
 */
void memory_find_free_image_block(
  RenderState *state, MemoryManager *m, const u64 alignment, const u64 crc, const u64 size, FreeImageMemoryBlock *block
);

void memory_free_block(MemoryPool *pool, const u64 offset, const u64 size);
void memory_free_image_block(ImageMemoryPool *pool, const u64 offset, const u64 size);

#endif
