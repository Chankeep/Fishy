#pragma once
#include "common.h"
#include "Shape.h"

namespace fishy
{
	class Scene
	{
	public:
		Scene() = default;
		Scene(std::vector<std::shared_ptr<Shape>> shapeList) : shapeList(std::move(shapeList)) {}

		bool Intersect(Ray& ray);

	private:
		std::vector<std::shared_ptr<Shape>> shapeList;
	};
}
