#pragma once

#include "GraphicsPipeline.h"
#include <entt/core/hashed_string.hpp>
#include <memory>
#include <unordered_map>

namespace Fishy {

class VulkanDevice;
class PipelineBuilder;

/**
 * @brief Hash key for pipeline caching.
 *
 * Uses entt::id_type (pre-hashed) for shader identification to avoid
 * runtime string hashing overhead. All fields are POD for fast comparison.
 */
struct PipelineKey {
	entt::id_type shaderId;	 // Pre-hashed shader path + entry points
	entt::id_type variantId; // Reserved for Slang variants / specialization constants
	vk::Format colorFormat;
	vk::Format depthFormat;
	vk::CullModeFlags cullMode;
	vk::FrontFace frontFace;
	vk::CompareOp depthCompareOp;
	uint8_t flags; // bit0=depthWrite, bit1=depthTest, bit2=blend

	// Pack boolean flags
	static constexpr uint8_t FLAG_DEPTH_WRITE = 1 << 0;
	static constexpr uint8_t FLAG_DEPTH_TEST = 1 << 1;
	static constexpr uint8_t FLAG_BLEND = 1 << 2;

	[[nodiscard]] bool depthWriteEnabled() const { return flags & FLAG_DEPTH_WRITE; }
	[[nodiscard]] bool depthTestEnabled() const { return flags & FLAG_DEPTH_TEST; }
	[[nodiscard]] bool blendEnabled() const { return flags & FLAG_BLEND; }

	bool operator==(const PipelineKey&) const = default;
};

/**
 * @brief Hash function for PipelineKey.
 */
struct PipelineKeyHash {
	size_t operator()(const PipelineKey& key) const noexcept {
		// Combine all fields into a single hash using FNV-1a style mixing
		// size_t h = 14695981039346656037ULL; // FNV offset basis
		// auto mix = [&h](auto val) {
		// 	h ^= static_cast<size_t>(val);
		// 	h *= 1099511628211ULL; // FNV prime
		// };

		// mix(key.shaderId);
		// mix(key.variantId);
		// mix(static_cast<uint32_t>(key.colorFormat));
		// mix(static_cast<uint32_t>(key.depthFormat));
		// mix(static_cast<uint32_t>(key.cullMode));
		// mix(static_cast<uint32_t>(key.frontFace));
		// mix(static_cast<uint32_t>(key.depthCompareOp));
		// mix(key.flags);

		// return h;
		return entt::hashed_string::value(reinterpret_cast<const char*>(&key), sizeof(key));
	}
};

/**
 * @brief Manages graphics pipeline creation with hash-based caching.
 *
 * This class provides:
 * - CPU-side deduplication via PipelineKey hashing
 * - GPU-side binary caching via vk::PipelineCache (passed to Vulkan API)
 * - Disk persistence for the Vulkan pipeline cache
 *
 * Usage:
 *   PipelineBuilder builder(device);
 *   builder.setShaders(...)
 *          .setLayout(...)
 *          .build(pipelineManager);  // Returns cached or newly created pipeline
 */
class PipelineManager {
public:
	explicit PipelineManager(VulkanDevice& device);
	~PipelineManager();
	PipelineManager(const PipelineManager&) = delete;
	PipelineManager& operator=(const PipelineManager&) = delete;

	/**
	 * @brief Get or create a pipeline for the given key.
	 *
	 * If a pipeline with the same key exists, returns the cached instance.
	 * Otherwise, uses the provided builder to create a new pipeline.
	 * Throws on creation failure.
	 *
	 * @param key The pipeline configuration key.
	 * @param builder The builder configured with pipeline parameters.
	 * @return Pointer to the pipeline (never null).
	 * @throws std::runtime_error if pipeline creation fails.
	 */
	[[nodiscard]] GraphicsPipeline* getOrCreate(const PipelineKey& key, class PipelineBuilder& builder);

	/**
	 * @brief Load pipeline cache from disk.
	 */
	void loadCache();

	/**
	 * @brief Save pipeline cache to disk.
	 */
	void saveCache();

	/**
	 * @brief Get the Vulkan pipeline cache object.
	 */
	[[nodiscard]] const vk::raii::PipelineCache& getVulkanCache() const { return _vulkanCache; }

	/**
	 * @brief Clear all cached pipelines.
	 */
	void clear();

private:
	VulkanDevice& _device;
	vk::raii::PipelineCache _vulkanCache = nullptr;
	std::unordered_map<PipelineKey, std::unique_ptr<GraphicsPipeline>, PipelineKeyHash> _pipelines;

	static constexpr const char* CACHE_FILENAME = "pipeline_cache.bin";
};

} // namespace Fishy
