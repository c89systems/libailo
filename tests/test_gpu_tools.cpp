#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>

#include <cstdint>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
  const auto length = input.tellg();
  if (length <= 0 || length % 4 != 0)
    return 2;
  std::vector<std::uint32_t> words(static_cast<std::size_t>(length) / 4);
  input.seekg(0);
  if (!input.read(reinterpret_cast<char *>(words.data()), length))
    return 3;
  spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_3);
  tools.SetMessageConsumer([](spv_message_level_t, const char *,
                              const spv_position_t &, const char *) {});
  if (!tools.Validate(words))
    return 4;
  spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_3);
  optimizer.RegisterPass(spvtools::CreateStripDebugInfoPass());
  std::vector<std::uint32_t> optimized;
  if (!optimizer.Run(words.data(), words.size(), &optimized) ||
      !tools.Validate(optimized))
    return 5;
  words[0] = 0;
  return tools.Validate(words) ? 6 : 0;
}
