//
// Created by chankeep on 12/18/2023.
//
#pragma once

#include "Fishy.h"
#include "Shape.h"
#include "Interaction.h"
#include "../materials/Material.h"
#include "../lights/Light.h"

namespace Fishy
{
    class Primitive
    {
    public:
        virtual ~Primitive() = default;
        virtual bool Intersect(Ray &r, Interaction &isect) const = 0;
    };

    class GeometricPrimitive : public Primitive
    {
    public:
        std::shared_ptr<Shape> shape;
        std::shared_ptr<Material> material;
        std::shared_ptr<Light> light;

        GeometricPrimitive(const std::shared_ptr<Shape> &shape,
                const std::shared_ptr<Material> &material,
                const std::shared_ptr<Light> &light)
                : shape(shape), material(material), light(light)
        {
        }

        ~GeometricPrimitive() override = default;

        bool Intersect(Ray &ray, Interaction &isect) const override
        {
            bool hit = shape->Intersect(ray, isect);
            if (hit)
            {
                isect.bsdf = material->Scattering(isect);
                isect.emission = light ? light->Le(isect, isect.w_o) : Color();
            }


            return hit;
        }

    };

}
// FishyRenderer

