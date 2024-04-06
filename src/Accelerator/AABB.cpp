//
// Created by chankeep on 4/2/2024.
//

#include "AABB.h"
#include "../core/Ray.h"

namespace Fishy
{
    bool AABB::hit(const Ray &r, double tMin, double tMax) const
    {
        for (int a = 0; a < 3; a++)
        {
            auto invD = 1.0f / r.direction[a];
            auto t0 = (min()[a] - r.origin[a]) * invD;
            auto t1 = (max()[a] - r.origin[a]) * invD;
            if (invD < 0.0f)
                std::swap(t0, t1);
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMax <= tMin)
                return false;
        }
        return true;
    }


}