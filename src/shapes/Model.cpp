//
// Created by chankeep on 4/8/2024.
//

#include "Model.h"

namespace Fishy
{
    Model::Model()
    {
        mesh = std::make_shared<Qt3DRender::QMesh>();
        addComponent(mesh.get());
    }

    Model::Model(const std::shared_ptr<Material> &material, const std::shared_ptr<Qt3DCore::QTransform> &transform)
        : material(material), transform(transform)
    {
        setMaterial(material.get());
        addComponent(transform.get());
        mesh = std::make_shared<Qt3DRender::QMesh>();
        addComponent(mesh.get());
    }

    std::vector<std::shared_ptr<Primitive>> &Model::getPrims()
    {
        return prims;
    }

    std::shared_ptr<Qt3DRender::QMesh> &Model::getQMesh()
    {
        return mesh;
    }

    void Model::setMaterial(Material *mat)
    {
        if(mat->type == FMaterialType::Matte)
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QPhongMaterial*>(mat);
            addComponent(castMat);
        }

        if(mat->type == FMaterialType::Mirror)
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QMetalRoughMaterial*>(mat);
            addComponent(castMat);
        }

        if(mat->type == FMaterialType::Glass)
        {
            auto *castMat = dynamic_cast<Qt3DExtras::QDiffuseSpecularMaterial*>(mat);
            addComponent(castMat);
        }
    }


} // Fishy