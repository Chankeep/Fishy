//
// Created by chankeep on 3/12/2024.
//

#include "Plane.h"

namespace Fishy
{
    bool Plane::Intersect(const Ray &ray, Interaction &isect) const
    {
        float numerator = dot(p1 - ray.origin, normal);
        float denominator = dot(ray.direction, normal);
        float t = numerator / denominator;
        if(t < 0.0001 && t >= Infinity)
            return false;

        else if(t < isect.distance)
        {
            isect = Interaction(ray(t), (ray(t) - ray.origin).normalized(), -ray.direction, t);
        }
        return true;
    }

    void Plane::setTransform(Qt3DCore::QTransform *transform)
    {
        auto translation = transform->translation();
        translation.setX(-translation.x());
        transform->setTranslation(translation);
//        v1 = transform->matrix().map(v1);
//        v2 = transform->matrix().map(v2);
//        v3 = transform->matrix().map(v3);
//        v4 = transform->matrix().map(v4);
//        origin = transform->matrix().map(origin);
    }
} // Fishy