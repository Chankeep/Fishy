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
    class Material : public Qt3DExtras::QPhongMaterial
    {
    public:
        Material() = default;
        virtual ~Material() = default;

        virtual std::unique_ptr<BSDF> Scattering(const Interaction &isect) const = 0;
        virtual void setColor() = 0;

    };

    class MatteMaterial : public Material
    {
    public:
        MatteMaterial() = default;

        explicit MatteMaterial(const Color &Kd) : Kd(Kd)
        {
            setDiffuse(QColor(Clamp(Kd)));
            setAmbient(QColor(qRgb(25,25,25)));
            setShininess(0.2);
        }

        std::unique_ptr<BSDF> Scattering(const Interaction &isect) const override
        {
            return std::make_unique<LambertionReflection>(Frame(isect.normal), Kd);
        }

        void setColor() override
        {
            auto diffuse = this->diffuse();
            Kd.setX(diffuse.redF());
            Kd.setY(diffuse.greenF());
            Kd.setZ(diffuse.blueF());
        }


    private:
        Color Kd;

    };


}

#endif //FISHY_MATERIAL_H
