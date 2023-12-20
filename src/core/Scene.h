#pragma once
#include "common.h"
#include "../shapes/Sphere.h"
#include "Interaction.h"

namespace fishy
{
	class Scene
	{
	public:
		Scene() = default;
		Scene(std::vector<std::shared_ptr<Shape>> shapeList) : shapeList(std::move(shapeList)) {}

		bool Intersect(Ray& ray, Interaction& isect);
        static Scene CreateSmallptScene()
        {
            // std::shared_ptr<Shape> left   = std::make_shared<Sphere>(1e5, vector3(1e5 + 1, 40.8, -81.6));
            // std::shared_ptr<Shape> right  = std::make_shared<Sphere>(1e5, vector3(-1e5 + 99, 40.8, -81.6));
            // std::shared_ptr<Shape> back   = std::make_shared<Sphere>(1e5, vector3(50, 40.8, -1e5));
            // std::shared_ptr<Shape> front  = std::make_shared<Sphere>(1e5, vector3(50, 40.8, 1e5 - 170));
            // std::shared_ptr<Shape> bottom = std::make_shared<Sphere>(1e5, vector3(50, 1e5, -81.6));
            // std::shared_ptr<Shape> top    = std::make_shared<Sphere>(1e5, vector3(50, -1e5 + 81.6, -81.6));

            std::shared_ptr<Shape> mirror = std::make_shared<Sphere>(5, vector3(5,-5, -25));
            std::shared_ptr<Shape> glass  = std::make_shared<Sphere>(5, vector3(-5, 5, -25));
            // std::shared_ptr<Shape> light  = std::make_shared<Sphere>(600, vector3(50, 681.6 - .27, -81.6));
            std::vector<std::shared_ptr<Shape>> shapeList{ mirror, glass };
            // left, right, back, front, bottom, top, light,


            return Scene{ shapeList};
        }

	private:
		std::vector<std::shared_ptr<Shape>> shapeList;
	};
}
