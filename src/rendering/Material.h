#pragma once

#include "../resources/Texture.h"
#include "DrawTypes.h" // For InstanceData types (includes RenderConstants.h)
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <variant>

namespace Fishy {

/**
 * @brief Material class following glTF PBR Metallic-Roughness workflow.
 *
 * Supports KHR_materials_clearcoat and other glTF extensions.
 * Pure data container - no GPU resources. GPU resources are managed by ResourceManager.
 * Textures are held via entt::resource handles for proper cache integration.
 */
class Material {
public:
	struct PBRParameters {
		// Metallic-Roughness workflow
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;

		// Normal and occlusion
		float normalScale = 1.0f;
		float occlusionStrength = 1.0f;

		// Emissive
		glm::vec3 emissiveFactor = glm::vec3(0.0f);

		// Alpha
		enum class AlphaMode { OPAQUE_MODE, MASK, BLEND };
		AlphaMode alphaMode = AlphaMode::OPAQUE_MODE;
		float alphaCutoff = 0.5f;

		// Rendering
		bool doubleSided = false;

		// KHR_materials_clearcoat
		float clearcoatFactor = 0.0f;
		float clearcoatRoughnessFactor = 0.0f;

		// KHR_materials_transmission
		float transmissionFactor = 0.0f;

		// KHR_materials_ior
		float ior = 1.5f;

		// KHR_materials_emissive_strength
		float emissiveStrength = 1.0f;
	};

	struct TextureIndices {
		uint32_t baseColor = INVALID_TEXTURE_INDEX;
		uint32_t metallicRoughness = INVALID_TEXTURE_INDEX;
		uint32_t normal = INVALID_TEXTURE_INDEX;
		uint32_t occlusion = INVALID_TEXTURE_INDEX;
		uint32_t emissive = INVALID_TEXTURE_INDEX;

		// Extensions
		uint32_t clearcoat = INVALID_TEXTURE_INDEX;
		uint32_t clearcoatRoughness = INVALID_TEXTURE_INDEX;
		uint32_t clearcoatNormal = INVALID_TEXTURE_INDEX;
		uint32_t transmission = INVALID_TEXTURE_INDEX;
	};

	Material() = default;
	~Material() = default;

	Material(const Material&) = default;
	Material& operator=(const Material&) = default;
	Material(Material&&) = default;
	Material& operator=(Material&&) = default;

	// Core PBR Textures (glTF naming convention)
	// using entt::resource handles for cache-friendly resource management
	entt::resource<Texture> baseColorMap;		  // RGBA base color
	entt::resource<Texture> metallicRoughnessMap; // G=roughness, B=metallic (glTF spec)
	entt::resource<Texture> normalMap;			  // RGB normal map
	entt::resource<Texture> occlusionMap;		  // R=ambient occlusion
	entt::resource<Texture> emissiveMap;		  // RGB emissive

	// Extension Textures
	entt::resource<Texture> clearcoatMap;		   // KHR_materials_clearcoat
	entt::resource<Texture> clearcoatRoughnessMap; // KHR_materials_clearcoat
	entt::resource<Texture> clearcoatNormalMap;	   // KHR_materials_clearcoat
	entt::resource<Texture> transmissionMap;	   // KHR_materials_transmission

	// Pre-computed bindless texture indices (populated by ResourceManager after texture loading)
	// These are computed once when materials are created/modified, avoiding per-frame hash lookups.
	TextureIndices textureIndices;

	PBRParameters params;
};

/**
 * @brief GPU uniform buffer layout for Material (std140 aligned).
 *
 * Must match the shader's material uniform block layout.
 */
struct MaterialUBO {
	glm::vec4 baseColorFactor; // 16 bytes, offset 0
	float metallicFactor;	   // 4 bytes, offset 16
	float roughnessFactor;	   // 4 bytes, offset 20
	float normalScale;		   // 4 bytes, offset 24
	float occlusionStrength;   // 4 bytes, offset 28
	glm::vec4 emissiveFactor;  // 16 bytes, offset 32 (w = emissiveStrength)
	float alphaCutoff;		   // 4 bytes, offset 48
	uint32_t flags;			   // 4 bytes, offset 52 (doubleSided, alphaMode, texture flags)

	// Extensions
	float clearcoatFactor;			// 4 bytes, offset 56
	float clearcoatRoughnessFactor; // 4 bytes, offset 60
	float transmissionFactor;		// 4 bytes, offset 64
	float ior;						// 4 bytes, offset 68
	float padding[3];				// 12 bytes, offset 72 (total 80 bytes, 16-byte aligned)
};

} // namespace Fishy
