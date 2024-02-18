#include "Sphere.h"

namespace Fishy
{

    bool Sphere::Intersect(const Ray &ray, Interaction &isect) const
    {
        vector3 oc = center - ray.origin;
        float neg_b = dot(oc, ray.direction);
        float det = neg_b * neg_b - dot(oc, oc) + radius * radius;

        bool hit = false;
        float distance = 0;
        if (det >= 0)
        {
            float sqrtDet = qSqrt(det);

            float epsilon = 1e-4;
            if (distance = neg_b - sqrtDet; distance > epsilon && distance < isect.distance)
            {
                hit = true;
            }
            else if (distance = neg_b + sqrtDet; distance > epsilon && distance < isect.distance)
            {
                hit = true;
            }
        }

        if (hit)
        {

            Point3 hit_point = ray(distance);
            isect = Interaction(hit_point, (hit_point - center).normalized(), -ray.direction, distance);

        }

        return hit;
    }
}