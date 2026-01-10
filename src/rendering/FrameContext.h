#pragma once

#include "core/VulkanDevice.h"
#include <span>
#include <stdexcept>
#include <vector>

namespace Fishy {

/**
 * @brief Per-frame context encapsulating synchronization primitives and linear allocator.
 *
 * This struct is designed for per-frame resource isolation, eliminating resource contention
 * in multi-frame-in-flight scenarios. The linear allocator provides zero-cost reset by
 * simply resetting the offset pointer.
 */
struct FrameContext {
	vk::raii::CommandBuffer commandBuffer;
	vk::raii::Semaphore imageAvailableSemaphore;
	vk::raii::Semaphore renderFinishedSemaphore;
	vk::raii::Fence inFlightFence;

	/**
	 * @brief Constructs a FrameContext with all necessary Vulkan resources.
	 * @param device Reference to the VulkanDevice for resource creation.
	 * @param pool Reference to the CommandPool for command buffer allocation.
	 * @param linearMemorySize Size of the linear allocator in bytes (default: 10MB).
	 */
	FrameContext(const VulkanDevice& device, CommandPool& pool, size_t linearMemorySize = 10 * 1024 * 1024)
		: commandBuffer(pool.allocateBuffer(true)), imageAvailableSemaphore(*device, vk::SemaphoreCreateInfo{}),
		  renderFinishedSemaphore(*device, vk::SemaphoreCreateInfo{}),
		  inFlightFence(*device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}), _device(device) {
		_linearMemory.resize(linearMemorySize);
	}

	// Non-copyable, move-constructible only (move assignment is deleted due to const reference member)
	FrameContext(const FrameContext&) = delete;
	FrameContext& operator=(const FrameContext&) = delete;
	FrameContext(FrameContext&&) noexcept = default;
	FrameContext& operator=(FrameContext&&) = delete;

	/**
	 * @brief Resets the frame context for reuse.
	 *
	 * Waits for the in-flight fence, resets the fence and command buffer,
	 * and clears the linear allocator (zero-cost by resetting offset).
	 */
	void reset() {
		[[maybe_unused]] auto result = _device->waitForFences({*inFlightFence}, vk::True, UINT64_MAX);
		_device->resetFences({*inFlightFence});
		commandBuffer.reset();
		_allocatedOffset = 0; // Instant release of all per-frame dynamic memory
	}

	/**
	 * @brief Allocates a contiguous array of T from the linear allocator.
	 *
	 * Memory is aligned to alignof(T). The allocation is valid until reset() is called.
	 *
	 * @tparam T The type of elements to allocate.
	 * @param count Number of elements to allocate.
	 * @return std::span<T> A span pointing to the allocated memory.
	 * @throws std::runtime_error if allocation exceeds linear memory capacity.
	 */
	template <typename T> std::span<T> allocate(size_t count) {
		if (count == 0) {
			return {};
		}

		constexpr size_t alignment = alignof(T);
		const size_t requiredSize = sizeof(T) * count;

		// Align the current offset
		const size_t alignedOffset = (_allocatedOffset + alignment - 1) & ~(alignment - 1);
		const size_t newOffset = alignedOffset + requiredSize;

		if (newOffset > _linearMemory.size()) {
			LogSystem::get().error(
				"FrameContext linear allocator out of memory: requested {} bytes, available {} bytes", requiredSize,
				_linearMemory.size() - _allocatedOffset);
			throw std::runtime_error("FrameContext linear allocator out of memory");
		}

		T* ptr = reinterpret_cast<T*>(_linearMemory.data() + alignedOffset);
		_allocatedOffset = newOffset;

		return std::span<T>(ptr, count);
	}

	/**
	 * @brief Allocates raw bytes from the linear allocator with specified alignment.
	 *
	 * @param size Number of bytes to allocate.
	 * @param alignment Required alignment (must be power of 2).
	 * @return void* Pointer to the allocated memory.
	 * @throws std::runtime_error if allocation exceeds linear memory capacity.
	 */
	void* allocateBytes(size_t size, size_t alignment = alignof(std::max_align_t)) {
		if (size == 0) {
			return nullptr;
		}

		if ((alignment & (alignment - 1)) != 0) {
			LogSystem::get().error("FrameContext allocateBytes: alignment {} is not a power of 2", alignment);
			throw std::runtime_error("Alignment must be power of 2");
		}

		const size_t alignedOffset = (_allocatedOffset + alignment - 1) & ~(alignment - 1);
		const size_t newOffset = alignedOffset + size;

		if (newOffset > _linearMemory.size()) {
			LogSystem::get().error(
				"FrameContext linear allocator out of memory: requested {} bytes, available {} bytes", size,
				_linearMemory.size() - _allocatedOffset);
			throw std::runtime_error("FrameContext linear allocator out of memory");
		}

		void* ptr = _linearMemory.data() + alignedOffset;
		_allocatedOffset = newOffset;

		return ptr;
	}

	/**
	 * @brief Returns the current allocated size in bytes.
	 */
	[[nodiscard]] size_t getAllocatedSize() const { return _allocatedOffset; }

	/**
	 * @brief Returns the total capacity of the linear allocator in bytes.
	 */
	[[nodiscard]] size_t getCapacity() const { return _linearMemory.size(); }

	/**
	 * @brief Returns the remaining available bytes in the linear allocator.
	 */
	[[nodiscard]] size_t getRemainingCapacity() const { return _linearMemory.size() - _allocatedOffset; }

private:
	const VulkanDevice& _device;
	std::vector<uint8_t> _linearMemory;
	size_t _allocatedOffset = 0;
};

} // namespace Fishy