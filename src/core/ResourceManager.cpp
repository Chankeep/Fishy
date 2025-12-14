#include "ResourceManager.h"
#include "VulkanDevice.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Fishy {

ResourceManager::ResourceManager(VulkanDevice& device) : _device(device) {}

ResourceManager::~ResourceManager() { clear(); }

const vk::raii::ShaderModule& ResourceManager::getShader(const std::string& filepath) {
	// Check cache first
	auto it = _shaderCache.find(filepath);
	if (it != _shaderCache.end()) {
		return it->second;
	}

	// Not in cache, load from file
	std::vector<char> code = readFile(filepath);
	vk::raii::ShaderModule module = createShaderModule(code);

	// Insert into cache and return reference to the inserted value
	// emplace returns a pair<iterator, bool>, .first is the iterator
	auto insertedIt = _shaderCache.emplace(filepath, std::move(module));
	return insertedIt.first->second;
}

void ResourceManager::clear() {
	// RAII objects in the map will be destroyed automatically
	_shaderCache.clear();
}

std::vector<char> ResourceManager::readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::string cwd = std::filesystem::current_path().string();
		throw std::runtime_error("failed to open file: " + filename + " (cwd: " + cwd + ")");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

vk::raii::ShaderModule ResourceManager::createShaderModule(const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(),
										  .pCode = reinterpret_cast<const uint32_t*>(code.data())};

	// We access the raw vk::raii::Device from our VulkanDevice wrapper
	return vk::raii::ShaderModule(_device.getDevice(), createInfo);
}

} // namespace Fishy
