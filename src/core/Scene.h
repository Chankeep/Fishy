#pragma once

#include "Fishy.h"
#include "../shapes/Sphere.h"
#include "Interaction.h"
#include "Primitive.h"

namespace Fishy
{
    class Scene
    {
    public:
        Scene() = default;

        Scene(std::vector<std::shared_ptr<Primitive>> prims) : prims(std::move(prims))
        {
        }

        bool Intersect(Ray &ray, Interaction &isect)
        {
            bool isHit = false;
            for (auto &prim: prims)
            {
                if (prim->Intersect(ray, isect))
                    isHit = true;

            }
            return isHit;
        }

        static Scene CreateSmallptScene()
        {
            std::shared_ptr<Shape> left = std::make_shared<Sphere>(1e5, vector3(1e5 + 20, 0, 0));
            std::shared_ptr<Shape> right = std::make_shared<Sphere>(1e5, vector3(-1e5 + -20, 0, 0));
            std::shared_ptr<Shape> back = std::make_shared<Sphere>(1e5, vector3(0, 0, 1e5 + 30));
            std::shared_ptr<Shape> front = std::make_shared<Sphere>(1e5, vector3(0, 0, -1e5 - 20));
            std::shared_ptr<Shape> bottom = std::make_shared<Sphere>(1e5, vector3(0, -1e5 - 18, 0));
            std::shared_ptr<Shape> top = std::make_shared<Sphere>(1e5, vector3(0, 1e5 + 18, 0));

            std::shared_ptr<Shape> mirror = std::make_shared<Sphere>(5, vector3(2, 0, 10));
            std::shared_ptr<Shape> glass = std::make_shared<Sphere>(5, vector3(-10, 0, 15));
            std::shared_ptr<Shape> light = std::make_shared<Sphere>(600, vector3(0, 618 - .21, 0));

            std::vector<std::shared_ptr<Primitive>> prims;
            std::shared_ptr<Material> red = std::make_shared<MatteMaterial>(Color(.75, .25, .25));
            std::shared_ptr<Material> blue = std::make_shared<MatteMaterial>(Color(.25, .25, .75));
            std::shared_ptr<Material> gray = std::make_shared<MatteMaterial>(Color(.75, .75, .75));
            std::shared_ptr<Material> black = std::make_shared<MatteMaterial>(Color());

            std::shared_ptr<Light> area_light = std::make_shared<Light>(Color(8, 8, 8), light.get());

            prims.emplace_back(std::make_shared<GeometricPrimitive>(left, red, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(right, blue, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(back, gray, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(front, red, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(bottom, gray, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(top, gray, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(mirror, blue, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(glass, red, nullptr));
            prims.emplace_back(std::make_shared<GeometricPrimitive>(light, black, area_light));


            return Scene{prims};
        }

    private:
        std::vector<std::shared_ptr<Primitive>> prims;
    };
}
