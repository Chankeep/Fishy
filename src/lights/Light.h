//
// Created by chankeep on 12/20/2023.
//

#ifndef FISHY_LIGHT_H
#define FISHY_LIGHT_H

#include "../core/Fishy.h"
#include "../core/Shape.h"

namespace Fishy
{
    class Light
    {
    public:
        Light() = default;
        Light(Color radiance, const Shape *shape) : radiance(radiance), shape(shape)
        {
        }
        virtual ~Light() = default;

        Color Le(const Interaction& lightIsect, const vector3& wo) const
        {
            return dot(lightIsect.normal, wo) > 0 ? radiance : Color();
        }

    private:
        Color radiance;
        const Shape *shape;
    };
}
#endif //FISHY_LIGHT_H
