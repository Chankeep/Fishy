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
        vector3 E1 = v2 - v1;
        vector3 E2 = v3 - v1;
        const vector3 P = cross(ray.direction, E2);
        const double det = dot(E1, P);
        if (fabs(det) < 1e-5) return false;
        const double invS1E1 = 1. / det;
        const vector3 T = ray.origin - v1;
        const double u = invS1E1 * dot(T, P);
        if (u < 0 || u > 1) return false;
        const vector3 Q = cross(T, E1);
        const double v = dot(ray.direction, Q) * invS1E1;
        if (v < 0 || u + v > 1) return false;
        const double t = dot(E2, Q) * invS1E1;
        if (t < 0)
            return false;
        vector3 hit_point = ray(t);
        if (t > isect.distance)
            return false;
        vector3 tempNormal = cross(E2, E1).normalized();
        if (dot(tempNormal, ray.direction) > 0.0001)
            tempNormal *= -1;
        isect = Interaction(hit_point, tempNormal, -ray.direction, t);
        return true;
    }

    void Triangle::setTransform(Qt3DCore::QTransform *transform)
    {

    }
} // Fishy