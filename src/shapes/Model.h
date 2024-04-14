//
// Created by chankeep on 4/8/2024.
//

#ifndef FISHY_MODEL_H
#define FISHY_MODEL_H

#include "../core/Fishy.h"
#include "../core/Primitive.h"

namespace Fishy
{

    class Model : public Qt3DCore::QEntity
    {
    public:
        Model();
        Model(const std::shared_ptr<Material>& material, const std::shared_ptr<Qt3DCore::QTransform>& transform);
        ~Model() override = default;

        void setMaterial(Material *mat);

        std::shared_ptr<Qt3DRender::QMesh>& getQMesh();
        std::vector<std::shared_ptr<Primitive>>& getPrims();

    private:
        std::shared_ptr<Qt3DRender::QMesh> mesh;
        std::shared_ptr<Qt3DCore::QTransform> transform;
        std::shared_ptr<Qt3DRender::QMaterial> material;
        std::vector<std::shared_ptr<Primitive>> prims;
    };

} // Fishy

#endif //FISHY_MODEL_H
