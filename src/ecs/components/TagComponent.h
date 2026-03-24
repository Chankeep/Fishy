#pragma once

#include <string>

namespace Fishy {

/**
 * @brief Component for storing an entity's name/tag.
 */
struct TagComponent {
	std::string name;

	TagComponent() = default;
	explicit TagComponent(const std::string& name) : name(name) {}
	explicit TagComponent(std::string&& name) : name(std::move(name)) {}
};

} // namespace Fishy
