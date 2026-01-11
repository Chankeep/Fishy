#pragma once

#include <cstdint>

namespace Fishy {

// ============================================================================
// Shadow Map Constants
// ============================================================================

static constexpr uint32_t SHADOW_MAP_SIZE = 2048;

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
 * Used by all graphics shaders to access instance and global data via Buffer Device Address.
 */
struct PushConstants {
	uint64_t instanceDataAddress; // BDA pointer to InstanceData array
	uint64_t globalDataAddress;	  // BDA pointer to GlobalUBO
};

} // namespace Fishy
