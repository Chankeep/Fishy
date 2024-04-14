//
// Created by chankeep on 4/2/2024.
//

#ifndef FISHY_BVH_H
#define FISHY_BVH_H

#include "../core/Fishy.h"
#include "../core/Primitive.h"
#include "../core/FishyShape.h"

namespace Fishy
{

    class BVH : public Primitive
    {
    public:
        BVH() = default;

        BVH(std::vector<std::shared_ptr<Primitive>>& prims) : BVH(prims, 0, prims.size()) {}

        BVH(std::vector<std::shared_ptr<Primitive>>& prims, size_t start, size_t end);

        virtual bool hit(const Ray &ray, double tNear, double tFar, Interaction &isect) const;
        virtual AABB boundingBox() const;
        virtual bool Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const;
        virtual void updateRenderData() const;

        std::shared_ptr<Primitive> leftPrim;
        std::shared_ptr<Primitive> rightPrim;
        AABB box;
    };

} // Fishy

#endif //FISHY_BVH_H
