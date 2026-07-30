#include "render.h"

#include "glyph.h"
#include "shaders/shaders.h"

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

  state->window = render_state->window;

  geyser_cmd_begin_staging(render_state);

  MemoryManager *mm = (MemoryManager *)calloc(1, sizeof(MemoryManager));

  memory_create_manager(render_state, mm);

  render_state->memory_manager = (RsMemoryManager *)mm;

  geyser_create_backbuffer(render_state);

  /* Normal rendering pipeline */
  const VkDescriptorSetLayoutBinding descriptor_bindings[] = {
    { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL }
  };

  const VkPushConstantRange push_constant_range[] = {
    { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GeyserPushConstants) },
  };

  GeyserVertexInputDescription vertex_input_description = geyser_create_vertex_input_description();

  geyser_add_vertex_input_binding(&vertex_input_description, 0, 8, VK_VERTEX_INPUT_RATE_VERTEX);

  geyser_add_vertex_input_attribute(&vertex_input_description, 0, 0, VK_FORMAT_R32G32_SFLOAT, 0);

  VkDescriptorSetLayout *descriptor_set_layouts = (VkDescriptorSetLayout *)calloc(1, sizeof(VkDescriptorSetLayout));

  geyser_create_descriptor_set_layout_binding(render_state, descriptor_bindings, 1, descriptor_set_layouts);

  geyser_create_pipeline(
    render_state,
    descriptor_set_layouts,
    1,
    push_constant_range,
    1,
    unlit_generic_vert_data,
    unlit_generic_vert_data_size,
    unlit_generic_frag_data,
    unlit_generic_frag_data_size,
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
    { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GeyserTextPushConstants) },
  };

  GeyserVertexInputDescription text_vertex_input_description = geyser_create_vertex_input_description();

  geyser_create_pipeline(
    render_state,
    text_descriptor_set_layouts,
    1,
    text_push_constant_range,
    1,
    text_vert_data,
    text_vert_data_size,
    text_frag_data,
    text_frag_data_size,
    &text_vertex_input_description,
    (GeyserPipeline *)&render_state->text_pipeline
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
  VkDeviceSize offsets[1]       = { 0 };
  VkDeviceSize first_offsets[1] = { 0 };

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

    geyser_cmd_end_staging(render_state);

    render_state->rendering = 1;

    geyser_cmd_begin_draw(render_state);
    geyser_cmd_begin_renderpass(render_state);

    /* Render game/world */

    vkCmdBindPipeline(render_state->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_state->pipeline.pipeline);

    geyser_cmd_set_viewport(render_state);

    GeyserPushConstants push_constants = { .camera = render_state->camera_transform };

    u32 first = 0;

    for (u32 i = 0; i < MAX_RENDERABLES; i++) {
      if (renderables[i]->active == GS_TRUE && renderables[i]->uv != NULL) {
        first = i;
        break;
      }
    }

    first_offsets[0] = renderables[first]->offset;
    vkCmdBindVertexBuffers(render_state->command_buffer, 0, 1, &renderables[first]->pool->buffer, first_offsets);

    Renderable *last_renderable = NULL;

    for (u32 i = first; i < MAX_RENDERABLES; i++) {
      /* Since renderables are sorted in a way such that non-active come last, we can just stop once we spot one of
       * these */
      if (renderables[i]->active == GS_FALSE || renderables[i]->uv == NULL)
        continue;

      if (renderables[i]->vertices_count > 0) {
        renderable_interpolate(renderables[i]);

        push_constants.quaternion   = quaternion_to_vec(renderables[i]->rotation);
        push_constants.position     = renderables[i]->position;
        push_constants.vertex_color = renderables[i]->color;
        push_constants.scale        = renderables[i]->scale;
        push_constants.uv_offset    = renderables[i]->uv_offset;
        push_constants.camera       = render_state->camera_transform;

        if (
          last_renderable != NULL &&
          memcmp(renderables[i]->uv, last_renderable->uv, sizeof(Vector2) * renderables[i]->vertices_count) != 0
        ) {
          offsets[0] = renderables[i]->offset;
          vkCmdBindVertexBuffers(render_state->command_buffer, 0, 1, &renderables[i]->pool->buffer, offsets);
        }

        if (last_renderable == NULL || strcmp(renderables[i]->texture_path, last_renderable->texture_path) != 0) {
          vkCmdBindDescriptorSets(
            render_state->command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            render_state->pipeline.pipeline_layout,
            0,
            1,
            &renderables[i]->texture->descriptor_set,
            0,
            NULL
          );
        }

        vkCmdPushConstants(
          render_state->command_buffer,
          render_state->pipeline.pipeline_layout,
          VK_SHADER_STAGE_VERTEX_BIT,
          0,
          sizeof(GeyserPushConstants),
          &push_constants
        );

        vkCmdDraw(render_state->command_buffer, renderables[i]->vertices_count, 1, 0, 0);

        last_renderable = renderables[i];
      }
    }

    /* Render text */

    vkCmdBindPipeline(
      render_state->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_state->text_pipeline.pipeline
    );

    const GeyserPushConstants text_push_constants = { .camera = render_state->camera_transform };

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
      sizeof(GeyserTextPushConstants),
      &text_push_constants
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
  free(renderables);
  geyser_destroy_vk(render_state);
  render_state_destroy(render_state);
  glfwTerminate();

  return 0;
}
