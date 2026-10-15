#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "static Vulkan: line %d: %s\n", __LINE__, #x);           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static unsigned injected_calls;
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL injected(VkInstance instance,
                                                         const char *name) {
  (void)instance;
  (void)name;
  ++injected_calls;
  return NULL;
}

int main(void) {
  uint32_t version = 0, count = 0;
  VkExtensionProperties *extensions;
  VkInstance first = VK_NULL_HANDLE, second = VK_NULL_HANDLE;
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                           NULL,
                           "ailo-static-dispatch",
                           0,
                           NULL,
                           0,
                           VK_API_VERSION_1_2};
  VkInstanceCreateInfo info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL};
  const char *missing_layer = "VK_LAYER_ailo_external_forbidden";
  VkDirectDriverLoadingInfoLUNARG external = {
      VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG, NULL, 0, injected};
  VkDirectDriverLoadingListLUNARG list = {
      VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG, NULL,
      VK_DIRECT_DRIVER_LOADING_MODE_EXCLUSIVE_LUNARG, 1, &external};

  CHECK(vkEnumerateInstanceVersion(&version) == VK_SUCCESS);
  CHECK(version >= VK_API_VERSION_1_2);
  CHECK(vkEnumerateInstanceLayerProperties(&count, NULL) == VK_SUCCESS);
  CHECK(count == 0);
  CHECK(vkEnumerateInstanceExtensionProperties(missing_layer, &count, NULL) ==
        VK_ERROR_LAYER_NOT_PRESENT);
  CHECK(vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) ==
        VK_SUCCESS);
  extensions = calloc(count, sizeof(*extensions));
  CHECK(extensions != NULL);
  CHECK(vkEnumerateInstanceExtensionProperties(NULL, &count, extensions) ==
        VK_SUCCESS);
  for (uint32_t i = 0; i < count; ++i)
    CHECK(strcmp(extensions[i].extensionName,
                 VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME) != 0);
  free(extensions);

  info.enabledLayerCount = 1;
  info.ppEnabledLayerNames = &missing_layer;
  CHECK(vkCreateInstance(&info, NULL, &first) == VK_ERROR_LAYER_NOT_PRESENT);
  info.enabledLayerCount = 0;
  info.ppEnabledLayerNames = NULL;
  info.pNext = &list;
  CHECK(vkCreateInstance(&info, NULL, &first) ==
        VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK(injected_calls == 0);
  info.pNext = NULL;

  for (unsigned i = 0; i < 3; ++i) {
    CHECK(vkCreateInstance(&info, NULL, &first) == VK_SUCCESS);
    CHECK(vkCreateInstance(&info, NULL, &second) == VK_SUCCESS);
    CHECK(vkGetInstanceProcAddr(second, "vkEnumeratePhysicalDevices") != NULL);
    CHECK(vkEnumeratePhysicalDevices(first, &count, NULL) == VK_SUCCESS);
    vkDestroyInstance(first, NULL);
    CHECK(vkEnumeratePhysicalDevices(second, &count, NULL) == VK_SUCCESS);
    printf("built-in Vulkan: %u physical devices\n", count);
    if (count) {
      VkPhysicalDevice *physical = calloc(count, sizeof(*physical));
      CHECK(physical != NULL);
      CHECK(vkEnumeratePhysicalDevices(second, &count, physical) == VK_SUCCESS);
      for (uint32_t p = 0; p < count; ++p) {
        uint32_t families = 0, selected;
        float priority = 1.0f;
        VkDevice device;
        VkEvent event;
        VkDeviceQueueCreateInfo queue = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            NULL,
            0,
            0,
            1,
            &priority};
        VkDeviceCreateInfo create = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                     NULL,
                                     0,
                                     1,
                                     &queue,
                                     0,
                                     NULL,
                                     0,
                                     NULL,
                                     NULL};
        VkEventCreateInfo event_info = {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
                                        NULL, 0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical[p], &families, NULL);
        VkQueueFamilyProperties *props = calloc(families, sizeof(*props));
        CHECK(props != NULL);
        vkGetPhysicalDeviceQueueFamilyProperties(physical[p], &families, props);
        for (selected = 0; selected < families; ++selected)
          if (props[selected].queueFlags & VK_QUEUE_COMPUTE_BIT)
            break;
        free(props);
        CHECK(selected < families);
        queue.queueFamilyIndex = selected;
        CHECK(vkCreateDevice(physical[p], &create, NULL, &device) ==
              VK_SUCCESS);
        CHECK(vkGetDeviceProcAddr(device, "vkCreateEvent") != NULL);
        CHECK(vkCreateEvent(device, &event_info, NULL, &event) == VK_SUCCESS);
        vkDestroyEvent(device, event, NULL);
        vkDestroyDevice(device, NULL);
      }
      free(physical);
    }
    vkDestroyInstance(second, NULL);
  }
  return 0;
}
