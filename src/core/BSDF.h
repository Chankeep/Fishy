﻿#pragma once

#include "Frame.h"


namespace Fishy
{
    struct BSDFSample
    {
        vector3 f; // scattering rate
        vector3 wi; // world wi
        double pdf{};
    };

    class BSDF
    {
    public:
        BSDF() = default;
        virtual ~BSDF() = default;

        explicit BSDF(const Frame &shadingFrame) : shadingFrame(shadingFrame)
        {
        }

        vector3 f(const vector3 &world_wo, const vector3 &world_wi) const;
        double pdf(const vector3 &world_wo, const vector3 &world_wi) const;
        virtual BSDFSample sample(const vector3 &wo, const vector3& normal, const vector2 &random) const;

    protected:
        virtual vector3 f_(const vector3 &wo, const vector3 &wi) const = 0;
        virtual double pdf_(const vector3 &wo, const vector3 &wi) const = 0;
        virtual BSDFSample sample_(const vector3 &wo, const vector3& normal, const vector2 &random) const = 0;

    private:
        vector3 ToLocal(const vector3 &v) const;
        vector3 ToWorld(const vector3 &v) const;

        Frame shadingFrame;
    };

    class LambertionReflection : public BSDF
    {
    public:
        LambertionReflection() = default;
        LambertionReflection(const Frame& shadingFrame, const vector3& albedo) : BSDF(shadingFrame), albedo(albedo){}

        vector3 f_(const vector3 &wo, const vector3 &wi) const override;
        double pdf_(const vector3 &wo, const vector3 &wi) const override;
        BSDFSample sample_(const vector3 &wo, const vector3& normal, const vector2 &random) const override;

    private:
        vector3 albedo;
    };

    class SpecularReflection : public BSDF
    {
    public:
        SpecularReflection(const Frame& shadingFrame, const Color& R) :
                BSDF(shadingFrame), R{ R }
        {
        }

        vector3 f_(const vector3& wo, const vector3& wi) const override;
        double pdf_(const vector3& wo, const vector3& wi) const override;

        BSDFSample sample_(const vector3 &wo, const vector3& normal, const vector2 &random) const override;

    private:
        Color R;
    };

    class FresnelSpecular : public BSDF
    {
    public:
        FresnelSpecular(const Frame& shadingFrame, const Color& R, const Color& T, double etaI, double etaT) :
                BSDF(shadingFrame), R{ R }, T{ T }, etaI{ etaI }, etaT{ etaT }
        {
        }

        vector3 f_(const vector3& wo, const vector3& wi) const override;
        double pdf_(const vector3& wo, const vector3& wi) const override;

        BSDFSample sample_(const vector3& wo, const vector3& normal, const vector2& random) const override;

    private:
        Color R;
        Color T;
        double etaI;
        double etaT;
    };

}