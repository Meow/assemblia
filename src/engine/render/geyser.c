#include "geyser.h"

#include <string.h>

/* The vendored glad Vulkan header predates the portability extensions that
 * MoltenVK requires, so define the names/flag values ourselves if missing.
 * These are stable, spec-defined constants. */
#ifndef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
#endif
#ifndef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
#define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif
#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

/* This pretty much remains the same all the time */
static const VkSemaphoreCreateInfo semaphore_create_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                             .pNext = NULL,
                                                             .flags = 0 };

static const VkClearColorValue clear_color_value = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } };

static GLADapiproc glad_vulkan_load_func(void *user, const char *name) {
  return glfwGetInstanceProcAddress((VkInstance)user, name);
}

static GLADapiproc glad_vulkan_load_func_vk(void *user, const char *name) {
  return vkGetInstanceProcAddr((VkInstance)user, name);
}

static inline const char *vk_result_name(const VkResult res) {
  switch (res) {
  case VK_SUCCESS: return "Success"; break;
  case VK_NOT_READY: return "Not ready"; break;
  case VK_TIMEOUT: return "Timeout"; break;
  case VK_EVENT_SET: return "Event set"; break;
  case VK_EVENT_RESET: return "Event reset"; break;
  case VK_INCOMPLETE: return "Incomplete"; break;
  case VK_ERROR_OUT_OF_HOST_MEMORY: return "Out of host memory"; break;
  case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "Out of device memory"; break;
  case VK_ERROR_INITIALIZATION_FAILED: return "Initialization failed"; break;
  case VK_ERROR_DEVICE_LOST: return "Device lost"; break;
  case VK_ERROR_MEMORY_MAP_FAILED: return "Memory map failed"; break;
  case VK_ERROR_LAYER_NOT_PRESENT: return "Layer not present"; break;
  case VK_ERROR_EXTENSION_NOT_PRESENT: return "Extension not present"; break;
  case VK_ERROR_FEATURE_NOT_PRESENT: return "Feature not present"; break;
  case VK_ERROR_INCOMPATIBLE_DRIVER: return "Incompatible driver"; break;
  case VK_ERROR_TOO_MANY_OBJECTS: return "Too many objects"; break;
  case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Format not supported"; break;
  case VK_ERROR_FRAGMENTED_POOL: return "Fragmented pool"; break;
  // Provided by VK_VERSION_1_1
  case VK_ERROR_OUT_OF_POOL_MEMORY: return "Out of pool memory"; break;
  // Provided by VK_VERSION_1_1
  case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "Invalid external handle"; break;
  // Provided by VK_KHR_surface
  case VK_ERROR_SURFACE_LOST_KHR: return "Surface lost"; break;
  // Provided by VK_KHR_surface
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "Native window in use"; break;
  // Provided by VK_KHR_swapchain
  case VK_SUBOPTIMAL_KHR: return "Suboptimal"; break;
  // Provided by VK_KHR_swapchain
  case VK_ERROR_OUT_OF_DATE_KHR: return "Out of date"; break;
  // Provided by VK_KHR_display_swapchain
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "Incompatible display"; break;
  // Provided by VK_EXT_debug_report
  case VK_ERROR_VALIDATION_FAILED_EXT: return "Validation failed"; break;
  default: return "Unknown"; break;
  }
}

VkBool32 debug_callback(
  VkDebugReportFlagsEXT _flags,
  VkDebugReportObjectTypeEXT _object_type,
  u64 _object,
  size_t _location,
  i32 _message_code,
  const char *_layer_prefix,
  const char *message,
  void *_userdata
) {
  printf("\033[1;32m[Vulkan Debug]\033[0m %s\n", message);
  return VK_FALSE;
}

void geyser_success_or_message(const VkResult res, const char *message) {
  if (res != VK_SUCCESS) {
    printf("\033[1;31m[Geyser Error]\033[0m %s (%s)\n", message, vk_result_name(res));
    abort();
  }
}

void geyser_init_vk(RenderState *restrict state) {
  u32 ext_count              = 0;
  u32 validation_layer_count = 0;

  /* Load functions necessary for instance creation */
  gladLoadVulkanUserPtr(NULL, glad_vulkan_load_func, NULL);

  const char **extensions = glfwGetRequiredInstanceExtensions(&ext_count);
  const char *ext_names[16];
  const char *validation_layer_names[1];

  u32 ext_names_count = 0;
  for (u32 i = 0; i < ext_count; i++)
    ext_names[ext_names_count++] = extensions[i];

  VkInstanceCreateFlags instance_flags = 0;

#ifdef __APPLE__
  /* MoltenVK is a non-conformant (portability) driver, so it is only reported
   * by the loader when portability enumeration is explicitly requested. */
  ext_names[ext_names_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
  instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  if (state->debug == 1) {
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    if (layer_count > 0) {
      VkLayerProperties *layer_properties = (VkLayerProperties *)malloc(sizeof(VkLayerProperties) * layer_count);
      vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);

      for (u32 i = 0; i < layer_count; i++) {
        if (strcmp(layer_properties[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
          validation_layer_names[0] = "VK_LAYER_KHRONOS_validation";
          validation_layer_count    = 1;
          break;
        }
      }

      free(layer_properties);
    } else {
      printf("Validation layers unavailable\n");
      validation_layer_count = 0;
    }

    ext_names[ext_names_count++] = "VK_EXT_debug_report";
  }

  ext_count = ext_names_count;

  if (state->debug == 1 && vkEnumerateInstanceVersion != NULL) {
    u32 instance_api_version = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&instance_api_version);

    printf(
      "[Geyser] Loader supports Vulkan %u.%u.%u\n",
      VK_API_VERSION_MAJOR(instance_api_version),
      VK_API_VERSION_MINOR(instance_api_version),
      VK_API_VERSION_PATCH(instance_api_version)
    );
  }

  /* Targeting 1.1: the vendored glad loader is generated for Vulkan 1.1, and
   * its extension detection breaks against instances created with apiVersion
   * >= 1.2 (the loader stops resolving global commands through
   * vkGetInstanceProcAddr(instance, ...) for those). Regenerate glad before
   * raising this. */
  const VkApplicationInfo app_info = { GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_APPLICATION_INFO),
                                       .pApplicationName   = "Geyser",
                                       .applicationVersion = 1,
                                       .pEngineName        = "Miniflow",
                                       .engineVersion      = 1,
                                       .apiVersion         = VK_API_VERSION_1_1 };

  const VkInstanceCreateInfo instance_info = { .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                               .pNext                   = NULL,
                                               .flags                   = instance_flags,
                                               .pApplicationInfo        = &app_info,
                                               .enabledLayerCount       = validation_layer_count,
                                               .ppEnabledLayerNames     = validation_layer_names,
                                               .enabledExtensionCount   = ext_count,
                                               .ppEnabledExtensionNames = ext_names };

  VkResult res = vkCreateInstance(&instance_info, NULL, &state->instance);

  if (res != VK_SUCCESS) {
    printf("[Geyser Error] Failed to create vulkan instance ");

    switch (res) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: printf("(host out of memory)\n"); break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: printf("(device out of memory)\n"); break;
    case VK_ERROR_INITIALIZATION_FAILED: printf("(initialization failed)\n"); break;
    case VK_ERROR_LAYER_NOT_PRESENT: printf("(layer not present)\n"); break;
    case VK_ERROR_EXTENSION_NOT_PRESENT: printf("(extension not present)\n"); break;
    case VK_ERROR_INCOMPATIBLE_DRIVER: printf("(driver is incompatible)\n"); break;
    default: printf("\n");
    }

    abort();
  }

  /* Now that we have an instance, load all other functions */
  if (!gladLoadVulkanUserPtr(NULL, glad_vulkan_load_func_vk, state->instance)) {
    printf("[Geyser Error] Failed to load instance-level Vulkan functions!\n");
    abort();
  }

  if (state->debug == 1) {
    const VkDebugReportCallbackCreateInfoEXT debug_callback_info = {
      GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT),
      .flags       = 0xf,
      .pfnCallback = debug_callback,
      .pUserData   = NULL
    };

    VkDebugReportCallbackEXT debug_report_callback;

    geyser_success_or_message(
      vkCreateDebugReportCallbackEXT(state->instance, &debug_callback_info, NULL, &debug_report_callback),
      "Failed to create a debug callback!"
    );
  }

  u32 device_count = 0;
  vkEnumeratePhysicalDevices(state->instance, &device_count, NULL);

  if (device_count <= 0) {
    printf("[Geyser Error] No physical graphics devices detected!\n");
    abort();
  }

  VkPhysicalDevice *physical_devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * device_count);
  vkEnumeratePhysicalDevices(state->instance, &device_count, physical_devices);

  /* Simply pick the first discrete GPU as our "ideal" GPU. */
  for (u32 i = 0; i < device_count; i++) {
    vkGetPhysicalDeviceProperties(physical_devices[i], &state->physical_device_properties);

    state->physical_device = physical_devices[i];

    if (state->physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      if (state->debug == 1) {
        printf("[Geyser] Using a discrete GPU\n");
        printf(
          "API Version: %u.%u.%u\nDriver Version: %u\nVendor ID: %u\nDevice: %s "
          "(ID: %u)\n",
          VK_API_VERSION_MAJOR(state->physical_device_properties.apiVersion),
          VK_API_VERSION_MINOR(state->physical_device_properties.apiVersion),
          VK_API_VERSION_PATCH(state->physical_device_properties.apiVersion),
          state->physical_device_properties.driverVersion,
          state->physical_device_properties.vendorID,
          state->physical_device_properties.deviceName,
          state->physical_device_properties.deviceID
        );
      }

      break;
    }
  }

  free(physical_devices);

  vkGetPhysicalDeviceFeatures(state->physical_device, &state->physical_device_features);

  state->physical_device_features.sampleRateShading = VK_TRUE;

  if (!gladLoadVulkanUserPtr(state->physical_device, glad_vulkan_load_func_vk, state->instance)) {
    printf("[Geyser Error] Failed to load device-level Vulkan functions!\n");
    abort();
  }

  u32 queue_properties_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device, &queue_properties_count, NULL);

  if (queue_properties_count == 0) {
    printf("[Geyser Error] Cannot fetch physical device queue properties\n");
    abort();
  }

  VkQueueFamilyProperties *queue_properties =
    (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * queue_properties_count);
  vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device, &queue_properties_count, queue_properties);

  for (state->queue_family_index = 0; state->queue_family_index < queue_properties_count; state->queue_family_index++)
    if (
      queue_properties[state->queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
      glfwGetPhysicalDevicePresentationSupport(state->instance, state->physical_device, state->queue_family_index)
    )
      break;

  free(queue_properties);

  u32 family_indices[]              = { state->queue_family_index };
  state->queue_family_indices       = family_indices;
  state->queue_family_indices_count = 1;

  const f32 queue_prios[] = { 1.0f, 1.0f };

  const VkDeviceQueueCreateInfo queue_create_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO),
    .queueFamilyIndex = state->queue_family_index,
    .queueCount       = 1,
    .pQueuePriorities = queue_prios
  };

  const char *device_ext_names[8];
  u32 device_ext_count                 = 0;
  device_ext_names[device_ext_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  device_ext_names[device_ext_count++] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;

  /* If the device exposes VK_KHR_portability_subset (e.g. MoltenVK), the Vulkan
   * spec requires that we enable it. */
  u32 available_device_ext_count = 0;
  vkEnumerateDeviceExtensionProperties(state->physical_device, NULL, &available_device_ext_count, NULL);

  if (available_device_ext_count > 0) {
    VkExtensionProperties *available_device_exts =
      (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * available_device_ext_count);
    vkEnumerateDeviceExtensionProperties(
      state->physical_device, NULL, &available_device_ext_count, available_device_exts
    );

    for (u32 i = 0; i < available_device_ext_count; i++) {
      if (strcmp(available_device_exts[i].extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
        device_ext_names[device_ext_count++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
        break;
      }
    }

    free(available_device_exts);
  }

  const VkDeviceCreateInfo device_create_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO),
                                                  .queueCreateInfoCount    = 1,
                                                  .pQueueCreateInfos       = &queue_create_info,
                                                  .enabledLayerCount       = validation_layer_count,
                                                  .ppEnabledLayerNames     = validation_layer_names,
                                                  .enabledExtensionCount   = device_ext_count,
                                                  .ppEnabledExtensionNames = device_ext_names,
                                                  .pEnabledFeatures        = &state->physical_device_features };

  res = vkCreateDevice(state->physical_device, &device_create_info, NULL, &state->device);

  if (res != VK_SUCCESS) {
    printf("[Geyser Error] Failed to create device! ");

    switch (res) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: printf("(Host out of memory)\n"); break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: printf("(Out of device memory)\n"); break;
    case VK_ERROR_INITIALIZATION_FAILED: printf("(Initialization failed)\n"); break;
    case VK_ERROR_EXTENSION_NOT_PRESENT: printf("(Extension not found)\n"); break;
    case VK_ERROR_FEATURE_NOT_PRESENT: printf("(Feature not found)\n"); break;
    case VK_ERROR_TOO_MANY_OBJECTS: printf("(Too many objects)\n"); break;
    case VK_ERROR_DEVICE_LOST: printf("(Device lost)\n"); break;
    default: printf("\n");
    }

    abort();
  }

  vkGetPhysicalDeviceMemoryProperties(state->physical_device, &state->physical_device_memory_properties);

  geyser_success_or_message(
    glfwCreateWindowSurface(state->instance, state->window, NULL, &state->surface),
    "Window surface initialization failed!"
  );

  VkSurfaceCapabilitiesKHR surface_capabilities;

  if (
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physical_device, state->surface, &surface_capabilities) !=
    VK_SUCCESS
  ) {
    printf("[Geyser Error] Failed to get physical device surface capabilities!\n");
    abort();
  } else if (state->debug == 1) {
    printf(
      "[Geyser] min %ux%u, max %ux%u, min image count is %u, max image "
      "count is %u\n",
      surface_capabilities.minImageExtent.width,
      surface_capabilities.minImageExtent.height,
      surface_capabilities.maxImageExtent.width,
      surface_capabilities.maxImageExtent.height,
      surface_capabilities.minImageCount,
      surface_capabilities.maxImageCount
    );
  }

  VkBool32 device_surface_supported = VK_FALSE;

  geyser_success_or_message(
    vkGetPhysicalDeviceSurfaceSupportKHR(
      state->physical_device, state->queue_family_index, state->surface, &device_surface_supported
    ),
    "Unable to determine device surface support!"
  );

  if (device_surface_supported != VK_TRUE) {
    printf("[Geyser Error] Device does not support the required surface type!\n");
    abort();
  }

  u32 device_present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(state->physical_device, state->surface, &device_present_mode_count, NULL);

  if (device_present_mode_count < 1) {
    printf("[Geyser Error] Device does not have any present modes!\n");
    abort();
  }

  VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  VkPresentModeKHR *device_present_modes =
    (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * device_present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    state->physical_device, state->surface, &device_present_mode_count, device_present_modes
  );

  for (u32 i = 0; i < device_present_mode_count; i++) {
    if (device_present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      preferred_present_mode = device_present_modes[i];
      break;
    }

    if (preferred_present_mode != VK_PRESENT_MODE_MAILBOX_KHR)
      preferred_present_mode = device_present_modes[i];
  }

  free(device_present_modes);

  u32 format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(state->physical_device, state->surface, &format_count, NULL);

  if (format_count < 1) {
    printf("[Geyser Error] Physical device surface does not have any formats!\n");
    abort();
  }

  VkSurfaceFormatKHR *surface_formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(state->physical_device, state->surface, &format_count, surface_formats);

  state->preferred_color_format         = surface_formats[0].format;
  VkColorSpaceKHR preferred_color_space = surface_formats[0].colorSpace;

  if (surface_formats[0].format != VK_FORMAT_UNDEFINED) {
    for (u32 i = 0; i < format_count; i++) {
      if (surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
        state->preferred_color_format = surface_formats[i].format;
        preferred_color_space         = surface_formats[i].colorSpace;
        break;
      }
    }
  }

  free(surface_formats);

  switch (state->preferred_color_format) {
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB: break;
  default:
    printf(
      "[Geyser Error] Surface color format doesn't appear to be 8-bit "
      "BGRA!\n"
    );
    abort();
  }

  const VkExtent2D ext = { .width = (u32)state->window_width, .height = (u32)state->window_height };

  VkSwapchainCreateInfoKHR swapchain_create_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR),
    .surface               = state->surface,
    .minImageCount         = surface_capabilities.minImageCount + 1,
    .imageFormat           = state->preferred_color_format,
    .imageColorSpace       = preferred_color_space,
    .imageExtent           = ext,
    .imageArrayLayers      = 1,
    .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = state->queue_family_indices_count,
    .pQueueFamilyIndices   = state->queue_family_indices,
    .preTransform          = surface_capabilities.currentTransform,
    .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode           = preferred_present_mode,
    .clipped               = VK_TRUE,
    .oldSwapchain          = VK_NULL_HANDLE
  };

  if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  geyser_success_or_message(
    vkCreateSwapchainKHR(state->device, &swapchain_create_info, NULL, &state->swapchain), "Failed to create swapchain!"
  );

  vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchain_image_count, NULL);

  if (state->swapchain_image_count < 1) {
    printf("[Geyser Error] Swapchain has no images!\n");
    abort();
  }

  VkImage *swapchain_images = (VkImage *)malloc(sizeof(VkImage) * state->swapchain_image_count);
  vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchain_image_count, swapchain_images);
  state->swapchain_images = swapchain_images;

  geyser_create_semaphore(state, &state->image_semaphore);

  vkAcquireNextImageKHR(
    state->device, state->swapchain, UINT64_MAX, state->image_semaphore, NULL, &state->current_swapchain_image
  );

  vkDestroySemaphore(state->device, state->image_semaphore, NULL);

  vkGetDeviceQueue(state->device, state->queue_family_index, 0, &state->queue);

  const VkAttachmentDescription attachment_description[1] = {
    { .flags          = 0,
      .format         = state->preferred_color_format,
      .samples        = VK_SAMPLE_COUNT_1_BIT, /* TODO: query VkPhysicalDeviceLimits */
      .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
      .finalLayout    = VK_IMAGE_LAYOUT_GENERAL }
  };

  const VkAttachmentReference attachment_ref[1] = { { .attachment = 0, .layout = VK_IMAGE_LAYOUT_GENERAL } };

  const VkSubpassDescription subpass_description[1] = { { .flags                   = 0,
                                                          .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                          .inputAttachmentCount    = 1,
                                                          .pInputAttachments       = attachment_ref,
                                                          .colorAttachmentCount    = 1,
                                                          .pColorAttachments       = attachment_ref,
                                                          .pResolveAttachments     = NULL,
                                                          .pDepthStencilAttachment = NULL,
                                                          .preserveAttachmentCount = 0,
                                                          .pPreserveAttachments    = NULL } };

  const VkRenderPassCreateInfo renderpass_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO),
    .attachmentCount = 1,
    .pAttachments    = attachment_description,
    .subpassCount    = 1,
    .pSubpasses      = subpass_description
  };

  if (vkCreateRenderPass(state->device, &renderpass_info, NULL, &state->renderpass) != VK_SUCCESS) {
    printf("[Geyser Error] Failed to create a render pass!\n");
    abort();
  }

  const VkBufferCreateInfo general_buffer_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO),
                                                   .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   .size                  = util_mebibytes(256), /* BAR size */
                                                   .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                                   .queueFamilyIndexCount = state->queue_family_indices_count,
                                                   .pQueueFamilyIndices   = state->queue_family_indices };

  vkCreateBuffer(state->device, &general_buffer_info, NULL, &state->buffer);

  const VkMemoryAllocateInfo general_memory_allocation_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO),
    .allocationSize = util_mebibytes(256), /* BAR size */
    .memoryTypeIndex =
      geyser_get_memory_type_index(state, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
  };

  vkAllocateMemory(state->device, &general_memory_allocation_info, NULL, &state->memory);
  vkBindBufferMemory(state->device, state->buffer, state->memory, 0);

  VkCommandPoolCreateInfo command_pool_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO),
    .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = state->queue_family_index
  };

  vkCreateCommandPool(state->device, &command_pool_info, NULL, &state->command_pool);

  VkCommandBufferAllocateInfo command_buffer_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO),
    .commandPool        = state->command_pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
  };

  vkAllocateCommandBuffers(state->device, &command_buffer_info, &state->command_buffer);

  VkImageMemoryBarrier pre_draw_barrier = { .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .pNext               = NULL,
                                            .srcAccessMask       = 0,
                                            .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                                            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

  VkImageMemoryBarrier pre_present_barrier = { .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                               .pNext               = NULL,
                                               .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                               .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
                                               .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                               .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                               .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                               .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

  state->pre_draw_barrier    = pre_draw_barrier;
  state->pre_present_barrier = pre_present_barrier;

  memset(&state->viewport, 0, sizeof(state->viewport));
  state->viewport.height   = state->window_height;
  state->viewport.width    = state->window_width;
  state->viewport.minDepth = 0.0f;
  state->viewport.maxDepth = 1.0f;

  memset(&state->scissor, 0, sizeof(state->scissor));
  state->scissor.extent.width  = (u32)state->window_width;
  state->scissor.extent.height = (u32)state->window_height;
  state->scissor.offset.x      = 0;
  state->scissor.offset.y      = 0;

  const VkDescriptorPoolSize descriptor_pool_size[] = {
    { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = GEYSER_MAX_TEXTURES },
    { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = GEYSER_MAX_GLYPHS }
  };

  const VkDescriptorPoolCreateInfo descriptor_pool_info = {
    GEYSER_MINIMAL_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO),
    .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets       = GEYSER_MAX_TEXTURES,
    .poolSizeCount = 2,
    .pPoolSizes    = descriptor_pool_size
  };

  vkCreateDescriptorPool(state->device, &descriptor_pool_info, NULL, &state->descriptor_pool);
}

void geyser_destroy_vk(RenderState *state) {
  vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
  vkDestroySurfaceKHR(state->instance, state->surface, NULL);
}

void geyser_fill_image_view_creation_structs(
  RenderState *state,
  VkImageSubresourceRange *resource_range,
  VkComponentMapping *mapping,
  VkImageViewCreateInfo *creation_info
) {
  resource_range->aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  resource_range->baseMipLevel   = 0;
  resource_range->levelCount     = 1;
  resource_range->baseArrayLayer = 0;
  resource_range->layerCount     = 1;

  mapping->r = VK_COMPONENT_SWIZZLE_R;
  mapping->g = VK_COMPONENT_SWIZZLE_G;
  mapping->b = VK_COMPONENT_SWIZZLE_B;
  mapping->a = VK_COMPONENT_SWIZZLE_A;

  creation_info->sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  creation_info->pNext            = NULL;
  creation_info->flags            = 0;
  creation_info->viewType         = VK_IMAGE_VIEW_TYPE_2D;
  creation_info->format           = state->preferred_color_format;
  creation_info->components       = *mapping;
  creation_info->subresourceRange = *resource_range;
}

void geyser_create_image_view(
  RenderState *state,
  const Vector2 size,
  const VkImageViewType type,
  const VkImageUsageFlags usage,
  const VkSampleCountFlags samples,
  MemoryManager *mm,
  const u64 crc,
  GeyserImageView *gs_image_view
) {
  VkImageSubresourceRange resource_range;
  VkComponentMapping mapping;
  VkImageViewCreateInfo image_view_creation_info;

  const VkImageCreateInfo image_creation_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO),
                                                  .imageType = VK_IMAGE_TYPE_2D,
                                                  .format    = state->preferred_color_format,
                                                  .extent = { .width = (u32)size.x, .height = (u32)size.y, .depth = 1 },
                                                  .mipLevels             = 1,
                                                  .arrayLayers           = 1,
                                                  .samples               = samples,
                                                  .tiling                = VK_IMAGE_TILING_OPTIMAL,
                                                  .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                           VK_IMAGE_USAGE_SAMPLED_BIT | usage,
                                                  .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                                  .queueFamilyIndexCount = state->queue_family_indices_count,
                                                  .pQueueFamilyIndices   = state->queue_family_indices,
                                                  .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED };

  geyser_success_or_message(
    vkCreateImage(state->device, &image_creation_info, NULL, &gs_image_view->base.image), "Failed to create image!"
  );

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(state->device, gs_image_view->base.image, &memory_requirements);

  FreeImageMemoryBlock mblock;

  memory_find_free_image_block(state, mm, memory_requirements.alignment, crc, memory_requirements.size, &mblock);

  gs_image_view->base.pool     = mblock.pool;
  gs_image_view->base.offset   = mblock.offset;
  gs_image_view->base.size     = memory_requirements.size;
  gs_image_view->base.newblock = mblock.newblock;

  geyser_success_or_message(
    vkBindImageMemory(
      state->device, gs_image_view->base.image, gs_image_view->base.pool->memory, gs_image_view->base.offset
    ),
    "Failed to bind image memory!"
  );

  geyser_fill_image_view_creation_structs(state, &resource_range, &mapping, &image_view_creation_info);

  image_view_creation_info.image    = gs_image_view->base.image;
  image_view_creation_info.viewType = type;

  if (vkCreateImageView(state->device, &image_view_creation_info, NULL, &gs_image_view->view) != VK_SUCCESS) {
    printf("[Geyser Error] Unable to create image view!\n");
    abort();
  }
}

u32 geyser_get_memory_type_index_filtered(
  const RenderState *restrict state, const VkMemoryPropertyFlags flags, const u32 type_bits
) {
  for (u32 i = 0; i < state->physical_device_memory_properties.memoryTypeCount; i++) {
    if ((type_bits & (1U << i)) == 0)
      continue;

    if ((state->physical_device_memory_properties.memoryTypes[i].propertyFlags & flags) == flags)
      return i;
  }

  printf("[Geyser Warning] No memory type with properties 0x%x (allowed mask 0x%x)!\n", flags, type_bits);

  return 0;
}

u32 geyser_get_memory_type_index(const RenderState *restrict state, const VkMemoryPropertyFlagBits flag) {
  return geyser_get_memory_type_index_filtered(state, (VkMemoryPropertyFlags)flag, ~0U);
}

void geyser_create_image(
  const RenderState *restrict state,
  const Vector2 size,
  const VkImageTiling tiling,
  const VkFormat format,
  const VkImageUsageFlags usage,
  GeyserImage *gi
) {
  const VkImageCreateInfo image_creation_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO),
                                                  .imageType = VK_IMAGE_TYPE_2D,
                                                  .format    = format,
                                                  .extent = { .width = (u32)size.x, .height = (u32)size.y, .depth = 1 },
                                                  .mipLevels             = 1,
                                                  .arrayLayers           = 1,
                                                  .samples               = VK_SAMPLE_COUNT_1_BIT,
                                                  .tiling                = tiling,
                                                  .usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | usage,
                                                  .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                                  .queueFamilyIndexCount = state->queue_family_indices_count,
                                                  .pQueueFamilyIndices   = state->queue_family_indices,
                                                  .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED };

  geyser_success_or_message(
    vkCreateImage(state->device, &image_creation_info, NULL, &gi->image), "Failed to create image!"
  );
}

void geyser_create_descriptor_set_layout_binding(
  const RenderState *restrict state,
  const VkDescriptorSetLayoutBinding descriptor_bindings[],
  const u32 descriptor_bindings_size,
  VkDescriptorSetLayout *layout
) {
  const VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO),
    .bindingCount = descriptor_bindings_size,
    .pBindings    = descriptor_bindings
  };

  vkCreateDescriptorSetLayout(state->device, &descriptor_layout_create_info, NULL, layout);
}

void geyser_create_pipeline(
  const RenderState *restrict state,
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
) {
  pipeline->descriptor_set_layouts       = descriptor_layouts;
  pipeline->descriptor_set_layouts_count = descriptor_layouts_size;

  const VkPipelineLayoutCreateInfo pipeline_layout_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO),
    .setLayoutCount         = descriptor_layouts_size,
    .pSetLayouts            = descriptor_layouts,
    .pushConstantRangeCount = push_constant_ranges_size,
    .pPushConstantRanges    = push_constant_ranges
  };

  vkCreatePipelineLayout(state->device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout);

  const VkShaderModuleCreateInfo vertex_shader_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO),
    .codeSize = vertex_shader_data_size,
    .pCode    = (u32 *)vertex_shader_data
  };

  const VkShaderModuleCreateInfo fragment_shader_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO),
    .codeSize = fragment_shader_data_size,
    .pCode    = (u32 *)fragment_shader_data
  };

  vkCreateShaderModule(state->device, &vertex_shader_info, NULL, &pipeline->vertex_shader);
  vkCreateShaderModule(state->device, &fragment_shader_info, NULL, &pipeline->fragment_shader);

  const VkPipelineShaderStageCreateInfo pipeline_shader_stages[] = {
    { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO),
      .stage               = VK_SHADER_STAGE_VERTEX_BIT,
      .module              = pipeline->vertex_shader,
      .pName               = "main",
      .pSpecializationInfo = NULL },
    { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO),
      .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module              = pipeline->fragment_shader,
      .pName               = "main",
      .pSpecializationInfo = NULL },
  };

  const VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO),
    .vertexBindingDescriptionCount   = vertex_input_description->input_binding_description_size,
    .pVertexBindingDescriptions      = vertex_input_description->input_binding_descriptions,
    .vertexAttributeDescriptionCount = vertex_input_description->input_attribute_description_size,
    .pVertexAttributeDescriptions    = vertex_input_description->input_attribute_descriptions
  };

  const VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO),
    .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };

  const VkPipelineRasterizationStateCreateInfo raster_state_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO),
    .depthClampEnable        = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode             = VK_POLYGON_MODE_FILL,
    .cullMode                = VK_CULL_MODE_BACK_BIT,
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable         = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp          = 0.0f,
    .depthBiasSlopeFactor    = 0.0f,
    .lineWidth               = 1.0f
  };

  const VkPipelineMultisampleStateCreateInfo multisample_state_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO),
    .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable   = VK_FALSE,
    .minSampleShading      = 1.0f,
    .pSampleMask           = NULL,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable      = VK_FALSE
  };

  const VkPipelineDepthStencilStateCreateInfo stencil_state_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO),
    .depthTestEnable       = VK_FALSE,
    .depthWriteEnable      = VK_FALSE,
    .depthCompareOp        = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable     = VK_FALSE
  };

  const VkPipelineViewportStateCreateInfo viewport_state_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO),
    .viewportCount = 1,
    .pViewports    = NULL,
    .scissorCount  = 1,
    .pScissors     = NULL
  };

  VkPipelineColorBlendAttachmentState color_attachment_states[] = {
    { .blendEnable         = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp        = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp        = VK_BLEND_OP_ADD,
      .colorWriteMask      = 0xF }
  };

  const VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO),
    .logicOpEnable   = VK_FALSE,
    .logicOp         = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments    = color_attachment_states,
    .blendConstants  = { 1.0f, 1.0f, 1.0f, 1.0f }
  };

  const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

  const VkPipelineDynamicStateCreateInfo dynamic_states_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO),
    .dynamicStateCount = 2,
    .pDynamicStates    = dynamic_states
  };

  const VkGraphicsPipelineCreateInfo pipeline_info[] = {
    { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO),
      .layout              = pipeline->pipeline_layout,
      .renderPass          = state->renderpass,
      .stageCount          = 2,
      .pStages             = pipeline_shader_stages,
      .pVertexInputState   = &vertex_input_info,
      .pInputAssemblyState = &input_assembly_info,
      .pTessellationState  = NULL,
      .pViewportState      = &viewport_state_info,
      .pRasterizationState = &raster_state_info,
      .pMultisampleState   = &multisample_state_info,
      .pDepthStencilState  = &stencil_state_info,
      .pColorBlendState    = &color_blend_state_info,
      .pDynamicState       = &dynamic_states_info,
      .subpass             = 0,
      .basePipelineHandle  = NULL,
      .basePipelineIndex   = 0 }
  };

  vkCreateGraphicsPipelines(state->device, NULL, 1, pipeline_info, NULL, &pipeline->pipeline);
}

GeyserVertexInputDescription geyser_create_vertex_input_description() {
  GeyserVertexInputDescription description;

  description.input_binding_description_size   = 0U;
  description.input_attribute_description_size = 0U;

  return description;
}

void geyser_add_vertex_input_binding(
  GeyserVertexInputDescription *description, const u32 binding, const u32 stride, const VkVertexInputRate input_rate
) {
  const VkVertexInputBindingDescription binding_description = { binding, stride, input_rate };
  description->input_binding_descriptions[description->input_binding_description_size++] = binding_description;
};

void geyser_add_vertex_input_attribute(
  GeyserVertexInputDescription *description, const u32 location, const u32 binding, VkFormat format, const u32 offset
) {
  const VkVertexInputAttributeDescription attribute_description = { location, binding, format, offset };
  description->input_attribute_descriptions[description->input_attribute_description_size++] = attribute_description;
}

void geyser_create_semaphore(const RenderState *restrict state, VkSemaphore *semaphore) {
  geyser_success_or_message(
    vkCreateSemaphore(state->device, &semaphore_create_info, NULL, semaphore), "Failed to create a semaphore!"
  );
}

void geyser_cmd_begin_draw(RenderState *restrict state) {
  geyser_create_semaphore(state, &state->image_semaphore);
  geyser_create_semaphore(state, &state->draw_semaphore);

  const VkResult res = vkAcquireNextImageKHR(
    state->device, state->swapchain, UINT64_MAX, state->image_semaphore, NULL, &state->current_swapchain_image
  );

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    if (state->debug)
      printf("[Geyser Debug] Target out of date, rebuilding...\n");

    return;
  } else if (res != VK_SUBOPTIMAL_KHR && res != 0) {
    printf("[Geyser Error] Failed to acquire new swapchain image!\n");
    abort();
  }

  const VkCommandBufferBeginInfo cmd_begin_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO), .pInheritanceInfo = NULL
  };

  vkBeginCommandBuffer(state->command_buffer, &cmd_begin_info);

  const VkImageMemoryBarrier backbuffer_barrier = { .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                    .pNext               = NULL,
                                                    .srcAccessMask       = 0,
                                                    .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                    .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                                                    .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
                                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                    .image               = state->backbuffer.image,
                                                    .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &backbuffer_barrier
  );

  const VkImageSubresourceRange res_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1
  };

  vkCmdClearColorImage(
    state->command_buffer, state->backbuffer.image, VK_IMAGE_LAYOUT_GENERAL, &clear_color_value, 1, &res_range
  );

  state->pre_draw_barrier.image = state->swapchain_images[state->current_swapchain_image];

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &state->pre_draw_barrier
  );
}

void geyser_cmd_end_draw(RenderState *restrict state) {
  const VkImageMemoryBarrier backbuffer_barrier = { .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                    .pNext               = NULL,
                                                    .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                    .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                                                    .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
                                                    .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                    .image               = state->backbuffer.image,
                                                    .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &backbuffer_barrier
  );

  const VkImageCopy copy_data = {
    .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
    .srcOffset      = { 0U, 0U, 0U },
    .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
    .dstOffset      = { 0U, 0U, 0U },
    .extent         = { (u32)state->window_width, (u32)state->window_height, 1U }
  };

  vkCmdCopyImage(
    state->command_buffer,
    state->backbuffer.image,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    state->swapchain_images[state->current_swapchain_image],
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &copy_data
  );

  state->pre_present_barrier.image = state->swapchain_images[state->current_swapchain_image];

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &state->pre_present_barrier
  );

  vkEndCommandBuffer(state->command_buffer);

  const VkFence no_fence = VK_NULL_HANDLE;
  const VkPipelineStageFlags pipe_stage_flags =
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  const VkSubmitInfo submit_info = { .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                     .pNext                = NULL,
                                     .waitSemaphoreCount   = 1,
                                     .pWaitSemaphores      = &state->image_semaphore,
                                     .pWaitDstStageMask    = &pipe_stage_flags,
                                     .commandBufferCount   = 1,
                                     .pCommandBuffers      = &state->command_buffer,
                                     .signalSemaphoreCount = 1,
                                     .pSignalSemaphores    = &state->draw_semaphore };

  geyser_success_or_message(vkQueueSubmit(state->queue, 1, &submit_info, no_fence), "Failed to submit to queue!");

  const VkPresentInfoKHR present_info = {
    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext              = NULL,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &state->draw_semaphore,
    .swapchainCount     = 1,
    .pSwapchains        = &state->swapchain,
    .pImageIndices      = &state->current_swapchain_image,
  };

  const VkResult res = vkQueuePresentKHR(state->queue, &present_info);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    if (state->debug)
      printf("[Geyser Debug] Target out of date, rebuilding...\n");
  } else if (res != VK_SUBOPTIMAL_KHR && res != 0) {
    printf("[Geyser Error] Failed to present queue!\n");
    abort();
  }

  geyser_success_or_message(vkQueueWaitIdle(state->queue), "Failed to wait for queue idle state!");

  vkDestroySemaphore(state->device, state->image_semaphore, NULL);
  vkDestroySemaphore(state->device, state->draw_semaphore, NULL);
}

void geyser_cmd_begin_renderpass(const RenderState *restrict state) {
  const VkClearValue clear_values[2] = {
    [0] = { .color = clear_color_value },
    [1] = { .depthStencil = { 1.0f, 0 } },
  };

  const VkRenderPassBeginInfo render_pass_begin_info = {
    .sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext                    = NULL,
    .renderPass               = state->renderpass,
    .framebuffer              = state->framebuffer,
    .renderArea.offset.x      = 0,
    .renderArea.offset.y      = 0,
    .renderArea.extent.width  = (u32)state->window_width,
    .renderArea.extent.height = (u32)state->window_height,
    .clearValueCount          = 2,
    .pClearValues             = clear_values,
  };

  vkCmdBeginRenderPass(state->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void geyser_cmd_end_renderpass(const RenderState *restrict state) { vkCmdEndRenderPass(state->command_buffer); }

void geyser_cmd_set_viewport(const RenderState *restrict state) {
  vkCmdSetViewport(state->command_buffer, 0, 1, &state->viewport);
  vkCmdSetScissor(state->command_buffer, 0, 1, &state->scissor);
}

void geyser_create_texture(RenderState *restrict state, const u64 crc, const Vector2 size, GeyserTexture *texture) {
  geyser_create_image_view(
    state,
    size,
    VK_IMAGE_VIEW_TYPE_2D,
    0,
    VK_SAMPLE_COUNT_1_BIT,
    (MemoryManager *)state->memory_manager,
    crc,
    &texture->base
  );

  const VkSamplerCreateInfo sampler_info = {
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext                   = NULL,
    .magFilter               = VK_FILTER_NEAREST,
    .minFilter               = VK_FILTER_NEAREST,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias              = 0.0f,
    .anisotropyEnable        = VK_FALSE,
    .maxAnisotropy           = 1.0,
    .compareEnable           = VK_FALSE,
    .compareOp               = VK_COMPARE_OP_NEVER,
    .minLod                  = 0.0f,
    .maxLod                  = 1.0f,
    .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };

  geyser_success_or_message(
    vkCreateSampler(state->device, &sampler_info, NULL, &texture->sampler), "Failed to create a texture sampler!"
  );
}

void geyser_allocate_texture_descriptor_set(
  RenderState *restrict state, GeyserTexture *texture, GeyserPipeline *pipeline
) {
  VkDescriptorSetAllocateInfo descriptor_info = { .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                  .pNext              = NULL,
                                                  .descriptorPool     = state->descriptor_pool,
                                                  .descriptorSetCount = pipeline->descriptor_set_layouts_count,
                                                  .pSetLayouts        = pipeline->descriptor_set_layouts };

  geyser_success_or_message(
    vkAllocateDescriptorSets(state->device, &descriptor_info, &texture->descriptor_set),
    "Failed to allocate a texture descriptor set!"
  );
}

void geyser_free_texture_descriptor_set(RenderState *restrict state, GeyserTexture *texture) {
  geyser_success_or_message(
    vkFreeDescriptorSets(state->device, state->descriptor_pool, 1, &texture->descriptor_set),
    "Failed to free a texture descriptor set!"
  );
}

void geyser_update_texture_descriptor_set(RenderState *restrict state, GeyserTexture *texture) {
  VkDescriptorImageInfo descriptor_info;
  VkWriteDescriptorSet descriptor_write;

  descriptor_info.sampler     = texture->sampler;
  descriptor_info.imageView   = texture->base.view;
  descriptor_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  memset(&descriptor_write, 0, sizeof(descriptor_write));

  descriptor_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet          = texture->descriptor_set;
  descriptor_write.descriptorCount = 1;
  descriptor_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_write.pImageInfo      = &descriptor_info;

  vkUpdateDescriptorSets(state->device, 1, &descriptor_write, 0, NULL);
}

void geyser_set_image_memory(RenderState *restrict state, GeyserImage *image, const Image *image_data) {
  if (state->rendering) {
    printf("[Geyser Error] Cannot set image memory during a render pass!\n");
    abort();
  }

  const u32 size = image_data->width * image_data->height * 4;
  void *data;

  geyser_success_or_message(
    vkMapMemory(state->device, state->memory, 0, size, 0, &data), "Failed to map image memory!"
  );

  for (u32 y = 0; y < image_data->height; y++) {
    u32 *row = (u32 *)((u8 *)data + image_data->width * y * 4);

    for (u32 x = 0; x < image_data->width; x++)
      row[x] = image_data->data[y * image_data->width + x];
  }

  vkUnmapMemory(state->device, state->memory);

  VkImageMemoryBarrier memory_barrier = { .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                          .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                                          .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          .image            = image->image,
                                          .subresourceRange = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                .baseMipLevel   = 0,
                                                                .levelCount     = 1,
                                                                .baseArrayLayer = 0,
                                                                .layerCount     = 1 },
                                          .srcAccessMask    = 0,
                                          .dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT };

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &memory_barrier
  );

  VkBufferImageCopy copy_info = { .bufferOffset      = 0,
                                  .bufferRowLength   = image_data->width,
                                  .bufferImageHeight = image_data->height,
                                  .imageSubresource  = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                         .mipLevel       = 0,
                                                         .baseArrayLayer = 0,
                                                         .layerCount     = 1 },
                                  .imageOffset       = { 0, 0, 0 },
                                  .imageExtent       = { image_data->width, image_data->height, 1 } };

  vkCmdCopyBufferToImage(
    state->command_buffer, state->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info
  );

  memory_barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  memory_barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
  memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &memory_barrier
  );

  geyser_cmd_submit_staging(state);
}

void geyser_set_image_memory_barrier(RenderState *restrict state, GeyserImage *image) {
  if (state->rendering) {
    printf("[Geyser Error] Cannot set image memory during a render pass!\n");
    abort();
  }

  VkImageMemoryBarrier memory_barrier = { .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                          .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                                          .newLayout        = VK_IMAGE_LAYOUT_GENERAL,
                                          .image            = image->image,
                                          .subresourceRange = { .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                .baseMipLevel   = 0,
                                                                .levelCount     = 1,
                                                                .baseArrayLayer = 0,
                                                                .layerCount     = 1 },
                                          .srcAccessMask    = 0,
                                          .dstAccessMask    = VK_ACCESS_SHADER_READ_BIT };

  vkCmdPipelineBarrier(
    state->command_buffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,
    0,
    NULL,
    0,
    NULL,
    1,
    &memory_barrier
  );

  geyser_cmd_submit_staging(state);
}

void geyser_cmd_begin_staging(RenderState *restrict state) {
  const VkCommandBufferBeginInfo cmd_begin_info = {
    GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO), .pInheritanceInfo = NULL
  };

  vkBeginCommandBuffer(state->command_buffer, &cmd_begin_info);
}

void geyser_cmd_end_staging(RenderState *restrict state) {
  vkEndCommandBuffer(state->command_buffer);

  const VkFence no_fence         = VK_NULL_HANDLE;
  const VkSubmitInfo submit_info = { .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                     .pNext                = NULL,
                                     .waitSemaphoreCount   = 0,
                                     .pWaitSemaphores      = NULL,
                                     .pWaitDstStageMask    = NULL,
                                     .commandBufferCount   = 1,
                                     .pCommandBuffers      = &state->command_buffer,
                                     .signalSemaphoreCount = 0,
                                     .pSignalSemaphores    = NULL };

  geyser_success_or_message(vkQueueSubmit(state->queue, 1, &submit_info, no_fence), "Failed to submit to queue!");

  geyser_success_or_message(vkQueueWaitIdle(state->queue), "Failed to wait for queue idle state!");
}

void geyser_cmd_submit_staging(RenderState *restrict state) {
  geyser_cmd_end_staging(state);
  geyser_cmd_begin_staging(state);
}

void geyser_create_backbuffer(RenderState *restrict state) {
  VkImageView fb_attachments[1];

  VkFramebufferCreateInfo framebuffer_info = { GEYSER_BASIC_VK_STRUCT_INFO(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO),
                                               .renderPass      = state->renderpass,
                                               .attachmentCount = 1,
                                               .pAttachments    = fb_attachments,
                                               .width           = (u32)state->window_width,
                                               .height          = (u32)state->window_height,
                                               .layers          = 1 };

  geyser_create_image_view(
    state,
    (Vector2) { state->window_width, state->window_height },
    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
    VK_SAMPLE_COUNT_1_BIT,
    (MemoryManager *)state->memory_manager,
    0,
    (GeyserImageView *)&state->backbuffer
  );

  fb_attachments[0] = state->backbuffer.view;

  geyser_success_or_message(
    vkCreateFramebuffer(state->device, &framebuffer_info, NULL, &state->framebuffer),
    "Failed to create a frame buffer!\n"
  );
}
