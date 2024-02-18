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
}