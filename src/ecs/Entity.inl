#pragma once

#include "../scene/Scene.h"

namespace Fishy {

template <typename T, typename... Args> T& Entity::addComponent(Args&&... args) {
	return _scene->getRegistry().emplace<T>(_handle, std::forward<Args>(args)...);
}

template <typename T> T& Entity::getComponent() { return _scene->getRegistry().get<T>(_handle); }

template <typename T> const T& Entity::getComponent() const { return _scene->getRegistry().get<T>(_handle); }

template <typename T, typename... Args> T& Entity::getOrAddComponent(Args&&... args)  {
	if(hasComponent<T>()){
		return getComponent<T>();
	}
	return addComponent<T>(std::forward<Args>(args)...);
}

template <typename T> bool Entity::hasComponent() const { return _scene->getRegistry().all_of<T>(_handle); }

template <typename T> void Entity::removeComponent() { _scene->getRegistry().remove<T>(_handle); }

} // namespace Fishy
