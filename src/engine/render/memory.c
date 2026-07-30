#include "memory.h"

#include "../crc64.h"
#include "../limits.h"
#include "../util.h"
#include "geyser.h"

static u64 _align_up(const u64 value, const u64 alignment) {
  if (alignment < 2)
    return value;

  return (value + alignment - 1) / alignment * alignment;
}

/**
 * Carves an aligned block of `size` bytes out of the free list.
 *
 * The list is kept sorted by offset; nodes are split, shrunk or unlinked as
 * needed and candidate nodes are never mutated unless the allocation succeeds.
 *
 * Returns the aligned offset of the allocation, or MEMORY_ALLOC_FAILED.
 */
static u64 _free_list_carve(FreeList **head, const u64 alignment, const u64 size) {
  FreeList *prev = NULL;

  for (FreeList *l = *head; l != NULL; prev = l, l = l->next) {
    const u64 aligned_offset = _align_up(l->offset, alignment);
    const u64 padding        = aligned_offset - l->offset;

    if (l->size < padding || l->size - padding < size)
      continue;

    const u64 tail = l->size - padding - size;

    if (padding > 0 && tail > 0) {
      /* Keep the padding in this node, spawn a new node for the tail */
      FreeList *tail_node = (FreeList *)calloc(1, sizeof(FreeList));
      tail_node->offset   = aligned_offset + size;
      tail_node->size     = tail;
      tail_node->next     = l->next;

      l->size = padding;
      l->next = tail_node;
    } else if (padding > 0) {
      l->size = padding;
    } else if (tail > 0) {
      l->offset = aligned_offset + size;
      l->size   = tail;
    } else {
      /* Exact fit: unlink the node */
      if (prev != NULL)
        prev->next = l->next;
      else
        *head = l->next;

      free(l);
    }

    return aligned_offset;
  }

  return MEMORY_ALLOC_FAILED;
}

/**
 * Returns a block to the free list, keeping it sorted by offset and
 * coalescing with both neighbors when they are adjacent.
 */
static void _free_list_release(FreeList **head, const u64 offset, const u64 size) {
  if (size == 0)
    return;

  FreeList *prev = NULL;
  FreeList *next = *head;

  while (next != NULL && next->offset < offset) {
    prev = next;
    next = next->next;
  }

  if (prev != NULL && prev->offset + prev->size == offset) {
    prev->size += size;

    if (next != NULL && prev->offset + prev->size == next->offset) {
      prev->size += next->size;
      prev->next = next->next;

      free(next);
    }

    return;
  }

  if (next != NULL && offset + size == next->offset) {
    next->offset = offset;
    next->size += size;

    return;
  }

  FreeList *node = (FreeList *)calloc(1, sizeof(FreeList));
  node->offset   = offset;
  node->size     = size;
  node->next     = next;

  if (prev != NULL)
    prev->next = node;
  else
    *head = node;
}

static void _free_list_destroy(FreeList *l) {
  while (l != NULL) {
    FreeList *next = l->next;
    free(l);
    l = next;
  }
}

MemoryComponent *_find_existing_memory_block(MemoryManager *manager, const u64 crc) {
  if (crc == 0)
    return NULL;

  for (u32 i = 0; i < MAX_MEMORY_COMPONENTS; i++)
    if (manager->components[i].crc != 0 && manager->components[i].crc == crc)
      return &manager->components[i];

  return NULL;
}

void _add_memory_component(
  MemoryManager *manager, const u64 crc, MemoryPool *mp, ImageMemoryPool *imp, const u64 offset, const u64 size
) {
  if (crc == 0)
    return;

  for (u32 i = 0; i < MAX_MEMORY_COMPONENTS; i++) {
    if (manager->components[i].crc == 0) {
      manager->components[i].crc        = crc;
      manager->components[i].pool       = mp;
      manager->components[i].image_pool = imp;
      manager->components[i].offset     = offset;
      manager->components[i].size       = size;

      return;
    }
  }

  printf("[Geyser Warning] Memory component table is full; allocation deduplication disabled for this block\n");
}

void memory_create_manager(RenderState *state, MemoryManager *m) {
  m->pools       = (MemoryPool *)calloc(1, sizeof(MemoryPool));
  m->image_pools = (ImageMemoryPool *)calloc(1, sizeof(ImageMemoryPool));
  m->components  = (MemoryComponent *)calloc(MAX_MEMORY_COMPONENTS, sizeof(MemoryComponent));

  memory_allocate_pool(state, m->pools);
  memory_allocate_image_pool(state, m->image_pools);
}

void memory_destroy_manager(RenderState *state, MemoryManager *m) {
  MemoryPool *pool = m->pools;

  while (pool != NULL) {
    MemoryPool *next = pool->next;

    _free_list_destroy(pool->free);
    vkDestroyBuffer(state->device, pool->buffer, NULL);
    vkFreeMemory(state->device, pool->memory, NULL);
    free(pool);

    pool = next;
  }

  ImageMemoryPool *image_pool = m->image_pools;

  while (image_pool != NULL) {
    ImageMemoryPool *next = image_pool->next;

    _free_list_destroy(image_pool->free);
    vkFreeMemory(state->device, image_pool->memory, NULL);
    free(image_pool);

    image_pool = next;
  }

  free(m->components);

  m->pools       = NULL;
  m->image_pools = NULL;
  m->components  = NULL;
}

void memory_allocate_pool(RenderState *state, MemoryPool *mp) {
  mp->free         = (FreeList *)calloc(1, sizeof(FreeList));
  mp->free->next   = NULL;
  mp->free->offset = 0;
  mp->free->size   = util_mebibytes(MEMORY_POOL_SIZE);

  VkMemoryRequirements memory_requirements;

  const VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = NULL,
    .size  = util_mebibytes(MEMORY_POOL_SIZE),
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .flags = 0,
  };

  geyser_success_or_message(
    vkCreateBuffer(state->device, &buffer_info, NULL, &mp->buffer), "Failed to create a memory pool buffer!"
  );

  vkGetBufferMemoryRequirements(state->device, mp->buffer, &memory_requirements);

  const VkMemoryAllocateInfo memory_alloc_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
    .allocationSize  = memory_requirements.size,
    .memoryTypeIndex = geyser_get_memory_type_index_filtered(
      state, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory_requirements.memoryTypeBits
    )
  };

  geyser_success_or_message(
    vkAllocateMemory(state->device, &memory_alloc_info, NULL, &mp->memory), "Failed to allocate memory pool memory!"
  );

  mp->size = memory_alloc_info.allocationSize;

  vkBindBufferMemory(state->device, mp->buffer, mp->memory, 0);
}

void memory_allocate_image_pool(RenderState *state, ImageMemoryPool *mp) {
  mp->free         = (FreeList *)calloc(1, sizeof(FreeList));
  mp->free->next   = NULL;
  mp->free->offset = 0;
  mp->free->size   = util_mebibytes(MEMORY_POOL_SIZE);

  /* Images bound to this pool must use a memory type allowed by their
   * memory requirements, so probe with a throwaway image matching the
   * ones geyser_create_image_view() creates. */
  u32 image_type_bits = ~0U;

  const VkImageCreateInfo probe_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO),
                                         .imageType   = VK_IMAGE_TYPE_2D,
                                         .format      = state->preferred_color_format,
                                         .extent      = { .width = 16, .height = 16, .depth = 1 },
                                         .mipLevels   = 1,
                                         .arrayLayers = 1,
                                         .samples     = VK_SAMPLE_COUNT_1_BIT,
                                         .tiling      = VK_IMAGE_TILING_OPTIMAL,
                                         .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                         .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                         .queueFamilyIndexCount = 0,
                                         .pQueueFamilyIndices   = NULL,
                                         .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED };

  VkImage probe_image;

  if (vkCreateImage(state->device, &probe_info, NULL, &probe_image) == VK_SUCCESS) {
    VkMemoryRequirements probe_requirements;
    vkGetImageMemoryRequirements(state->device, probe_image, &probe_requirements);
    image_type_bits = probe_requirements.memoryTypeBits;
    vkDestroyImage(state->device, probe_image, NULL);
  }

  const VkMemoryAllocateInfo memory_alloc_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
    .allocationSize = util_mebibytes(MEMORY_POOL_SIZE),
    .memoryTypeIndex =
      geyser_get_memory_type_index_filtered(state, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_type_bits)
  };

  geyser_success_or_message(
    vkAllocateMemory(state->device, &memory_alloc_info, NULL, &mp->memory), "Failed to allocate memory pool memory!"
  );

  mp->size = memory_alloc_info.allocationSize;
}

void memory_extend_pool(RenderState *state, MemoryPool *pool) {
  while (pool->next != NULL)
    pool = pool->next;

  pool->next = (MemoryPool *)calloc(1, sizeof(MemoryPool));
  memory_allocate_pool(state, pool->next);
}

void memory_extend_image_pool(RenderState *state, ImageMemoryPool *pool) {
  while (pool->next != NULL)
    pool = pool->next;

  pool->next = (ImageMemoryPool *)calloc(1, sizeof(ImageMemoryPool));
  memory_allocate_image_pool(state, pool->next);
}

static void _validate_request(const MemoryManager *m, const u64 size) {
  if (size > util_mebibytes(MEMORY_POOL_SIZE)) {
    printf(
      "[Geyser Error] Cannot assign more than %lu MiB of GPU memory! (%llu bytes requested)\n",
      MEMORY_POOL_SIZE,
      (unsigned long long)size
    );
    abort();
  }

  if (m == NULL || m->pools == NULL || m->image_pools == NULL) {
    printf("[Geyser Error] Memory manager is not initialized!\n");
    abort();
  }
}

void memory_find_free_block(
  RenderState *state, MemoryManager *m, const u64 crc, const u64 size, FreeMemoryBlock *block
) {
  _validate_request(m, size);

  const MemoryComponent *mc = _find_existing_memory_block(m, crc);

  if (mc != NULL && mc->pool != NULL) {
    block->pool     = mc->pool;
    block->offset   = mc->offset;
    block->newblock = 0;

    return;
  }

  for (;;) {
    for (MemoryPool *pool = m->pools; pool != NULL; pool = pool->next) {
      const u64 offset = _free_list_carve(&pool->free, MEMORY_DEFAULT_ALIGNMENT, size);

      if (offset != MEMORY_ALLOC_FAILED) {
        _add_memory_component(m, crc, pool, NULL, offset, size);

        block->pool     = pool;
        block->offset   = offset;
        block->newblock = 1;

        return;
      }
    }

    /* A fresh pool always satisfies the request (size <= pool size, offset 0
     * is aligned), so this loops at most twice. */
    memory_extend_pool(state, m->pools);
  }
}

void memory_find_free_image_block(
  RenderState *state, MemoryManager *m, const u64 alignment, const u64 crc, const u64 size, FreeImageMemoryBlock *block
) {
  _validate_request(m, size);

  const MemoryComponent *mc = _find_existing_memory_block(m, crc);

  if (mc != NULL && mc->image_pool != NULL) {
    block->pool     = mc->image_pool;
    block->offset   = mc->offset;
    block->newblock = 0;

    return;
  }

  for (;;) {
    for (ImageMemoryPool *pool = m->image_pools; pool != NULL; pool = pool->next) {
      const u64 offset = _free_list_carve(&pool->free, alignment, size);

      if (offset != MEMORY_ALLOC_FAILED) {
        _add_memory_component(m, crc, NULL, pool, offset, size);

        block->pool     = pool;
        block->offset   = offset;
        block->newblock = 1;

        return;
      }
    }

    memory_extend_image_pool(state, m->image_pools);
  }
}

void memory_free_block(MemoryPool *pool, const u64 offset, const u64 size) {
  if (pool == NULL)
    return;

  _free_list_release(&pool->free, offset, size);
}

void memory_free_image_block(ImageMemoryPool *pool, const u64 offset, const u64 size) {
  if (pool == NULL)
    return;

  _free_list_release(&pool->free, offset, size);
}
