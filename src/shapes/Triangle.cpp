//
// Created by chankeep on 2/18/2024.
//

#include "Triangle.h"

namespace Fishy
{
    TriangleMesh::TriangleMesh(int numTriangles, int numVertices,
            const int *vertexIndices,
            const Point3 *P) :
            nTriangles(numTriangles),
            nVertices(numVertices),
            vertexIndices(vertexIndices, vertexIndices + 3 * nTriangles)
    {
        p.reset(new Point3[numVertices]);
        for (int i = 0; i < numVertices; i++) p[i] = P[i];
    }

    bool Triangle::Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const
    {
        const double EPSILON = 0.001; // 定义一个足够小的常量
        vector3 E1 = v1 - v0;
        vector3 E2 = v2 - v0;

        // 计算 S1
        const vector3 S1 = cross(ray.direction, E2);
        // 计算行列式
        const double det = dot(E1, S1);
        if (det == 0 || det < 0) return false;

        const double invDet = 1.0 / det;
        // 计算交点的参数
        const vector3 S = ray.origin - v0;
        double b1 = dot(S, S1);
        if (b1 < 0 || b1 > det) return false;

        // 计算 S2
        const vector3 S2 = cross(S, E1);
        double b2 = dot(ray.direction, S2);
        if (b2 < 0 || b1 + b2 > det) return false;

        // 计算 t
        const double t = dot(E2, S2) * invDet;
//        if (t < EPSILON) return false;

        b1 *= invDet;
        b2 *= invDet;

        if (t > isect.tNear)
        {
            return false;
        }
        // 计算法线
        vector3 tempNormal = cross(E1, E2).normalized();
        tempNormal = dot(tempNormal, ray.direction) < 0 ? tempNormal : -tempNormal;
        //更新交点信息
        isect = Interaction(ray(t), tempNormal, -ray.direction, t);
        return true;
    }

    void Triangle::setTransform(Qt3DCore::QTransform *transform)
    {
        auto mat = transform->matrix();
        v0 = mat.map(v0);
        v1 = mat.map(v1);
        v2 = mat.map(v2);

        vector3 pMax, pMin;
        pMax.setX(std::max({v0.x(), v1.x(), v2.x()}));
        pMax.setY(std::max({v0.y(), v1.y(), v2.y()}));
        pMax.setZ(std::max({v0.z(), v1.z(), v2.z()}));

        pMin.setX(std::min({v0.x(), v1.x(), v2.x()}));
        pMin.setY(std::min({v0.y(), v1.y(), v2.y()}));
        pMin.setZ(std::min({v0.z(), v1.z(), v2.z()}));
        box = {pMin, pMax};
    }

    double Triangle::area() const
    {
        return cross(v1 - v0, v2 - v0).length() * 0.5;
    }
} // Fishy