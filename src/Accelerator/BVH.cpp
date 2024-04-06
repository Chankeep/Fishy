//
// Created by chankeep on 4/2/2024.
//

#include "BVH.h"
#include <random>

namespace Fishy
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 2);

    inline bool box_compare(const std::shared_ptr<Primitive> a, const std::shared_ptr<Primitive> b, int axis) {
        AABB box_a = a->boundingBox();
        AABB box_b = b->boundingBox();

        return box_a.min()[axis] < box_b.min()[axis];
    }


    bool box_x_compare (const std::shared_ptr<Primitive> a, const std::shared_ptr<Primitive> b) {
        return box_compare(a, b, 0);
    }

    bool box_y_compare (const std::shared_ptr<Primitive> a, const std::shared_ptr<Primitive> b) {
        return box_compare(a, b, 1);
    }

    bool box_z_compare (const std::shared_ptr<Primitive> a, const std::shared_ptr<Primitive> b) {
        return box_compare(a, b, 2);
    }

    BVH::BVH(std::vector<std::shared_ptr<Primitive>>& prims, size_t start, size_t end)
    {
        int axis = dis(gen);
        auto comparator = (axis == 0) ? box_x_compare
                                      : (axis == 1) ? box_y_compare
                                                    : box_z_compare;

        size_t object_span = end - start;

        if (object_span == 1) {
            leftPrim = rightPrim = prims[start];
        } else if (object_span == 2) {
            if (comparator(prims[start], prims[start+1])) {
                leftPrim = prims[start];
                rightPrim = prims[start+1];
            } else {
                leftPrim = prims[start+1];
                rightPrim = prims[start];
            }
        } else {
            std::sort(prims.begin() + start, prims.begin() + end, comparator);

            auto mid = start + object_span/2;
            leftPrim = make_shared<BVH>(prims, start, mid);
            rightPrim = make_shared<BVH>(prims, mid, end);
        }

        AABB box_left = leftPrim->boundingBox();
        AABB box_right = rightPrim->boundingBox();

        box = surroundingBox(box_left, box_right);
    }

    AABB BVH::boundingBox() const
    {
        return box;
    }

    bool BVH::hit(const Ray &ray, double tMin, double tMax, Interaction &isect) const
    {
        if (!box.hit(ray, tMin, tMax))
            return false;

        bool hitLeft = leftPrim->hit(ray, tMin, tMax, isect);
        bool hitRight = rightPrim->hit(ray, tMin, hitLeft ? isect.distance : tMax, isect);

        return hitLeft || hitRight;
    }

    bool BVH::Intersect(const Ray &ray, Interaction &isect) const{
        return false;
    }

    void BVH::updateRenderData() const
    {
        return;
    }
} // Fishy