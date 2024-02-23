//
// Created by chankeep on 12/20/2023.
//

#ifndef FISHY_LIGHT_H
#define FISHY_LIGHT_H

#include "../core/Fishy.h"
#include "../core/FishyShape.h"

namespace Fishy
{
    class Light : public Qt3DRender::QPointLight
    {
    public:
        Light() = default;

        Light(Color radiance, const FishyShape *shape)
        : radiance(radiance), shape(shape)
        {
            setColor(color2QColor(radiance.normalized()));
            setIntensity(1.8);
        }

        virtual ~Light() = default;

        Color Le(const Interaction &lightIsect, const vector3 &wo) const
        {
            return dot(lightIsect.normal, wo) > 0 ? radiance : Color();
        }

    private:
        Color radiance;
        const FishyShape *shape;
    };
}
#endif //FISHY_LIGHT_H
