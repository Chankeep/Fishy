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
    static std::shared_ptr<Material> white;
    static std::shared_ptr<Material> red;
    static std::shared_ptr<Material> blue;
    static std::shared_ptr<Material> grey;
    static std::shared_ptr<Material> mirrorMat;
    static std::shared_ptr<Material> glassMat;


    class Material : public Qt3DRender::QMaterial
    {
    public:
        Material() = default;
        virtual ~Material() = default;

        virtual std::unique_ptr<BSDF> Scattering(const Interaction &isect) const = 0;
        virtual void setParameters() = 0;

        FMaterialType type;

    };

    class MatteMaterial : public Material, public Qt3DExtras::QPhongMaterial
    {
    public:
        MatteMaterial() = default;

        explicit MatteMaterial(const Color &Kd) : Kd(Kd)
        {
            setDiffuse(QColor(Clamp(Kd)));
            setAmbient(QColor(qRgb(25, 25, 25)));
            setShininess(0.2);
            type = FMaterialType::Matte;
        }

        std::unique_ptr<BSDF> Scattering(const Interaction &isect) const override
        {
            return std::make_unique<LambertionReflection>(Frame(isect.normal), Kd);
        }

        void setParameters() override
        {
            auto d = diffuse();
            Kd.setX(d.redF());
            Kd.setY(d.greenF());
            Kd.setZ(d.blueF());
        }


    private:
        Color Kd;

    };

    class MirrorMaterial : public Material, public Qt3DExtras::QMetalRoughMaterial
    {
    public:
        MirrorMaterial(const Color &Kr) :
                Kr{Kr}
        {
            setBaseColor(QColor(Clamp(Kr)));
            setRoughness(0.10);
            setMetalness(0.90);
            type = FMaterialType::Mirror;
        }

        std::unique_ptr<BSDF> Scattering(const Interaction &isect) const override
        {
            return std::make_unique<SpecularReflection>(Frame(isect.normal), Kr);
        }

        void setParameters() override
        {
            auto vColor = this->baseColor();
            auto baseColor = vColor.value<QColor>();
            Kr.setX(baseColor.redF());
            Kr.setY(baseColor.greenF());
            Kr.setZ(baseColor.blueF());
        }

    private:
        Color Kr;
    };

    class GlassMaterial : public Material, public Qt3DExtras::QDiffuseSpecularMaterial
    {
    public:
        GlassMaterial(const Color &Kr, const Color &Kt, double eta) :
                Kr{Kr}, Kt{Kt}, eta{eta}
        {
            setDiffuse(QColor(Clamp(Kr, 0.5)));
            setShininess(0.05);
            setAlphaBlendingEnabled(true);
            type = FMaterialType::Glass;
        }

        std::unique_ptr<BSDF> Scattering(const Interaction &isect) const override
        {
            return std::make_unique<FresnelSpecular>(Frame(isect.normal), Kr, Kt, 1, eta);
        }

        void setParameters() override
        {
            auto v = this->diffuse();
            auto diffuse = v.value<QColor>();
            Kr.setX(diffuse.redF());
            Kr.setY(diffuse.greenF());
            Kr.setZ(diffuse.blueF());
        }

    private:
        Color Kr;
        Color Kt;
        double eta;
    };
}

#endif //FISHY_MATERIAL_H
