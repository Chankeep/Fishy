#include "Sphere.h"

namespace Fishy
{

    bool Sphere::Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const
    {
        vector3 ray_to_center = origin - ray.origin;
        double t0 = dot(ray_to_center, ray.direction); // 射线与方向向量的点积

        // 计算判别式
        double det = t0 * t0 - dot(ray_to_center, ray_to_center) + radius * radius;

        // 如果判别式小于0，则没有交点
        if (det < 0) return false;

        // 计算平方根的判别式
        double sqrtDet = std::sqrt(det);

        // 计算两个可能的交点距离
        double t1 = t0 - sqrtDet;
        double t2 = t0 + sqrtDet;

        // 找到最小的有效交点距离
        double distance = std::min(t1, t2);
        constexpr double epsilon = 1e-4;
        if (distance > epsilon && distance < isect.tNear) {
            // 计算交点
            Point3 hit_point = ray(distance);
            vector3 normal = (hit_point - origin).normalized();

            // 判断法线方向
            bool front_face = dot(ray.direction, normal) < 0;
            vector3 shading_normal = front_face ? normal : -normal;

            // 更新交点信息
            isect = Interaction(hit_point, shading_normal, -ray.direction, distance);

            return true;
        }
        return false;

    }


    void Sphere::setTransform(Qt3DCore::QTransform *transform)
    {
        radius *= transform->scale();
        origin = transform->matrix().map(origin);
        auto rvec = vector3(radius, radius, radius);
        box = {origin - rvec, origin + rvec};
    }

    double Sphere::area() const
    {
        return 4.0 * radius * radius * Pi;
    }
}