#include "PipelineManager.h"

#include "PipelineBuilder.h"
#include "core/VulkanDevice.h"
#include <fstream>

namespace Fishy {

PipelineManager::PipelineManager(VulkanDevice& device) : _device(device) { loadCache(); }

PipelineManager::~PipelineManager() {
	try {
		saveCache();
	} catch (const std::exception& e) {
		FISHY_LOG_ERROR("Failed to save pipeline cache: {}", e.what());
	}
}

GraphicsPipeline* PipelineManager::getOrCreate(const PipelineKey& key, PipelineBuilder& builder) {
	// 1. Check CPU-side cache
	if (auto it = _pipelines.find(key); it != _pipelines.end()) {
		FISHY_LOG_TRACE("Pipeline cache hit for shaderId: {}", key.shaderId);
		return it->second.get();
	}

	// 2. Cache miss - create new pipeline using PipelineBuilder
	FISHY_LOG_TRACE("Pipeline cache miss for shaderId: {}, creating new pipeline", key.shaderId);

	// Build pipeline with our Vulkan cache for driver-level binary caching
	// Throws on failure (exception propagates to caller)
	auto pipeline = builder.build(*_device, nullptr, _vulkanCache);
	auto* ptr = pipeline.get();
	_pipelines[key] = std::move(pipeline);
	return ptr;
}

void PipelineManager::loadCache() {
	std::ifstream file(CACHE_FILENAME, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		FISHY_LOG_INFO("No pipeline cache file found, creating new cache");
		_vulkanCache = vk::raii::PipelineCache(*_device, vk::PipelineCacheCreateInfo{});
		return;
	}

	const auto fileSize = static_cast<size_t>(file.tellg());
	file.seekg(0);

	std::vector<char> cacheData(fileSize);
	file.read(cacheData.data(), static_cast<std::streamsize>(fileSize));
	file.close();

	vk::PipelineCacheCreateInfo createInfo{.initialDataSize = fileSize, .pInitialData = cacheData.data()};

	try {
		_vulkanCache = vk::raii::PipelineCache(*_device, createInfo);
		FISHY_LOG_INFO("Loaded pipeline cache from disk ({} bytes)", fileSize);
	} catch (const std::exception& e) {
		FISHY_LOG_WARN("Failed to load pipeline cache, creating new: {}", e.what());
		_vulkanCache = vk::raii::PipelineCache(*_device, vk::PipelineCacheCreateInfo{});
	}
}

void PipelineManager::saveCache() {
	if (!*_vulkanCache) {
		return;
	}

	auto cacheData = _vulkanCache.getData();
	if (cacheData.empty()) {
		FISHY_LOG_TRACE("Pipeline cache is empty, nothing to save");
		return;
	}

	std::ofstream file(CACHE_FILENAME, std::ios::binary);
	if (!file.is_open()) {
		FISHY_LOG_ERROR("Failed to open pipeline cache file for writing");
		return;
	}

	file.write(reinterpret_cast<const char*>(cacheData.data()), cacheData.size());
	file.close();

	FISHY_LOG_INFO("Saved pipeline cache to disk ({} bytes)", cacheData.size());
}

void PipelineManager::clear() {
	_pipelines.clear();
	FISHY_LOG_TRACE("Cleared all cached pipelines");
}

} // namespace Fishy
