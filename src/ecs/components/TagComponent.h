#pragma once

#include <string>

namespace Fishy {

/**
 * @brief Component for storing an entity's name/tag.
 */
struct TagComponent {
	std::string tag;

	TagComponent() = default;
	explicit TagComponent(const std::string& tag) : tag(tag) {}
	explicit TagComponent(std::string&& tag) : tag(std::move(tag)) {}
};

} // namespace Fishy
