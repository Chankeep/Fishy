#pragma once

#include "spdlog/common.h"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include <source_location>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef NDEBUG
	#define FISHY_LOG_TRACE(...) do {} while(0)
#else
	#define FISHY_LOG_TRACE(...) ::Fishy::LogSystem::get().trace(__VA_ARGS__)
#endif

#define FISHY_LOG_INFO(...)     ::Fishy::LogSystem::get().info(__VA_ARGS__)
#define FISHY_LOG_WARN(...)     ::Fishy::LogSystem::get().warn(__VA_ARGS__)
#define FISHY_LOG_ERROR(...)    ::Fishy::LogSystem::get().error(__VA_ARGS__)
#define FISHY_LOG_CRITICAL(...) ::Fishy::LogSystem::get().critical(__VA_ARGS__)

namespace Fishy {

// Helper struct to 'smuggle' source_location
struct LogFormat {
	std::string_view str;
	std::source_location loc;

	// Constructor: takes string, automatically gets caller's location
	// Template T allows passing const char*, std::string, string_view, etc.
	template <typename T>
	LogFormat(const T& s, const std::source_location& l = std::source_location::current()) : str(s), loc(l) {}
};

class LogSystem {
public:
	// Get single instance
	static LogSystem& get();

	// Disable copy
	LogSystem(const LogSystem&) = delete;
	LogSystem& operator=(const LogSystem&) = delete;

	// Initialize log system (stdout, file, ringbuffer)
	void init(const std::string& log_filename = "vulkan_app.log");

	// Get spdlog object pointer (for user_data in vk-bootstrap callback)
	[[nodiscard]] std::shared_ptr<spdlog::logger> logger() { return m_logger; }
	[[nodiscard]] std::shared_ptr<spdlog::logger> logger() const { return m_logger; }

	// Get RingBuffer Sink (used for GUI display, e.g. ImGui)
	// Return type requires incorporating specific spdlog headers in cpp, or use auto/template here
	// For cleaner interface, void* could be used, or ringbuffer_sink.h should be included
	// Suggestion: Retrieve the latest log texts directly
	[[nodiscard]] std::vector<std::string> get_ringbuffer_logs(size_t count);

	// Static Vulkan Debug Callback function (matching vulkan.h signature)
	static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	template <typename... Args> void trace(LogFormat fmt, Args&&... args) {
		log_impl(spdlog::level::trace, fmt, std::forward<Args>(args)...);
	}
	template <typename... Args> void info(LogFormat fmt, Args&&... args) {
		log_impl(spdlog::level::info, fmt, std::forward<Args>(args)...);
	}
	template <typename... Args> void warn(LogFormat fmt, Args&&... args) {
		log_impl(spdlog::level::warn, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args> void error(LogFormat fmt, Args&&... args) {
		log_impl(spdlog::level::err, fmt, std::forward<Args>(args)...);
	}
	template <typename... Args> void critical(LogFormat fmt, Args&&... args) {
		log_impl(spdlog::level::critical, fmt, std::forward<Args>(args)...);
	}

private:
	LogSystem() = default;
	~LogSystem() = default;

	template <typename... Args> void log_impl(spdlog::level::level_enum lvl, const LogFormat& fmt, Args&&... args) {
		if (!m_logger)
			return;

		spdlog::source_loc spd_loc{fmt.loc.file_name(), static_cast<int>(fmt.loc.line()), fmt.loc.function_name()};

		// Note: we use runtime formatting here because string is wrapped in struct.
		// This sacrifices compile-time format checking (e.g. incorrect {} count errors during runtime instead of compile time)
		// but gains perfect support for source_location.
		m_logger->log(spd_loc, lvl, spdlog::fmt_lib::runtime(fmt.str), std::forward<Args>(args)...);
	}

	std::shared_ptr<spdlog::logger> m_logger;
	std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> m_ringbuffer_sink;
};
} // namespace Fishy