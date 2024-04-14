//
// Created by chankeep on 2/19/2024.
//

#ifndef FISHY_MODELLOADER_H
#define FISHY_MODELLOADER_H

#include "../core/Fishy.h"
#include "../core/Primitive.h"
#include "Model.h"
#include "Triangle.h"
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Fishy
{

    class ModelLoader
    {
        QString directory;
        std::vector<std::shared_ptr<TriangleMesh>> meshes;
    public:

        std::shared_ptr<Model> loadModel(const QString &path, const std::shared_ptr<Material> &material);
        void processNode(const aiNode *node, const aiScene *scene);
        std::shared_ptr<TriangleMesh> processMesh(const aiMesh *mesh, const aiScene *scene);
        void buildNoTextureModel(std::vector<std::shared_ptr<Primitive>> &prims, const std::shared_ptr<Material> &material, const std::shared_ptr<Qt3DCore::QTransform> transform);
    };

} // Fishy

#endif //FISHY_MODELLOADER_H
