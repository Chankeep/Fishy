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
                const Point3 *P, const vector3 *T,
                const Normal3 *N, const vector2 *uv,
                const int *faceIndices);

        const int nTriangles, nVertices;
        std::vector<int> vertexIndices;
        std::unique_ptr<Point3[]> p;
        std::unique_ptr<Normal3[]> n;
        std::unique_ptr<vector3[]> t;
        std::unique_ptr<vector2[]> uv;
        std::vector<int> faceIndices;
    };

class Triangle : public FishyShape
    {
    public:
        Triangle(const std::shared_ptr<TriangleMesh> mesh, int nTriangles)
                : mesh(mesh)
        {
            v = &mesh->vertexIndices[3 * nTriangles];
            faceIndex = mesh->faceIndices.size() ? mesh->faceIndices[nTriangles] : 0;
        }

        bool Intersect(const Ray& ray, Interaction& isect) const override;
        void setTransform(Qt3DCore::QTransform *transform) override;
    private:
        std::shared_ptr<TriangleMesh> mesh;
        const int *v;
        int faceIndex;
    };

} // Fishy

#endif //FISHY_TRIANGLE_H
