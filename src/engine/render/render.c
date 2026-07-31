#include "render.h"

#include "glyph.h"
#include "shader_compiler.h"

#include <game/interface.h>

void glfw_error_fun(i32 error_code, const char *error_message) {
  printf("\033[1;31m[GLFW Error]\033[0m %s\n", error_message);
}

static inline const char *platform_name(i32 platform) {
  switch (platform) {
  case GLFW_PLATFORM_WAYLAND: return "Wayland"; break;
  case GLFW_PLATFORM_X11: return "X11"; break;
  case GLFW_PLATFORM_COCOA: return "Cocoa"; break;
  case GLFW_PLATFORM_WIN32: return "Win32"; break;
  default: return "Unknown";
  }
}

i32 render_perform(void *args) {
  ThreadData *const td   = (ThreadData *)args;
  mutex_t *lock          = (mutex_t *)td->lock;
  GameState *const state = (GameState *)td->state;
  Renderable **renderables;
  GlyphText *text_objects;

  glfwSetErrorCallback(glfw_error_fun);

  /* Compile (or load cached) shaders before bringing up the window so we fail fast. */
  u32 unlit_generic_vert_size, unlit_generic_frag_size, text_vert_size, text_frag_size;
  u8 *unlit_generic_vert = shader_load("unlit_generic.vert", SHADER_STAGE_VERTEX, &unlit_generic_vert_size);
  u8 *unlit_generic_frag = shader_load("unlit_generic.frag", SHADER_STAGE_FRAGMENT, &unlit_generic_frag_size);
  u8 *text_vert          = shader_load("text.vert", SHADER_STAGE_VERTEX, &text_vert_size);
  u8 *text_frag          = shader_load("text.frag", SHADER_STAGE_FRAGMENT, &text_frag_size);

  if (unlit_generic_vert == NULL || unlit_generic_frag == NULL || text_vert == NULL || text_frag == NULL) {
    shader_free(unlit_generic_vert);
    shader_free(unlit_generic_frag);
    shader_free(text_vert);
    shader_free(text_frag);
    game_add_flag(state, GS_EXIT);

    return 1;
  }

#if !defined(_WIN32) && !defined(__APPLE__)
  if (strcmp(state->preferred_platform, "x11") == 0)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  else
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif

  if (!glfwInit()) {
    game_add_flag(state, GS_EXIT);

    return 1;
  }

  if (glfwVulkanSupported() == GLFW_TRUE) {
    DEBUG_MESSAGE("Vulkan is supported\n");
  } else {
    DEBUG_MESSAGE("Vulkan is not supported\n");

    glfwTerminate();
    game_add_flag(state, GS_EXIT);

    return 1;
  }

  printf("[GLFW] Selected platform: %s\n", platform_name(glfwGetPlatform()));

  RenderState *const render_state = render_state_init();

  if (game_is_debug(state))
    render_state->debug = 1;

  renderables  = (Renderable **)calloc(MAX_RENDERABLES, sizeof(Renderable *));
  text_objects = (GlyphText *)calloc(MAX_TEXT_OBJECTS, sizeof(GlyphText));

  for (u32 i = 0; i < MAX_RENDERABLES; i++) {
    renderables[i] = (Renderable *)calloc(1, sizeof(Renderable));
    renderable_make_default(renderables[i]);
  }

  render_state_create_window(render_state);
  geyser_init_vk(render_state);

  /* Text buffer */

  VkBuffer text_buffer;
  VkDeviceMemory text_memory;
  const VkBufferCreateInfo text_buffer_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO),
                                                .usage                 = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                .size                  = util_mebibytes(8),
                                                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                                .queueFamilyIndexCount = render_state->queue_family_indices_count,
                                                .pQueueFamilyIndices   = render_state->queue_family_indices };

  vkCreateBuffer(render_state->device, &text_buffer_info, NULL, &text_buffer);

  const VkMemoryAllocateInfo text_memory_allocation_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
    .allocationSize  = util_mebibytes(8),
    .memoryTypeIndex = geyser_get_memory_type_index(render_state, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
  };

  vkAllocateMemory(render_state->device, &text_memory_allocation_info, NULL, &text_memory);
  vkBindBufferMemory(render_state->device, text_buffer, text_memory, 0);

  /* Renderable instance buffer: per-renderable data for the single instanced
   * draw, indexed by gl_InstanceIndex in the shader. */

  VkBuffer renderable_buffer;
  VkDeviceMemory renderable_memory;
  const VkDeviceSize renderable_buffer_size       = sizeof(GeyserRenderableData) * MAX_RENDERABLES;
  const VkBufferCreateInfo renderable_buffer_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO),
                                                      .usage                 = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                      .size                  = renderable_buffer_size,
                                                      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                                      .queueFamilyIndexCount = render_state->queue_family_indices_count,
                                                      .pQueueFamilyIndices   = render_state->queue_family_indices };

  vkCreateBuffer(render_state->device, &renderable_buffer_info, NULL, &renderable_buffer);

  const VkMemoryAllocateInfo renderable_memory_allocation_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
    .allocationSize  = renderable_buffer_size,
    .memoryTypeIndex = geyser_get_memory_type_index(
      render_state, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    )
  };

  vkAllocateMemory(render_state->device, &renderable_memory_allocation_info, NULL, &renderable_memory);
  vkBindBufferMemory(render_state->device, renderable_buffer, renderable_memory, 0);

  state->window = render_state->window;

  geyser_cmd_begin_staging(render_state);

  MemoryManager *mm = (MemoryManager *)calloc(1, sizeof(MemoryManager));

  memory_create_manager(render_state, mm);

  render_state->memory_manager = (RsMemoryManager *)mm;

  geyser_create_backbuffer(render_state);

  /* Normal rendering pipeline: all renderables are drawn in one instanced
   * call, pulling per-instance data from a storage buffer and sampling from
   * an array of textures. */
  const VkDescriptorSetLayoutBinding descriptor_bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL },
    { 1,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      GEYSER_RENDERABLE_TEXTURE_SLOTS,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      NULL },
  };

  const VkPushConstantRange push_constant_range[] = {
    { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GeyserPushConstants) },
  };

  GeyserVertexInputDescription vertex_input_description = geyser_create_vertex_input_description();

  VkDescriptorSetLayout *descriptor_set_layouts = (VkDescriptorSetLayout *)calloc(1, sizeof(VkDescriptorSetLayout));

  geyser_create_descriptor_set_layout_binding(render_state, descriptor_bindings, 2, descriptor_set_layouts);

  geyser_create_pipeline(
    render_state,
    descriptor_set_layouts,
    1,
    push_constant_range,
    1,
    unlit_generic_vert,
    unlit_generic_vert_size,
    unlit_generic_frag,
    unlit_generic_frag_size,
    &vertex_input_description,
    (GeyserPipeline *)&render_state->pipeline
  );

  /* Text rendering pipeline */
  const VkDescriptorSetLayoutBinding text_descriptor_bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL },
    { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
  };

  VkDescriptorSetLayout *text_descriptor_set_layouts =
    (VkDescriptorSetLayout *)calloc(1, sizeof(VkDescriptorSetLayout));

  geyser_create_descriptor_set_layout_binding(
    render_state, text_descriptor_bindings, 2, &text_descriptor_set_layouts[0]
  );

  const VkPushConstantRange text_push_constant_range[] = {
    { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GeyserPushConstants) },
  };

  GeyserVertexInputDescription text_vertex_input_description = geyser_create_vertex_input_description();

  geyser_create_pipeline(
    render_state,
    text_descriptor_set_layouts,
    1,
    text_push_constant_range,
    1,
    text_vert,
    text_vert_size,
    text_frag,
    text_frag_size,
    &text_vertex_input_description,
    (GeyserPipeline *)&render_state->text_pipeline
  );

  shader_free(unlit_generic_vert);
  shader_free(unlit_generic_frag);
  shader_free(text_vert);
  shader_free(text_frag);

  /* One descriptor set per texture batch. A batch is a contiguous range of
   * instances that together use at most GEYSER_RENDERABLE_TEXTURE_SLOTS unique
   * textures; a typical frame fits in a single batch (= one draw call). */
  VkDescriptorSet renderable_descriptor_sets[GEYSER_MAX_RENDERABLE_BATCHES];

  {
    VkDescriptorSetLayout batch_layouts[GEYSER_MAX_RENDERABLE_BATCHES];

    for (u32 i = 0; i < GEYSER_MAX_RENDERABLE_BATCHES; i++)
      batch_layouts[i] = descriptor_set_layouts[0];

    const VkDescriptorSetAllocateInfo batch_allocate_info = {
      GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO),
      .descriptorPool     = render_state->descriptor_pool,
      .descriptorSetCount = GEYSER_MAX_RENDERABLE_BATCHES,
      .pSetLayouts        = batch_layouts
    };

    geyser_success_or_message(
      vkAllocateDescriptorSets(render_state->device, &batch_allocate_info, renderable_descriptor_sets),
      "Failed to allocate the renderable descriptor sets!"
    );

    const VkDescriptorBufferInfo renderable_buffer_descriptor_info = { .buffer = renderable_buffer,
                                                                       .offset = 0,
                                                                       .range  = VK_WHOLE_SIZE };

    for (u32 i = 0; i < GEYSER_MAX_RENDERABLE_BATCHES; i++) {
      VkWriteDescriptorSet buffer_write;

      memset(&buffer_write, 0, sizeof(buffer_write));

      buffer_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      buffer_write.dstSet          = renderable_descriptor_sets[i];
      buffer_write.dstBinding      = 0;
      buffer_write.descriptorCount = 1;
      buffer_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      buffer_write.pBufferInfo     = &renderable_buffer_descriptor_info;

      vkUpdateDescriptorSets(render_state->device, 1, &buffer_write, 0, NULL);
    }
  }

  GeyserRenderableData *renderable_data_handle;

  geyser_success_or_message(
    vkMapMemory(
      render_state->device, renderable_memory, 0, renderable_buffer_size, 0, (void **)&renderable_data_handle
    ),
    "Failed to map renderable instance memory!"
  );

  void *text_data_handle;

  geyser_success_or_message(
    vkMapMemory(render_state->device, text_memory, 0, sizeof(Glyph), 0, &text_data_handle), "Failed to map text memory!"
  );

  GeyserTexture text_texture;
  Image text_texture_img;
  const char *path = "assets/ui/font.png";
  asset_load_image(&text_texture_img, path);

  geyser_create_texture(
    render_state, 0, vector_make2((f32)text_texture_img.width, (f32)text_texture_img.height), &text_texture
  );
  geyser_set_image_memory(render_state, &text_texture.base.base, &text_texture_img);

  text_texture.copy = 0;

  asset_unload_image(&text_texture_img);

  VkDescriptorSet *text_descriptor_set                 = (VkDescriptorSet *)calloc(1, sizeof(VkDescriptorSet));
  VkDescriptorSetAllocateInfo descriptor_allocate_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                           .pNext = NULL,
                                                           .descriptorPool = render_state->descriptor_pool,
                                                           .descriptorSetCount =
                                                             render_state->text_pipeline.descriptor_set_layouts_count,
                                                           .pSetLayouts =
                                                             render_state->text_pipeline.descriptor_set_layouts };

  geyser_success_or_message(
    vkAllocateDescriptorSets(render_state->device, &descriptor_allocate_info, text_descriptor_set),
    "Failed to allocate the text descriptor sets!"
  );

  VkDescriptorBufferInfo descriptor_info;
  VkDescriptorImageInfo descriptor_image_info;
  VkWriteDescriptorSet descriptor_write;

  descriptor_image_info.sampler     = text_texture.sampler;
  descriptor_image_info.imageView   = text_texture.base.view;
  descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  descriptor_info.buffer = text_buffer;
  descriptor_info.offset = 0;
  descriptor_info.range  = VK_WHOLE_SIZE;

  memset(&descriptor_write, 0, sizeof(descriptor_write));

  descriptor_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet          = text_descriptor_set[0];
  descriptor_write.dstBinding      = 0;
  descriptor_write.descriptorCount = 1;
  descriptor_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptor_write.pBufferInfo     = &descriptor_info;

  vkUpdateDescriptorSets(render_state->device, 1, &descriptor_write, 0, NULL);

  memset(&descriptor_write, 0, sizeof(descriptor_write));

  descriptor_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet          = text_descriptor_set[0];
  descriptor_write.dstBinding      = 1;
  descriptor_write.descriptorCount = 1;
  descriptor_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_write.pImageInfo      = &descriptor_image_info;

  vkUpdateDescriptorSets(render_state->device, 1, &descriptor_write, 0, NULL);

  geyser_cmd_end_staging(render_state);

  i64 delay = state->fps_max != 0 ? 1000000 / state->fps_max : 0;
  i64 start_time, end_time, gpu_start, gpu_end;

  typedef struct RenderableBatch {
    GeyserTexture *textures[GEYSER_RENDERABLE_TEXTURE_SLOTS];
    u32 texture_count;
    u32 first_instance;
    u32 instance_count;
  } RenderableBatch;

  RenderableBatch batches[GEYSER_MAX_RENDERABLE_BATCHES];

  render_state->init_time = platform_time();

  /* Actual rendering loop */
  while (!game_should_exit(state)) {
    if (render_state_should_close(render_state)) {
      game_add_flag(state, GS_EXIT);
      break;
    }

    start_time = platform_time_usec();

#if defined(_WIN32) || defined(__APPLE__)
    /* Windows polls on the render thread; macOS must poll on the main thread,
     * which is the render thread here. Elsewhere the input thread polls. */
    glfwPollEvents();
#endif

    geyser_cmd_begin_staging(render_state);

    game_adjust_renderables(state, lock, render_state, renderables, MAX_RENDERABLES, text_objects, MAX_TEXT_OBJECTS);

    u32 total_glyphs = 0;

    for (u32 i = 0; i < MAX_TEXT_OBJECTS; i++) {
      if (text_objects[i].size == 0)
        break;

      if (total_glyphs + text_objects[i].size > GEYSER_MAX_GLYPHS) {
        printf(
          "Too many glyphs in scene (did you clear?): %u, max: %u\n",
          total_glyphs + text_objects[i].size,
          GEYSER_MAX_GLYPHS
        );
        break;
      }

      memcpy(
        (u8 *)text_data_handle + sizeof(Glyph) * total_glyphs,
        text_objects[i].glyphs,
        sizeof(Glyph) * text_objects[i].size
      );

      total_glyphs += text_objects[i].size;
    }

    /* Write per-renderable instance data straight into the mapped storage
     * buffer, grouping instances into texture batches as we go. Almost every
     * frame this yields a single batch, and thus a single draw call. */
    u32 batch_count     = 0;
    u32 total_instances = 0;

    for (u32 i = 0; i < MAX_RENDERABLES; i++) {
      Renderable *const r = renderables[i];

      /* Since renderables are sorted in a way such that non-active come last, we can just stop once we spot one of
       * these */
      if (r->active == GS_FALSE)
        break;

      if (r->uv == NULL || r->vertices_count == 0 || r->texture == NULL)
        continue;

      renderable_interpolate(r);

      GeyserRenderableData *const instance = &renderable_data_handle[total_instances];

      instance->quaternion = quaternion_to_vec(r->rotation);
      instance->position   = r->position;
      instance->color      = r->color;
      instance->scale      = r->scale;
      instance->uv_offset  = r->uv_offset;

      memcpy(instance->uvs, r->uv, sizeof(Vector2) * 6);

      RenderableBatch *batch = batch_count > 0 ? &batches[batch_count - 1] : NULL;
      i32 slot               = -1;

      if (batch != NULL) {
        for (u32 j = 0; j < batch->texture_count; j++) {
          if (batch->textures[j] == r->texture) {
            slot = (i32)j;
            break;
          }
        }
      }

      if (slot < 0) {
        if (batch == NULL || batch->texture_count == GEYSER_RENDERABLE_TEXTURE_SLOTS) {
          if (batch_count == GEYSER_MAX_RENDERABLE_BATCHES) {
            printf("Too many unique renderable textures in scene, skipping the rest\n");
            break;
          }

          batch                 = &batches[batch_count++];
          batch->texture_count  = 0;
          batch->first_instance = total_instances;
          batch->instance_count = 0;
        }

        slot                                    = (i32)batch->texture_count;
        batch->textures[batch->texture_count++] = r->texture;
      }

      instance->texture_index = slot;

      batch->instance_count++;
      total_instances++;
    }

    geyser_cmd_end_staging(render_state);

    /* Point each batch's descriptor set at the textures it uses. The queue is
     * idle between frames, so updating the sets here is safe. */
    for (u32 b = 0; b < batch_count; b++) {
      VkDescriptorImageInfo image_infos[GEYSER_RENDERABLE_TEXTURE_SLOTS];

      for (u32 s = 0; s < GEYSER_RENDERABLE_TEXTURE_SLOTS; s++) {
        /* Every array element must be a valid descriptor, so unused slots
         * repeat the first texture. */
        const GeyserTexture *tex = batches[b].textures[s < batches[b].texture_count ? s : 0];

        image_infos[s].sampler     = tex->sampler;
        image_infos[s].imageView   = tex->base.view;
        image_infos[s].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      }

      VkWriteDescriptorSet texture_write;

      memset(&texture_write, 0, sizeof(texture_write));

      texture_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      texture_write.dstSet          = renderable_descriptor_sets[b];
      texture_write.dstBinding      = 1;
      texture_write.descriptorCount = GEYSER_RENDERABLE_TEXTURE_SLOTS;
      texture_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      texture_write.pImageInfo      = image_infos;

      vkUpdateDescriptorSets(render_state->device, 1, &texture_write, 0, NULL);
    }

    render_state->rendering = 1;

    geyser_cmd_begin_draw(render_state);
    geyser_cmd_begin_renderpass(render_state);

    /* Render game/world */

    vkCmdBindPipeline(render_state->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_state->pipeline.pipeline);

    geyser_cmd_set_viewport(render_state);

    const GeyserPushConstants push_constants = { .camera = render_state->camera_transform };

    vkCmdPushConstants(
      render_state->command_buffer,
      render_state->pipeline.pipeline_layout,
      VK_SHADER_STAGE_VERTEX_BIT,
      0,
      sizeof(GeyserPushConstants),
      &push_constants
    );

    for (u32 b = 0; b < batch_count; b++) {
      vkCmdBindDescriptorSets(
        render_state->command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        render_state->pipeline.pipeline_layout,
        0,
        1,
        &renderable_descriptor_sets[b],
        0,
        NULL
      );

      vkCmdDraw(render_state->command_buffer, 6, batches[b].instance_count, 0, batches[b].first_instance);
    }

    /* Render text */

    vkCmdBindPipeline(
      render_state->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_state->text_pipeline.pipeline
    );

    vkCmdBindDescriptorSets(
      render_state->command_buffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      render_state->text_pipeline.pipeline_layout,
      0,
      1,
      text_descriptor_set,
      0,
      NULL
    );

    vkCmdPushConstants(
      render_state->command_buffer,
      render_state->text_pipeline.pipeline_layout,
      VK_SHADER_STAGE_VERTEX_BIT,
      0,
      sizeof(GeyserPushConstants),
      &push_constants
    );

    vkCmdDraw(render_state->command_buffer, 6, total_glyphs, 0, 0);

    gpu_start = platform_time_usec();

    geyser_cmd_end_renderpass(render_state);
    geyser_cmd_end_draw(render_state);

    gpu_end = platform_time_usec();

    render_state->rendering = 0;
    render_state->current_frame++;

    end_time = platform_time_usec();

    if (game_is_verbose(state) && render_state->current_frame % 100 == 0) {
      const i64 total_time = end_time - start_time;
      const i64 gpu_time   = gpu_end - gpu_start;
      const i64 cpu_time   = total_time - gpu_time;

      printf(
        "Frame times\nTotal: %liμs (%li FPS)\nCPU: %liμs (%li FPS)\nGPU: %liμs (%li FPS)\n",
        total_time,
        1000000 / total_time,
        cpu_time,
        1000000 / cpu_time,
        gpu_time,
        1000000 / gpu_time
      );
    }

    if (state->fps_max > 0 && (end_time - start_time) < delay)
      platform_usleep(delay - (end_time - start_time));
  }

  for (u32 i = 0; i < MAX_RENDERABLES; i++)
    renderable_free(render_state, renderables[i]);

  memory_destroy_manager(render_state, mm);
  free(mm);
  render_state->memory_manager = NULL;

  vkUnmapMemory(render_state->device, text_memory);
  vkUnmapMemory(render_state->device, renderable_memory);
  free(renderables);
  geyser_destroy_vk(render_state);
  render_state_destroy(render_state);
  glfwTerminate();

  return 0;
}
