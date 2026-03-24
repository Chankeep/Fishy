#include "LogSystem.h"

#include <iostream>
#include <vector>

namespace Fishy {
LogSystem& LogSystem::get() {
	static LogSystem instance;
	return instance;
}

void LogSystem::init(const std::string& log_filename) {
	try {
		std::vector<spdlog::sink_ptr> sinks;

		// 1. Stdout Sink (Colored, thread-safe)
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::trace);
		// Custom format: [Time] [Level] [ThreadID] Message
		console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [t:%t] [%s:%#] %v");
		sinks.push_back(console_sink);

		// 2. File Sink (Overwrite mode)
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_filename, true);
		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%!] %v");
		sinks.push_back(file_sink);

		// 3. Ringbuffer Sink (Keep the latest 1024 logs)
		auto ring_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(1024);
		ring_sink->set_level(spdlog::level::trace);
		ring_sink->set_pattern("[%H:%M:%S] [%l] %v");
		// Save the original pointer for use by get_ringbuffer_logs
		m_ringbuffer_sink = ring_sink;
		sinks.push_back(ring_sink);

		// Create logger
		m_logger = std::make_shared<spdlog::logger>("vk_logger", sinks.begin(), sinks.end());
		m_logger->set_level(spdlog::level::trace);

		// Flush to file immediately on Error level to prevent loss of logs on crash
		m_logger->flush_on(spdlog::level::err);

		// Set as default, in case third-party libraries also use spdlog
		spdlog::set_default_logger(m_logger);

		FISHY_LOG_INFO("LogSystem initialized: Stdout, File({}), Ringbuffer", log_filename);

	} catch (const spdlog::spdlog_ex& ex) {
		std::cerr << "Log init failed: " << ex.what() << std::endl;
	}
}

std::vector<std::string> LogSystem::get_ringbuffer_logs(size_t count) {
	if (!m_ringbuffer_sink)
		return {};

	auto sink = m_ringbuffer_sink;
	return sink->last_formatted(count);
}

// Vulkan Callback Implementation
VKAPI_ATTR VkBool32 VKAPI_CALL LogSystem::vulkan_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

	// Get logger from UserData
	auto logger = static_cast<spdlog::logger*>(pUserData);
	if (!logger)
		return VK_FALSE;

	// Format prefix, e.g. [Validation] or [Performance]
	std::string prefix = "[Vulkan]";
	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		prefix = "[Vulkan-Perf]";
	else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		prefix = "[Vulkan-Valid]";

	// Dispatch log based on severity
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		logger->trace("{} {}", prefix, pCallbackData->pMessage);
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		logger->info("{} {}", prefix, pCallbackData->pMessage);
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		logger->warn("{} {}", prefix, pCallbackData->pMessage);
	} else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		logger->error("{} {}", prefix, pCallbackData->pMessage);
	}

	return VK_FALSE;
}

} // namespace Fishy