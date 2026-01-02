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

		// 1. Stdout Sink (带颜色，多线程安全)
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::trace);
		// 自定义格式: [时间] [级别] [线程ID] 消息
		console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [t:%t] [%s:%#] %v");
		sinks.push_back(console_sink);

		// 2. File Sink (覆盖模式)
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_filename, true);
		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%!] %v");
		sinks.push_back(file_sink);

		// 3. Ringbuffer Sink (保留最近 128 条日志)
		auto ring_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(128);
		ring_sink->set_level(spdlog::level::trace);
		// 保存原始指针以便 get_ringbuffer_logs 使用
		m_ringbuffer_sink = ring_sink;
		sinks.push_back(ring_sink);

		// 创建 logger
		m_logger = std::make_shared<spdlog::logger>("vk_logger", sinks.begin(), sinks.end());
		m_logger->set_level(spdlog::level::trace);

		// 当遇到 Error 级别时立即刷新到文件，防止崩溃丢失日志
		m_logger->flush_on(spdlog::level::err);

		// 设为默认，方便第三方库如果也用 spdlog
		spdlog::set_default_logger(m_logger);

		LogSystem::get().info("LogSystem initialized: Stdout, File({}), Ringbuffer", log_filename);

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

// Vulkan Callback 实现
VKAPI_ATTR VkBool32 VKAPI_CALL LogSystem::vulkan_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

	// 从 UserData 获取 logger
	auto logger = static_cast<spdlog::logger*>(pUserData);
	if (!logger)
		return VK_FALSE;

	// 格式化前缀，例如 [Validation] 或 [Performance]
	std::string prefix = "[Vulkan]";
	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		prefix = "[Vulkan-Perf]";
	else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		prefix = "[Vulkan-Valid]";

	// 根据严重程度分发日志
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