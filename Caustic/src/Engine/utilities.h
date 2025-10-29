#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>

namespace veng {

bool streq(const char* left, const char* right);
std::vector<std::uint8_t> ReadFile(std::filesystem::path shader_path);

}
