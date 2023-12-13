#include "BSDF.h"


namespace fishy
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

    vector3 BSDF::pdf(const vector3 &world_wo, const vector3 &world_wi) const
    {
        return pdf_(ToLocal(world_wo), ToLocal(world_wi));
    }

    vector3 LambertionReflection::f_(const vector3 &wo, const vector3 &wi) const
    {
        return albedo * InvPi;
    }

    vector3 LambertionReflection::pdf_(const vector3 &wo, const vector3 &wi) const
    {
        return
    }

    BSDFSample LambertionReflection::sample_(const vector3 &wo, const vector2 &random) const
    {
        BSDFSample res;


    }
}