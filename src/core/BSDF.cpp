#include "BSDF.h"


namespace Fishy
{
    vector3 BSDF::ToWorld(const vector3 &v) const
    {
        return shadingFrame.ToWorld(v);
    }

    vector3 BSDF::ToLocal(const vector3 &v) const
    {
        return shadingFrame.ToLocal(v);
    }

    vector3 BSDF::f(const vector3 &world_wo, const vector3 &world_wi) const {
        return f_(ToLocal(world_wo), ToLocal(world_wi));
    }

    BSDFSample BSDF::sample(const vector3 &wo, const vector2 &random) const
    {
        auto res = sample_(ToLocal(wo), random);
        res.wi = ToWorld(res.wi);
        return res;
    }

    double BSDF::pdf(const vector3 &world_wo, const vector3 &world_wi) const
    {
        return pdf_(ToLocal(world_wo), ToLocal(world_wi));
    }

    vector3 LambertionReflection::f_(const vector3 &wo, const vector3 &wi) const
    {
        return albedo * InvPi;
    }

    double LambertionReflection::pdf_(const vector3 &wo, const vector3 &wi) const
    {
        return wo.z() * wi.z() > 0 ? qAbs(wi.z()) * InvPi : 0;
    }

    BSDFSample LambertionReflection::sample_(const vector3 &wo, const vector2 &random) const
    {
        BSDFSample res;

        res.wi = CosineSampleHemisphere(random);
        if(wo.z() < 0)
            res.wi.setZ(res.wi.z() * -1);
        res.pdf = pdf_(wo, res.wi);
        res.f = f_(wo, res.wi);

        return res;
    }

    vector3 SpecularReflection::f_(const vector3 &wo, const vector3 &wi) const { return Color(); }

    double SpecularReflection::pdf_(const vector3 &wo, const vector3 &wi) const { return 0; }

    BSDFSample SpecularReflection::sample_(const vector3 &wo, const vector2 &random) const {
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#SpecularReflection
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.h#L387-L408
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.cpp#L181-L191

        BSDFSample sample;
        sample.wi = vector3(-wo.x(), -wo.y(), wo.z());
        sample.pdf = 1;
        sample.f = R / qAbs(sample.wi.z()); // for `(R / cos_theta) * Li * cos_theta / pdf = R * Li`

        return sample;
    }

    vector3 FresnelSpecular::f_(const vector3 &wo, const vector3 &wi) const { return Color(); }

    double FresnelSpecular::pdf_(const vector3 &wo, const vector3 &wi) const { return 0; }

    BSDFSample FresnelSpecular::sample_(const vector3 &wo, const vector2 &random) const {
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#Fresnelalbedo
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.h#L440-L463
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.cpp#L627-L667

        BSDFSample sample;

        Normal3 normal(0, 0, 1); // use `z` as normal
        bool into = dot(normal, wo) > 0; // ray from outside going in?

        Normal3 woNormal = into ? normal : normal * -1;
        // IOR(index of refractive)
        float eta = into ? etaI / etaT : etaT / etaI;


        // compute reflect direction by refection law
        vector3 reflectDirection = vector3(-wo.x(), -wo.y(), wo.z());

        // compute refract direction by Snell's law
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#SpecularTransmission see `Refract()`
        float cosThetaI = dot(wo, woNormal);
        float cosThetaT2 = 1 - eta * eta * (1 - cosThetaI * cosThetaI);
        if (cosThetaT2 < 0) // Total internal reflection
        {
            return sample;
        }
        float cosThetaT = sqrt(cosThetaT2);
        vector3 refractDirection = (-wo * eta + woNormal * (cosThetaI * eta - cosThetaT)).normalized();


        // compute the fraction of incoming light that is reflected or transmitted
        // by Schlick Approximation of Fresnel Dielectric 1994 https://en.wikipedia.org/wiki/Schlick%27s_approximation
        float a = etaT - etaI;
        float b = etaT + etaI;
        float R0 = a * a / (b * b);
        float c = 1 - (into ? cosThetaI : cosThetaT);

        float Re = R0 + (1 - R0) * c * c * c * c * c;
        float Tr = 1 - Re;


        if (random.x() < Re) // Russian roulette
        {
            // Compute specular reflection for _FresnelSpecular_

            sample.wi = reflectDirection;
            sample.pdf = Re;
            sample.f = (R * Re) / qAbs(sample.wi.z());
        }
        else
        {
            // Compute specular transmission for _FresnelSpecular_

            sample.wi = refractDirection;
            sample.pdf = Tr;
            sample.f = (T * Tr) / qAbs(sample.wi.z());
        }

        return sample;
    }
}