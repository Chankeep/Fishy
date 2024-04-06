//
// Created by chankeep on 2/18/2024.
//

#include "Triangle.h"

namespace Fishy
{
    TriangleMesh::TriangleMesh(int numTriangles, int numVertices,
            const int* vertexIndices,
            const Point3 *P, const vector3 *T, const Normal3 *N,
            const vector2 *UV, const int *faceIndices): nTriangles(numTriangles),
            nVertices(numVertices),
            vertexIndices(vertexIndices, vertexIndices + 3 * nTriangles)
    {
        p.reset(new Point3[numVertices]);
        for(int i=0;i<numVertices;i++) p[i] = P[i];

        if(UV)
        {
            uv.reset(new vector2[numVertices]);
            memcpy(uv.get(), UV, numVertices * sizeof(vector2));
        }
        if(N)
        {
            n.reset(new Normal3[numVertices]);
            for(int i=0; i<numVertices;i++) n[i] = N[i];
        }
        if(T)
        {
            n.reset(new vector3[numVertices]);
            for(int i=0; i<numVertices;i++) t[i] = T[i];
        }
        if(faceIndices)
            this->faceIndices = std::vector<int>(faceIndices, faceIndices + numTriangles);
    }

    bool Triangle::Intersect(const Ray &ray, Interaction &isect) const
    {

        const float EPSILON = 1e-5f; // 定义一个足够小的常量
        vector3 E1 = v1 - v0;
        vector3 E2 = v2 - v0;

        // 计算 P
        const vector3 P = cross(ray.direction, E2);
        // 计算行列式
        const double det = dot(E1, P);
        if (fabs(det) < EPSILON) return false;
        // 计算交点的参数
        const double invDet = 1.0 / det;
        const vector3 T = ray.origin - v0;
        const double u = invDet * dot(T, P);
        if (u < 0 || u > 1) return false;
        // 计算 Q
        const vector3 Q = cross(T, E1);
        const double v = dot(ray.direction, Q) * invDet;
        if (v < 0 || u + v > 1) return false;
        // 计算 t
        const double t = dot(E2, Q) * invDet;
        if (t < EPSILON) return false;
        if(t < isect.distance)
        {
            // 计算法线
            vector3 tempNormal = cross(E2, E1).normalized();
            if (dot(tempNormal, ray.direction) > 0) {
                tempNormal *= -1;
            }
            // 更新交点信息
            isect = Interaction(ray(t), tempNormal, -ray.direction, t);
            return true;
        }
        return false;


    }

    AABB Triangle::boundingBox() const
    {
        return AABB();
    }

    void Triangle::setTransform(Qt3DCore::QTransform *transform)
    {
        auto mat = transform->matrix();
        v0 = mat.map(v0);
        v1 = mat.map(v1);
        v2 = mat.map(v2);
    }
} // Fishy