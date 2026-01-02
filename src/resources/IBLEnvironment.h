#pragma once

#include "CubemapTexture.h"
#include "Texture.h"

#include <memory>
#include <string>

namespace Fishy {

/**
 * @brief Container for IBL (Image-Based Lighting) environment textures.
 *
 * Manages irradiance cubemap, pre-filtered specular cubemap, and BRDF LUT.
 */
struct IBLEnvironment {
	std::unique_ptr<CubemapTexture> irradianceMap;	// Diffuse IBL
	std::unique_ptr<CubemapTexture> prefilteredMap; // Specular IBL (with mipmaps)
	std::unique_ptr<Texture> brdfLUT;				// BRDF integration LUT (2D)

	uint32_t prefilteredMipLevels = 1;

	/**
	 * @brief Load IBL environment from directory.
	 * Expects: diffuse.ktx2, specular.ktx2
	 * BRDF LUT is generated if not cached.
	 */
	[[nodiscard]] static std::unique_ptr<IBLEnvironment> load(const VulkanDevice& device, const std::string& directory);

	/**
	 * @brief Check if BRDF LUT cache exists.
	 */
	[[nodiscard]] static bool hasCachedBRDFLUT(const std::string& cachePath);

	/**
	 * @brief Load BRDF LUT from cache or generate and save.
	 */
	[[nodiscard]] static std::unique_ptr<Texture> loadOrGenerateBRDFLUT(const VulkanDevice& device,
																		const std::string& cachePath);
};

} // namespace Fishy
