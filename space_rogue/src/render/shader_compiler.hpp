#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

std::vector<char> readFile(const std::string& filename);
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);