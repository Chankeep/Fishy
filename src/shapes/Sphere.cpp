#include "Sphere.h"

namespace fishy
{

    bool Sphere::Intersect(Ray &r, Interaction &isect) const
    {
        const vector3 oc = center - r.origin;
        const double neg_b = dot(oc, r.direction);
        double det = neg_b * neg_b - dot(oc, oc) + radius * radius;

        if (det < 0)
            return false;

        det = qSqrt(det);
        bool isHit = false;
        double t = 0.0;
        constexpr double epsilon = 1e-4;

        if (t = neg_b - det; t > epsilon && t < r.distance)
            isHit = true;
        else if (t = neg_b + det; t > epsilon && t < r.distance)
            isHit = true;

        if (isHit)
        {
            r.distance = t;

            vector3 hitPoint = r(t);
            isect = Interaction(hitPoint, (hitPoint - center).normalized(), -r.direction);
            isect.bsdf = std::make_unique<LambertionReflection>(Frame(isect.normal), vector3(0.3 / t, 0.3 / t, 0.3 / t));
        }

        return isHit;
    }
}