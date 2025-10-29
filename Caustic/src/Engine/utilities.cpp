#include "precomp.h"
#include "utilities.h"
#include <fstream>
#include <cstring>

namespace veng {

bool streq(const char* left, const char* right) {
  return std::strcmp(left, right) == 0;
}

std::vector<std::uint8_t> ReadFile(std::filesystem::path shader_path) {
  if(!std::filesystem::exists(shader_path)) {
    return {};
  }

  if(!std::filesystem::is_regular_file(shader_path)) {
    return {};
  }

  std::ifstream file(shader_path, std::ios::binary);
  if(!file.is_open()) {
    return {};
  }

  const auto file_size = std::filesystem::file_size(shader_path);
  std::vector<std::uint8_t> buffer(static_cast<size_t>(file_size));
  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
  return buffer;
}

}  // namespace veng
