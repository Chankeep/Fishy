//
// Created by chankeep on 2/18/2024.
//

#ifndef FISHY_TRIANGLE_H
#define FISHY_TRIANGLE_H

#include "../core/Fishy.h"
#include "../core/FishyShape.h"


namespace Fishy
{

    struct TriangleMesh
    {
        TriangleMesh(int numTriangles, int numVertices,
                const int *vertexIndices,
                const Point3 *P);

        const int nTriangles, nVertices;
        std::vector<int> vertexIndices;
        std::unique_ptr<Point3[]> p;
    };

    class Triangle : public FishyShape
    {
    public:
        Triangle(const vector3 &v0 = {}, const vector3 &v1 = {}, const vector3 &v2 = {})
                : v0(v0), v1(v1), v2(v2)
        {
        }

        Triangle(const std::shared_ptr<TriangleMesh> mesh, int nTriangles)
                : mesh(mesh)
        {
            v = &mesh->vertexIndices[3 * nTriangles];
            v0 = mesh->p[v[0]];
            v1 = mesh->p[v[1]];
            v2 = mesh->p[v[2]];
        }

        virtual bool Intersect(const Ray &ray, double tNear, double tFar, Interaction &isect) const override;
        virtual void setTransform(Qt3DCore::QTransform *transform) override;
        virtual double area() const override;

    private:
        std::shared_ptr<TriangleMesh> mesh;
        const int *v;

        vector3 v0, v1, v2;
    };

} // Fishy

#endif //FISHY_TRIANGLE_H
