#include "shared-memory.h"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

// Standalone driver test: no GGML scheduler, model or libailo implementation.
extern "C" void ggml_library_driver_log(int, const char *, const char *format,
                                        va_list args) {
  if (std::getenv("RADV_DEBUG") || std::getenv("VK_LOADER_DEBUG"))
    std::vfprintf(stderr, format, args);
}

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error(std::string("line ") +                          \
                               std::to_string(__LINE__) + ": " #x);            \
  } while (0)
template <class T> T structure(VkStructureType type) {
  T value{};
  value.sType = type;
  return value;
}
struct resources {
  VkInstance instance{};
  VkDevice device{};
  VkDeviceMemory memory{};
  VkBuffer buffer{};
  void *mapped{};
  VkDescriptorSetLayout descriptors{};
  VkDescriptorPool descriptor_pool{};
  VkPipelineLayout layout{};
  VkShaderModule shader{};
  VkPipeline pipeline{};
  VkCommandPool commands{};
  VkFence fence{};
  ~resources() {
    if (device) {
      vkDeviceWaitIdle(device);
      vkDestroyFence(device, fence, nullptr);
      vkDestroyCommandPool(device, commands, nullptr);
      vkDestroyPipeline(device, pipeline, nullptr);
      vkDestroyShaderModule(device, shader, nullptr);
      vkDestroyPipelineLayout(device, layout, nullptr);
      vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
      vkDestroyDescriptorSetLayout(device, descriptors, nullptr);
      if (mapped)
        vkUnmapMemory(device, memory);
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
      vkDestroyDevice(device, nullptr);
    }
    if (instance)
      vkDestroyInstance(instance, nullptr);
  }
};

int main() {
  try {
    resources r;
    auto app = structure<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
    app.apiVersion = VK_API_VERSION_1_2;
    auto instance =
        structure<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    instance.pApplicationInfo = &app;
    CHECK(vkCreateInstance(&instance, nullptr, &r.instance) == VK_SUCCESS);
    uint32_t count = 0;
    CHECK(vkEnumeratePhysicalDevices(r.instance, &count, nullptr) ==
          VK_SUCCESS);
    CHECK(count > 0); // Hardware is required; absence is not a successful skip.
    std::vector<VkPhysicalDevice> devices(count);
    CHECK(vkEnumeratePhysicalDevices(r.instance, &count, devices.data()) ==
          VK_SUCCESS);
    VkPhysicalDevice physical = devices[0];
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
    std::printf("GPU=%s vendor=%04x device=%04x driver=%u\n",
                properties.deviceName, properties.vendorID, properties.deviceID,
                properties.driverVersion);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
    uint32_t selected = 0;
    while (selected < count &&
           !(families[selected].queueFlags & VK_QUEUE_COMPUTE_BIT))
      ++selected;
    CHECK(selected < count);
    float priority = 1;
    auto queue_info = structure<VkDeviceQueueCreateInfo>(
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    queue_info.queueFamilyIndex = selected;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    auto create =
        structure<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    create.queueCreateInfoCount = 1;
    create.pQueueCreateInfos = &queue_info;
    CHECK(vkCreateDevice(physical, &create, nullptr, &r.device) == VK_SUCCESS);
    VkQueue queue;
    vkGetDeviceQueue(r.device, selected, 0, &queue);
    constexpr uint32_t groups = 32, maximum = groups * 128, guard = 16;
    auto buffer =
        structure<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    buffer.size = (maximum + guard) * sizeof(uint32_t);
    buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    CHECK(vkCreateBuffer(r.device, &buffer, nullptr, &r.buffer) == VK_SUCCESS);
    VkMemoryRequirements required;
    vkGetBufferMemoryRequirements(r.device, r.buffer, &required);
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    uint32_t memory_type = 0;
    constexpr auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    while (memory_type < memory.memoryTypeCount &&
           (!(required.memoryTypeBits & (1u << memory_type)) ||
            (memory.memoryTypes[memory_type].propertyFlags & flags) != flags))
      ++memory_type;
    CHECK(memory_type < memory.memoryTypeCount);
    auto allocate =
        structure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    allocate.allocationSize = required.size;
    allocate.memoryTypeIndex = memory_type;
    CHECK(vkAllocateMemory(r.device, &allocate, nullptr, &r.memory) ==
          VK_SUCCESS);
    CHECK(vkBindBufferMemory(r.device, r.buffer, r.memory, 0) == VK_SUCCESS);
    CHECK(vkMapMemory(r.device, r.memory, 0, VK_WHOLE_SIZE, 0, &r.mapped) ==
          VK_SUCCESS);
    VkDescriptorSetLayoutBinding binding{};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    auto descriptors = structure<VkDescriptorSetLayoutCreateInfo>(
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    descriptors.bindingCount = 1;
    descriptors.pBindings = &binding;
    CHECK(vkCreateDescriptorSetLayout(r.device, &descriptors, nullptr,
                                      &r.descriptors) == VK_SUCCESS);
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};
    auto layout = structure<VkPipelineLayoutCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &r.descriptors;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    CHECK(vkCreatePipelineLayout(r.device, &layout, nullptr, &r.layout) ==
          VK_SUCCESS);
    auto shader = structure<VkShaderModuleCreateInfo>(
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    shader.codeSize = sizeof(shared_shader);
    shader.pCode = shared_shader;
    CHECK(vkCreateShaderModule(r.device, &shader, nullptr, &r.shader) ==
          VK_SUCCESS);
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    auto pool = structure<VkDescriptorPoolCreateInfo>(
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    pool.maxSets = 1;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &size;
    CHECK(vkCreateDescriptorPool(r.device, &pool, nullptr,
                                 &r.descriptor_pool) == VK_SUCCESS);
    auto set_info = structure<VkDescriptorSetAllocateInfo>(
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    set_info.descriptorPool = r.descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &r.descriptors;
    VkDescriptorSet set;
    CHECK(vkAllocateDescriptorSets(r.device, &set_info, &set) == VK_SUCCESS);
    VkDescriptorBufferInfo buffer_info{r.buffer, 0, buffer.size};
    auto write =
        structure<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
    write.dstSet = set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(r.device, 1, &write, 0, nullptr);
    auto command_pool = structure<VkCommandPoolCreateInfo>(
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    command_pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool.queueFamilyIndex = selected;
    CHECK(vkCreateCommandPool(r.device, &command_pool, nullptr, &r.commands) ==
          VK_SUCCESS);
    auto command_info = structure<VkCommandBufferAllocateInfo>(
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    command_info.commandPool = r.commands;
    command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount = 1;
    VkCommandBuffer command;
    CHECK(vkAllocateCommandBuffers(r.device, &command_info, &command) ==
          VK_SUCCESS);
    auto fence =
        structure<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    CHECK(vkCreateFence(r.device, &fence, nullptr, &r.fence) == VK_SUCCESS);
    unsigned cases = 0;
    for (const uint32_t lanes : {64u, 128u}) {
      for (const uint32_t slots : {128u, 1024u, 4096u, 8192u}) {
        if (slots * 2 * sizeof(uint32_t) >
            properties.limits.maxComputeSharedMemorySize)
          continue;
        const uint32_t constants[] = {slots, lanes};
        const VkSpecializationMapEntry entries[] = {{0, 0, 4}, {1, 4, 4}};
        VkSpecializationInfo specialization{2, entries, sizeof(constants),
                                            constants};
        auto pipeline = structure<VkComputePipelineCreateInfo>(
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
        pipeline.stage = structure<VkPipelineShaderStageCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline.stage.module = r.shader;
        pipeline.stage.pName = "main";
        pipeline.stage.pSpecializationInfo = &specialization;
        pipeline.layout = r.layout;
        CHECK(vkCreateComputePipelines(r.device, VK_NULL_HANDLE, 1, &pipeline,
                                       nullptr, &r.pipeline) == VK_SUCCESS);
        auto *output = static_cast<uint32_t *>(r.mapped);
        for (uint32_t i = 0; i < maximum + guard; ++i)
          output[i] = 0xdeadbeef;
        CHECK(vkResetCommandBuffer(command, 0) == VK_SUCCESS);
        auto begin = structure<VkCommandBufferBeginInfo>(
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        CHECK(vkBeginCommandBuffer(command, &begin) == VK_SUCCESS);
        auto barrier =
            structure<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &barrier, 0, nullptr, 0, nullptr);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, r.pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                r.layout, 0, 1, &set, 0, nullptr);
        const uint32_t seed = 0x13579bdu + cases;
        vkCmdPushConstants(command, r.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(seed), &seed);
        vkCmdDispatch(command, groups, 1, 1);
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
                             nullptr, 0, nullptr);
        CHECK(vkEndCommandBuffer(command) == VK_SUCCESS);
        auto submit = structure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        CHECK(vkQueueSubmit(queue, 1, &submit, r.fence) == VK_SUCCESS);
        CHECK(vkWaitForFences(r.device, 1, &r.fence, VK_TRUE, 10000000000ull) ==
              VK_SUCCESS);
        uint32_t bad = 0;
        for (uint32_t i = 0; i < maximum + guard; ++i) {
          const auto expected = i < groups * lanes ? 0u : 0xdeadbeefu;
          if (output[i] != expected) {
            if (bad < 4)
              std::fprintf(
                  stderr,
                  "shared=%u lanes=%u index=%u xor=%08x expected=%08x\n",
                  slots * 8, lanes, i, output[i], expected);
            ++bad;
          }
        }
        CHECK(bad == 0);
        CHECK(vkResetFences(r.device, 1, &r.fence) == VK_SUCCESS);
        vkDestroyPipeline(r.device, r.pipeline, nullptr);
        r.pipeline = VK_NULL_HANDLE;
        ++cases;
      }
    }
    CHECK(cases >= 4);
    std::printf("Shared-memory integer isolation: %u cases passed\n", cases);
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
