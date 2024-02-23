//
// Created by chankeep on 2/19/2024.
//
#include "ModelLoader.h"
#include <Qt3DRender/QMesh>

namespace Fishy
{

    void ModelLoader::processNode(aiNode *node, const aiScene *scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[i];
            meshes.emplace_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i], scene);
    }

    std::shared_ptr<TriangleMesh> ModelLoader::processMesh(aiMesh *mesh, const aiScene *scene)
    {

        long nVertices = mesh->mNumVertices;
        long nTriangles = mesh->mNumFaces;
        int *vertexIndices = new int[nTriangles * 3];
        auto *P = new Point3[nVertices];
        vector3 *S = nullptr;// new Vector3f[nVertices];
        auto *N = new Normal3[nVertices];
        auto *uv = new vector2[nVertices];
        int *faceIndices = nullptr; //第几个面的编号

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            //顶点
            P[i].setX(mesh->mVertices[i].x);
            P[i].setY(mesh->mVertices[i].y);
            P[i].setZ(mesh->mVertices[i].z);
            //法向量
            if (mesh->HasNormals())
            {
                N[i].setX(mesh->mNormals[i].x);
                N[i].setY(mesh->mNormals[i].y);
                N[i].setZ(mesh->mNormals[i].z);
            }
            //纹理和切线向量
            if (mesh->mTextureCoords[0])
            {
                uv[i].setX(mesh->mTextureCoords[0][i].x);
                uv[i].setY(mesh->mTextureCoords[0][i].y);
            }
        }
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                vertexIndices[3 * i + j] = face.mIndices[j];
        }
        if (!mesh->HasNormals())
        {
            delete[] N;
            N = nullptr;
        }
        if (!mesh->mTextureCoords[0])
        {
            delete[] uv;
            uv = nullptr;
        }
        std::shared_ptr<TriangleMesh> trimesh =
                std::make_shared<TriangleMesh>(nTriangles, nVertices, vertexIndices, P, S, N, uv, faceIndices);

        delete[] vertexIndices;
        delete[] P;
        delete[] S;
        delete[] N;
        delete[] uv;
        return trimesh;
    }

    void ModelLoader::buildNoTextureModel(std::vector<std::shared_ptr<Primitive>> &prims, const std::shared_ptr<Material> &material)
    {
        std::vector<std::shared_ptr<FishyShape>> trisObj;
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
            prims.emplace_back(std::make_shared<GeometricPrimitive>(trisObj[i], material, vector3(0,0,0)));
        }
        meshes.clear();
    }

    void ModelLoader::loadModel(const QString& path)
    {
        Assimp::Importer importer;
        Assimp::Exporter exporter;
        const aiScene *scene = importer.ReadFile(path.toStdString(), aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);
//        auto mesh = new Qt3DRender::QMesh();
//        mesh->setSource(path);
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode )
            return;
        qDebug() << "successfully loaded model!";
        if(path.section('.', -1, -1) != QString("obj"))
        {
            exporter.Export(scene, "obj", "./temp.obj");
        }
        directory = path.section('/', -1, -1);
        processNode(scene->mRootNode, scene);

    }

}