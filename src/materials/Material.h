//
// Created by chankeep on 12/20/2023.
//

#ifndef FISHY_MATERIAL_H
#define FISHY_MATERIAL_H

#include "../core/Fishy.h"
#include "../core/BSDF.h"
#include "../core/Interaction.h"

namespace Fishy
{
    class Material
    {
    public:
        Material() = default;

        virtual ~Material() = default;

        virtual std::unique_ptr<BSDF> Scattering(const Interaction &isect) const = 0;

    };

    class MatteMaterial : public Material
    {
    public:
        MatteMaterial() = default;
        explicit MatteMaterial(const Color& Kd) : Kd(Kd) {}

        std::unique_ptr<BSDF> Scattering(const Interaction &isect) const override
        {
            return std::make_unique<LambertionReflection>(Frame(isect.normal), Kd);
        }
    private:
        Color Kd;

    };
}
#endif //FISHY_MATERIAL_H
