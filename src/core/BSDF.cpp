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

    vector3 BSDF::f(const vector3 &world_wo, const vector3 &world_wi) const
    {
        return f_(ToLocal(world_wo), ToLocal(world_wi));
    }

    BSDFSample BSDF::sample(const vector3 &wo, const vector3 &normal, const vector2 &random) const
    {
        auto res = sample_(wo, normal, random);
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

    BSDFSample LambertionReflection::sample_(const vector3 &wo, const vector3 &normal, const vector2 &random) const
    {
        BSDFSample sample;

        const double random1 = 2 * Pi * random[0];
        const double random2 = random[1];
        const double random2Sqrt = sqrt(random2);
        //BRDF Render Equation

        vector3 w = normal;
        vector3 u = cross((fabs(w.x()) > .1 ? vector3(0, 1, 0) : vector3(1, 0, 0)), w).normalized();
        vector3 v = cross(w, u);

        sample.wi = (u * cos(random1) * random2Sqrt + v * sin(random1) * random2Sqrt + w * sqrt(1 - random2)).normalized();
        sample.pdf = qAbs(dot(sample.wi, normal)) / Pi;
        sample.f = albedo / Pi;

        return sample;
    }

    vector3 SpecularReflection::f_(const vector3 &wo, const vector3 &wi) const
    {
        return Color();
    }

    double SpecularReflection::pdf_(const vector3 &wo, const vector3 &wi) const
    {
        return 0;
    }

    BSDFSample SpecularReflection::sample_(const vector3 &wo, const vector3 &normal, const vector2 &random) const
    {
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#SpecularReflection
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.h#L387-L408
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.cpp#L181-L191

        BSDFSample sample;
        sample.wi = -wo - 2 * dot(-wo, normal) * normal;
        sample.pdf = 1;
        sample.f = R / qAbs(dot(sample.wi, normal));
        return sample;

//        const double random1 = 2 * Pi * random[0];
//        const double random2 = rng_.random_double01();
//        const double random2Sqrt = sqrt(random2);
//        //BRDF Render Equation
//        Vector3 w = inf.normal;
//        Vector3 u = ((fabs(w.x) > .1 ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).Cross(w)).Normalize();
//        Vector3 v = w.Cross(u);
//
//        const Vector3 wi_direction = (u * cos(random1) * random2Sqrt + v * sin(random1) * random2Sqrt + w * sqrt(1 - random2)).Normalize();
    }

    vector3 FresnelSpecular::f_(const vector3 &wo, const vector3 &wi) const
    {
        return Color();
    }

    double FresnelSpecular::pdf_(const vector3 &wo, const vector3 &wi) const
    {
        return 0;
    }

    BSDFSample FresnelSpecular::sample_(const vector3 &wo, const vector3 &normal, const vector2 &random) const
    {
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#Fresnelalbedo
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.h#L440-L463
        // https://github.com/infancy/pbrt-v3/blob/master/src/core/reflection.cpp#L627-L667

        BSDFSample sample;

        bool into = dot(normal, wo) > 0; // ray from outside going in?

        // IOR(index of refractive)
        float eta = into ? etaI / etaT : etaT / etaI;

        // compute reflect direction by refection law
        vector3 reflectDirection = -wo - 2 * dot(-wo, normal) * normal;

        // compute refract direction by Snell's law
        // https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission#SpecularTransmission see `Refract()`
        float cosThetaI = dot(wo, normal);
        float sinThetaT2 = qMax(0., 1. - cosThetaI * cosThetaI);
        sinThetaT2 *= eta * eta;
        if (sinThetaT2 >= 1) // Total internal reflection
        {
            return sample;
        }
        float cosThetaT = qSqrt(1 - sinThetaT2);
        vector3 refractDirection = (-wo * eta + normal * (cosThetaI * eta - cosThetaT));

        // compute the fraction of incoming light that is reflected or transmitted
        // by Schlick Approximation of Fresnel Dielectric 1994 https://en.wikipedia.org/wiki/Schlick%27s_approximation
        float a = etaT - etaI;
        float b = etaT + etaI;
        float R0 = a / b;
        R0 *= R0;
        float c = 1 - (into ? cosThetaI : cosThetaT);

        float Re = R0 + (1 - R0) * c * c * c * c * c;
        float Tr = 1 - Re;

        float P = .25 + .5 * Re;
        float RP = Re / P;
        float TP = Tr / (1 - P);

        if (random.x() < P) // Russian roulette
        {
            // Compute specular reflection for _FresnelSpecular_

            sample.wi = reflectDirection;
            sample.pdf = 1;
            sample.f = (R * RP) / qAbs(dot(sample.wi, normal));
        }
        else
        {
            // Compute specular transmission for _FresnelSpecular_

            sample.wi = refractDirection;
            sample.pdf = 1;
            sample.f = (T * TP) / qAbs(dot(sample.wi, normal));
        }

        return sample;
    }
}