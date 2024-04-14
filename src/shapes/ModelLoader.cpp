//
// Created by chankeep on 2/19/2024.
//
#include "ModelLoader.h"

namespace Fishy
{

    void ModelLoader::processNode(const aiNode *node, const aiScene *scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[i];
            meshes.emplace_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i], scene);
    }

    std::shared_ptr<TriangleMesh> ModelLoader::processMesh(const aiMesh *mesh, const aiScene *scene)
    {
        auto nVertices = mesh->mNumVertices;//顶点个数
        auto nTriangles = mesh->mNumFaces;//面数
        auto *vertexIndices = new int[nTriangles * 3];//顶点索引
        auto *P = new Point3[nVertices];//顶点坐标

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            //顶点
            P[i].setX(mesh->mVertices[i].x);
            P[i].setY(mesh->mVertices[i].y);
            P[i].setZ(mesh->mVertices[i].z);
        }
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                vertexIndices[3 * i + j] = face.mIndices[j];
        }
        std::shared_ptr<TriangleMesh> trimesh =
                std::make_shared<TriangleMesh>(nTriangles, nVertices, P);

        delete[] vertexIndices;
        delete[] P;
        return trimesh;
    }

    void ModelLoader::buildNoTextureModel(std::vector<std::shared_ptr<Primitive>> &prims, const std::shared_ptr<Material> &material, const std::shared_ptr<Qt3DCore::QTransform> transform)
    {
        std::vector<std::shared_ptr<Triangle>> trisObj;
        for (int i = 0; i < meshes.size(); i++)
        {
            for (int j = 0; j < meshes[i]->nTriangles; ++j)
            {
                std::shared_ptr<TriangleMesh> meshPtr = meshes[i];
                trisObj.push_back(std::make_shared<Triangle>(meshPtr, j));
            }
        }
        //将物体填充到基元
        for (int i = 0; i < trisObj.size(); ++i)
        {
            prims.emplace_back(std::make_shared<GeometricPrimitive>(trisObj[i], material, transform));
        }
        meshes.clear();
    }

    std::shared_ptr<Model> ModelLoader::loadModel(const QString &path, const std::shared_ptr<Material> &material)
    {
        Assimp::Importer importer;
        Assimp::Exporter exporter;
        const aiScene *scene = importer.ReadFile(path.toStdString(),
                aiProcess_Triangulate
                );
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
            return nullptr;
        qDebug() << "successfully loaded model!";
        directory = path;
        processNode(scene->mRootNode, scene);

        auto modelName = path.section('/', -1, -1).section('.', 0, 0);

        auto transform = std::make_shared<Qt3DCore::QTransform>();
        transform->setTranslation(vector3(278,278,278));
        transform->setScale(10);

        auto mesh = std::make_shared<Model>(material, transform);
        mesh->getQMesh()->setSource(QUrl::fromLocalFile(path));
        mesh->setObjectName(modelName);

        buildNoTextureModel(mesh->getPrims(), material, transform);

        return mesh;
    }

}