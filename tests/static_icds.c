#include <vulkan/vulkan_core.h>

#include <stdio.h>
#include <stdlib.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
ailo_radv_icdGetInstanceProcAddr(VkInstance, const char *);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
ailo_anv_icdGetInstanceProcAddr(VkInstance, const char *);

static int probe(const char *name, PFN_vkGetInstanceProcAddr get_proc) {
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                           NULL,
                           "libailo-static-probe",
                           0,
                           NULL,
                           0,
                           VK_API_VERSION_1_3};
  VkInstanceCreateInfo info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL};
  PFN_vkCreateInstance create =
      (PFN_vkCreateInstance)get_proc(VK_NULL_HANDLE, "vkCreateInstance");
  VkInstance instance = VK_NULL_HANDLE;
  PFN_vkDestroyInstance destroy;
  PFN_vkEnumeratePhysicalDevices enumerate;
  PFN_vkGetPhysicalDeviceProperties properties;
  uint32_t count = 0, i;
  VkPhysicalDevice *devices;
  VkResult result;
  if (!create || create(&info, NULL, &instance) != VK_SUCCESS)
    return 1;
  destroy = (PFN_vkDestroyInstance)get_proc(instance, "vkDestroyInstance");
  enumerate = (PFN_vkEnumeratePhysicalDevices)get_proc(
      instance, "vkEnumeratePhysicalDevices");
  properties = (PFN_vkGetPhysicalDeviceProperties)get_proc(
      instance, "vkGetPhysicalDeviceProperties");
  if (!destroy || !enumerate || !properties)
    return 2;
  result = enumerate(instance, &count, NULL);
  if (result != VK_SUCCESS) {
    destroy(instance, NULL);
    return 3;
  }
  printf("%s: %u physical devices\n", name, count);
  devices = count ? (VkPhysicalDevice *)calloc(count, sizeof(*devices)) : NULL;
  if (count && !devices) {
    destroy(instance, NULL);
    return 4;
  }
  if (count && enumerate(instance, &count, devices) != VK_SUCCESS) {
    free(devices);
    destroy(instance, NULL);
    return 5;
  }
  for (i = 0; i < count; ++i) {
    VkPhysicalDeviceProperties props;
    properties(devices[i], &props);
    printf("%s: %s\n", name, props.deviceName);
  }
  free(devices);
  destroy(instance, NULL);
  return 0;
}

int main(void) {
  return probe("RADV", ailo_radv_icdGetInstanceProcAddr) ||
         probe("ANV", ailo_anv_icdGetInstanceProcAddr);
}
