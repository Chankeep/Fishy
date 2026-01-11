#pragma once
#include <array>
#include <cstring>
#include <expected>
#include <string_view>

namespace Fishy {
struct ModelLoadError {
	enum class Code : uint8_t { FileNotFound, ParseFailed, InvalidFormat, TextureLoadFailed, Unknown };

	Code code;
	std::array<char, 256> messageBuffer{};

	[[nodiscard]] std::string_view message() const noexcept { return {messageBuffer.data()}; }

	static ModelLoadError make(Code c, std::string_view msg) {
		ModelLoadError err{c, {}};
		std::size_t len = std::min(msg.size(), err.messageBuffer.size() - 1);

		std::memcpy(err.messageBuffer.data(), msg.data(), len);

		return err;
	}
};

template <typename T> using Result = std::expected<T, ModelLoadError>;
using VoidResult = std::expected<void, ModelLoadError>;

} // namespace Fishy