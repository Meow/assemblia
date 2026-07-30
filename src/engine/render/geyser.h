#ifndef __ENGINE_RENDER_GEYSER_H
#define __ENGINE_RENDER_GEYSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define GEYSER_MINIMAL_VK_STRUCT_INFO(t) .sType = t, .pNext = NULL
#define GEYSER_BASIC_VK_STRUCT_INFO(t)   .sType = t, .pNext = NULL, .flags = 0
#define GEYSER_MAX_TEXTURES              16384
#define GEYSER_MAX_GLYPHS                65536

/**
 * Geyser is a minimalistic Vulkan middleware library.
 *
 * It is domain-specific and isn't designed to be used
 * anywhere outside of this project, however it could be
 * used with some adaptation.
 *
 */
#include "../input/asset.h"
#include "../types/matrix.h"
#include "../types/vector.h"
#include "memory.h"
#include "render_state.h"

typedef enum GeyserBool { GS_FALSE = 0, GS_TRUE = 1 } GeyserBool;

typedef struct GeyserImage {
  VkImage image;
  ImageMemoryPool *pool;
  u64 offset;
  u64 size;
  u8 newblock;
} GeyserImage;

typedef struct GeyserImageView {
  GeyserImage base;
  VkImageView view;
} GeyserImageView;

typedef struct GeyserTexture {
  GeyserImageView base;
  VkSampler sampler;
  VkDescriptorSet descriptor_set;
  u8 copy;
} GeyserTexture;

typedef struct GeyserPipeline {
  VkDescriptorSetLayout *descriptor_set_layouts;
  VkPipelineLayout pipeline_layout;
  VkShaderModule vertex_shader;
  VkShaderModule fragment_shader;
  VkPipeline pipeline;
  u32 descriptor_set_layouts_count;
} GeyserPipeline;

typedef struct GeyserVertexInputDescription {
  VkVertexInputBindingDescription input_binding_descriptions[16];
  VkVertexInputAttributeDescription input_attribute_descriptions[16];
  u32 input_binding_description_size;
  u32 input_attribute_description_size;
} GeyserVertexInputDescription;

typedef struct GeyserPushConstants {
  Matrix4 camera;
  Vector4 quaternion;
  Vector4 position;
  Vector4 vertex_color;
  Vector2 scale;
  Vector2 uv_offset;
} GeyserPushConstants;

typedef struct GeyserTextPushConstants {
  Matrix4 camera;
} GeyserTextPushConstants;

/**
 * @brief Initializes Vulkan resources.
 *
 * Populates most Vulkan-related fields in the
 * RenderState struct. It also creates the window surface
 * for rendering.
 *
 * ```
 * geyser_init_vk(render_state);
 * ...
 * // do some rendering here
 * ...
 * geyser_destroy_vk(render_state);
 * ```
 *
 * @warning Will crash the app if Vulkan fails to initialize.
 * @param state Rendering state.
 */
void geyser_init_vk(RenderState *state);

/**
 * @brief Frees Vulkan resources.
 *
 * ```
 * // some rendering here
 * geyser_destroy_vk(render_state);
 * // you can't do vulkan rendering past this point
 * ```
 *
 * @param state Rendering state
 */
void geyser_destroy_vk(RenderState RESTRICTED_PTR state);

/**
 * @brief Ensures result is VK_SUCCESS or aborts with a message.
 *
 * @param res A result of any Vulkan function.
 * @param message A message to display if the result is not VK_SUCCESS.
 */
void geyser_success_or_message(const VkResult res, const char *message);

/**
 * @brief Creates the backbuffer and framebuffer.
 *
 * Dumbly enough this is required to run after the full vulkan initialization,
 * after the memory manager has been initialized.
 *
 * @param state The render state.
 */
void geyser_create_backbuffer(RenderState RESTRICTED_PTR state);

/**
 * @brief Internal function to fill image view structs.
 *
 * @param state The Render state.
 * @param resource_range The resource range struct to fill.
 * @param mapping The color channel mapping struct to fill.
 * @param creation_info The creation info struct to fill.
 */
void geyser_fill_image_view_creation_structs(
  RenderState *state,
  VkImageSubresourceRange *resource_range,
  VkComponentMapping *mapping,
  VkImageViewCreateInfo *creation_info
);

/**
 * @brief Creates a new image view.
 *
 * @param state The render state.
 * @param size Image size.
 * @param type The type of image view as defined by VkImageViewTypeBits.
 * @param usage The usage of the image as defined by VkImageUsageFlagsBits.
 * @param gs_image_view The image view to write to.
 */
void geyser_create_image_view(
  RenderState *state,
  const Vector2 size,
  const VkImageViewType type,
  const VkImageUsageFlags usage,
  const VkSampleCountFlags samples,
  MemoryManager *mm,
  const u64 crc,
  GeyserImageView *gs_image_view
);

/**
 * @brief Creates a new texture (image view, sampler and descriptor set).
 *
 * @param state The render state.
 * @param size Size of the texture.
 * @param texture The texture to write to.
 */
void geyser_create_texture(RenderState RESTRICTED_PTR state, const u64 crc, const Vector2 size, GeyserTexture *texture);

/**
 * @brief Allocates texture's descriptor set.
 *
 * @param state The render state.
 * @param texture The texture, whose descriptor set should be allocated.
 * @param pipeline The pipeline to use.
 */
void geyser_allocate_texture_descriptor_set(
  RenderState RESTRICTED_PTR state, GeyserTexture *texture, GeyserPipeline *pipeline
);

/**
 * @brief Frees texture's descriptor set.
 *
 * @param state The render state.
 * @param texture The texture whose descriptor set needs to be freed.
 */
void geyser_free_texture_descriptor_set(RenderState RESTRICTED_PTR state, GeyserTexture *texture);

/**
 * @brief Updates texture descriptor set (e.g. when texture image is updated).
 *
 * @param state The render state.
 * @param texture The texture, whose descriptor set should be updated.
 */
void geyser_update_texture_descriptor_set(RenderState RESTRICTED_PTR state, GeyserTexture *texture);

/**
 * @brief Gets the memory index of the memory supporting the required properties.
 *
 * @param state The render state.
 * @param flag The required memory properties.
 * @return u32 The index of the memory.
 */
u32 geyser_get_memory_type_index(const RenderState RESTRICTED_PTR state, const VkMemoryPropertyFlagBits flag);

/**
 * @brief Like geyser_get_memory_type_index(), but only considers memory types
 * allowed by @p type_bits (a VkMemoryRequirements::memoryTypeBits mask).
 */
u32 geyser_get_memory_type_index_filtered(
  const RenderState RESTRICTED_PTR state, const VkMemoryPropertyFlags flags, const u32 type_bits
);

/**
 * @brief Creates a new image.
 *
 * @param state The render state.
 * @param size The size of the image in pixels.
 * @param tiling Tiling flags as defined by VkImageTilingBits
 * @param format The desired format of the image.
 * @param usage Intended usage of the image as defined by VkImageUsageFlagsBits
 * @param gi The image structure to write to.
 */
void geyser_create_image(
  const RenderState RESTRICTED_PTR state,
  const Vector2 size,
  const VkImageTiling tiling,
  const VkFormat format,
  const VkImageUsageFlags usage,
  GeyserImage *gi
);

/**
 * @brief Creates a Vulkan rendering pipeline.
 *
 * @param state The render state.
 * @param descriptor_bindings The array of descriptor bindings.
 * @param descriptor_bindings_size The size of the descriptor binding array.
 * @param push_constant_ranges The arrray describing push constants.
 * @param push_constant_ranges_size The size of the push constant description array.
 * @param vertex_shader_data Compiled SPIR-V vertex shader code as array of bytes.
 * @param vertex_shader_data_size The size of the vertex shader code.
 * @param fragment_shader_data Compiled SPIR-V fragment shader code as array of bytes.
 * @param fragment_shader_data_size The size of the fragment shader code.
 * @param vertex_input_description Description of the vertex shader inputs (location=whatever thingies in GLSL).
 * @param pipeline The pipeline struct to write to.
 */
void geyser_create_pipeline(
  const RenderState RESTRICTED_PTR state,
  const VkDescriptorSetLayout descriptor_layouts[],
  const u32 descriptor_layouts_size,
  const VkPushConstantRange push_constant_ranges[],
  const u32 push_constant_ranges_size,
  const u8 vertex_shader_data[],
  const u32 vertex_shader_data_size,
  const u8 fragment_shader_data[],
  const u32 fragment_shader_data_size,
  GeyserVertexInputDescription *vertex_input_description,
  GeyserPipeline *pipeline
);

void geyser_create_descriptor_set_layout_binding(
  const RenderState RESTRICTED_PTR state,
  const VkDescriptorSetLayoutBinding descriptor_bindings[],
  const u32 descriptor_bindings_size,
  VkDescriptorSetLayout *layout
);

/**
 * @brief A helper function to initialize an empty vertex input description.
 *
 * This is used to simplify passing vertex inputs to `geyser_create_pipeline`.
 *
 * @return GeyserVertexInputDescription The empty vertex input description struct.
 */
GeyserVertexInputDescription geyser_create_vertex_input_description();

/**
 * @brief Adds a vertex input binding.
 *
 * @param description The vertex input description to write to.
 * @param binding The binding ID.
 * @param stride The size, in bytes, of the binding.
 * @param input_rate The input rate of the binding. You probably want INPUT_RATE_VERTEX.
 */
void geyser_add_vertex_input_binding(
  GeyserVertexInputDescription *description, const u32 binding, const u32 stride, const VkVertexInputRate input_rate
);

/**
 * @brief Adds a vertex input attribute.
 *
 * @param description The vertex input description to write to.
 * @param location The ID of the location to use (location=X in GLSL).
 * @param binding The binding this attribute belongs to.
 * @param format The data layout format.
 * @param offset The memory offset in bytes if re-using the same binding.
 */
void geyser_add_vertex_input_attribute(
  GeyserVertexInputDescription *description, const u32 location, const u32 binding, VkFormat format, const u32 offset
);

/**
 * @brief Begins a color pass.
 *
 * @param state The render state.
 */
void geyser_cmd_begin_draw(RenderState RESTRICTED_PTR state);

/**
 * @brief Ends the current color pass.
 *
 * Sends commands for execution to the GPU. Once this command is issued,
 * the rendering is no longer in the hands of the CPU, and is entirely on
 * the GPU side.
 *
 * This also rotates the swapchain image and presents the rendered image to
 * the screen.
 *
 * @param state The render state.
 */
void geyser_cmd_end_draw(RenderState RESTRICTED_PTR state);

/**
 * @brief Creates a semaphore.
 *
 * @param state The render state.
 * @param semaphore Semaphore to create.
 */
void geyser_create_semaphore(const RenderState RESTRICTED_PTR state, VkSemaphore *semaphore);

/**
 * @brief Begins a render pass.
 *
 * @param state The render state.
 */
void geyser_cmd_begin_renderpass(const RenderState RESTRICTED_PTR state);

/**
 * @brief Ends the current render pass.
 *
 * @param state The render state.
 */
void geyser_cmd_end_renderpass(const RenderState RESTRICTED_PTR state);

/**
 * @brief Sets the viewport.
 *
 * It uses the values of `state->viewport` and `state->scissor` and
 * simply sets the viewport to those.
 *
 * @param state The render state.
 */
void geyser_cmd_set_viewport(const RenderState RESTRICTED_PTR state);

/**
 * @brief Sets the image memory.
 *
 * This sends the image memory data from the RAM to the GPU VRAM.
 *
 * @param state The render state.
 * @param image The image to set the memory of.
 * @param data The image data to set the memory to.
 */
void geyser_set_image_memory(RenderState RESTRICTED_PTR state, GeyserImage *image, const Image *data);

/**
 * @brief Sets the image type to general.
 *
 * When you re-use the same memory block, you gotta do this.
 *
 * @param state The render state.
 * @param image The image to set the memory barrier of.
 */
void geyser_set_image_memory_barrier(RenderState RESTRICTED_PTR state, GeyserImage *image);

/**
 * @brief Begins a staging pass.
 *
 * This initialized the command buffer in a "headless"
 * configuration, perfect for sending data to the GPU,
 * or doing other things such as allocating new memory
 * and such.
 *
 * Do not draw anything in this pass.
 *
 * @param state The render state.
 */
void geyser_cmd_begin_staging(RenderState RESTRICTED_PTR state);

/**
 * @brief Ends the current staging pass.
 *
 * This submits the staging commands to the GPU and prepares
 * the command buffer to be reconfigured for the color pass /
 * rendering pass.
 *
 * @param state The render state.
 */
void geyser_cmd_end_staging(RenderState RESTRICTED_PTR state);

/**
 * @brief Submits the current command buffer commands to the GPU.
 *
 * Only use while the command buffer is in the staging mode. No, really,
 * I am not responsible for the messed up stuff that can happen if you
 * do this anywhere else.
 *
 * Use this every time after you vkMapMemory and vkUnmapMemory,
 * or copy buffers/images. Otherwise not all of your commands will
 * be executed correctly.
 *
 * @param state The render state.
 */
void geyser_cmd_submit_staging(RenderState RESTRICTED_PTR state);

#ifdef __cplusplus
}
#endif

#endif
