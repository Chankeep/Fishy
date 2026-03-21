#pragma once

#include <cstdint>
#include <utility>

namespace Fishy {

// ============================================================================
// Shadow Map Constants
// ============================================================================

static constexpr uint32_t SHADOW_ATLAS_SIZE = 4096;
static constexpr uint32_t SHADOW_TILE_SIZE = 1024;
static constexpr uint32_t MAX_SHADOW_TILES = 16; // (4096/1024)²
static constexpr uint32_t SHADOW_TILES_PER_ROW = SHADOW_ATLAS_SIZE / SHADOW_TILE_SIZE;

// Pixel offset of a shadow tile in the atlas
inline constexpr std::pair<uint32_t, uint32_t> shadowTilePixelOffset(uint32_t tileIndex) {
	return {(tileIndex % SHADOW_TILES_PER_ROW) * SHADOW_TILE_SIZE,
			(tileIndex / SHADOW_TILES_PER_ROW) * SHADOW_TILE_SIZE};
}

// ============================================================================
// Frame Constants
// ============================================================================

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// ============================================================================
// Indirect Rendering Constants
// ============================================================================

// Initial buffer sizes for indirect rendering
static constexpr uint32_t INITIAL_MAX_OBJECTS = 256;
static constexpr uint32_t INITIAL_MAX_COMMANDS = 256;

// ============================================================================
// Bindless Resource Constants
// ============================================================================

// Maximum number of textures in bindless descriptor array
static constexpr uint32_t MAX_BINDLESS_TEXTURES = 4096;

// Sentinel value for unbound/invalid texture indices
constexpr uint32_t INVALID_TEXTURE_INDEX = 0xFFFFFFFF;

// ============================================================================
// Push Constants
// ============================================================================

/**
 * @brief Push constants layout for BDA-based rendering.
 *
 * Used by all graphics shaders to access instance, global, and light data via Buffer Device Address.
 */
struct PushConstants {
	uint64_t instanceDataAddress; // BDA pointer to InstanceData array
	uint64_t globalDataAddress;	  // BDA pointer to GlobalUBO
	uint64_t lightDataAddress;	  // BDA pointer to Lights SSBO
	uint64_t shadowDataAddress;   // BDA pointer to shadow SSBO
	uint32_t shadowRenderIndex;   // Current shadow tile index
	uint32_t _pad;
}; // 40 bytes

} // namespace Fishy
