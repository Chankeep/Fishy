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

namespace Fishy {

// 辅助结构体，用来“偷运”source_location
struct LogFormat {
	std::string_view str;
	std::source_location loc;

	// 构造函数：接收字符串，同时自动获取调用者的位置
	// 这里的模板 T 允许你传入 const char*, std::string, string_view 等
	template <typename T>
	LogFormat(const T& s, const std::source_location& l = std::source_location::current()) : str(s), loc(l) {}
};

class LogSystem {
public:
	// 获取单例
	static LogSystem& get();

	// 禁止拷贝
	LogSystem(const LogSystem&) = delete;
	LogSystem& operator=(const LogSystem&) = delete;

	// 初始化日志系统 (stdout, file, ringbuffer)
	void init(const std::string& log_filename = "vulkan_app.log");

	// 获取 spdlog 对象指针 (用于 vk-bootstrap callback 的 user_data)
	[[nodiscard]] std::shared_ptr<spdlog::logger> logger() { return m_logger; }
	[[nodiscard]] std::shared_ptr<spdlog::logger> logger() const { return m_logger; }

	// 获取 RingBuffer Sink (用于 GUI 显示，如 ImGui)
	// 返回类型需要你在 cpp 中包含具体的 spdlog 头文件，或者这里使用 auto/template
	// 为了接口干净，这里返回 void* 或者你需要包含 ringbuffer_sink.h
	// 建议：直接获取最近的日志文本
	[[nodiscard]] std::vector<std::string> get_ringbuffer_logs(size_t count);

	// 静态 Vulkan Debug 回调函数 (符合 vulkan.h 签名)
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

		// 注意：这里我们使用 runtime 格式化，因为我们把字符串包在了结构体里
		// 这样会牺牲掉编译期的格式检查（比如 {} 数量不对不会报错，而是运行时报错），
		// 但换来了完美的 source_location 支持。
		m_logger->log(spd_loc, lvl, spdlog::fmt_lib::runtime(fmt.str), std::forward<Args>(args)...);
	}

	std::shared_ptr<spdlog::logger> m_logger;
	std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> m_ringbuffer_sink;
};
} // namespace Fishy