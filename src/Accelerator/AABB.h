//
// Created by chankeep on 4/2/2024.
//

#ifndef FISHY_AABB_H
#define FISHY_AABB_H

#include "../core/Fishy.h"
namespace Fishy
{

    class AABB
    {
    public:
        AABB()= default;

        AABB(const vector3 &a, const vector3 &b)
        {
            _min = a;
            _max = b;
        }

        vector3 min() const
        {
            return _min;
        }

        vector3 max() const
        {
            return _max;
        }

        bool hit(const Ray &r, double tMin, double tMax) const;


        vector3 _min;
        vector3 _max;

    };

    inline AABB surroundingBox(const AABB& box0, const AABB& box1)
    {
        vector3 smallVec(qMin(box0.min().x(), box1.min().x()),
                qMin(box0.min().y(), box1.min().y()),
                qMin(box0.min().z(), box1.min().z()));
        vector3 bigVec(qMax(box0.max().x(), box1.max().x()),
                qMax(box0.max().y(), box1.max().y()),
                qMax(box0.max().z(), box1.max().z()));
        return AABB(smallVec, bigVec);
    }
}

#endif //FISHY_AABB_H
