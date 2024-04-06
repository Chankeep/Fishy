//
// Created by chankeep on 3/12/2024.
//

#include "Plane.h"

namespace Fishy
{
    Plane::Plane() : width(50), height(50)
    {
        initialize();
    }

    Plane::Plane(const Point3 &p0, const Point3 &p1, const Point3 &p2, const Point3 &p3)
            : p0(p0), p1(p1), p2(p2), p3(p3)
    {
        width = p2.x() - p0.x();
        height = p1.z() - p0.z();
        setWidth(width);
        setHeight(height);
        setMeshResolution(QSize(10, 10));

        auto v1 = p1 - p0;
        auto v2 = p3 - p1;
        normal = cross(v1, v2).normalized();
        center = (p0 + p1 + p2 + p3) / 4;

        t1 = std::make_unique<Triangle>(p0, p1, p2);
        t2 = std::make_unique<Triangle>(p0, p2, p3);
        this->type = FShapeType::FPlane;
    }

    Plane::Plane(const float &width, const float &height) : width(width), height(height)
    {
        setWidth(width);
        setHeight(height);
        setMeshResolution(QSize(10, 10));
        this->type = FShapeType::FPlane;

        auto x = width / 2;
        auto z = height / 2;
        p0 = Point3(-x, 0, -z);
        p1 = Point3(-x, 0, z);
        p2 = Point3(x, 0, z);
        p3 = Point3(x, 0, -z);

        normal = vector3(0, 1, 0);
        t1 = std::make_unique<Triangle>(p0, p1, p2);
        t2 = std::make_unique<Triangle>(p0, p2, p3);
    }

    void Plane::initialize()
    {
        setWidth(50);
        setHeight(50);
        setMeshResolution(QSize(10, 10));
        this->type = FShapeType::FPlane;

        auto x = width / 2;
        auto z = height / 2;
        p0 = Point3(-x, 0, -z);
        p1 = Point3(-x, 0, z);
        p2 = Point3(x, 0, z);
        p3 = Point3(x, 0, -z);

        normal = vector3(0, 1, 0);
        center = (p0 + p1 + p2 + p3) / 4;
        t1 = std::make_unique<Triangle>(p0, p1, p2);
        t2 = std::make_unique<Triangle>(p0, p2, p3);
    }

    bool Plane::Intersect(const Ray &ray, Interaction &isect) const
    {// 计算点积 (光线方向与平面法向量)
//        float numerator = dot(center - ray.origin, normal);
//        float denominator = dot(ray.direction, normal);
//        float t = numerator / denominator;
//        // 如果 t 为负数，说明交点在光线起点的后方
//        if (t < 0) return false;
//
//        // 计算交点坐标
//        vector3 hitPoint = ray(t);
//
//        // 设置交点信息
//        isect = Interaction(hitPoint, normal, -ray.direction, t);
//
//        return true;

        if(!t1 || !t2)
        {
            return false;
        }
        return t1->Intersect(ray, isect) || t2->Intersect(ray, isect);
    }

    AABB Plane::boundingBox() const
    {
        return AABB(p0, p2);
    }

    void Plane::setTransform(Qt3DCore::QTransform *transform)
    {
//        p0 = transform->matrix().map(p0);
//        p1 = transform->matrix().map(p1);
//        p2 = transform->matrix().map(p2);
//        p3 = transform->matrix().map(p3);
//        center = transform->matrix().map(center);
//        normal = transform->matrix().mapVector(normal);

        t1->setTransform(transform);
        t2->setTransform(transform);
    }


} // Fishy