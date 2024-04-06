//
// Created by chankeep on 4/4/2024.
//
#include "Fishy.h"
#include "Scene.h"

namespace Fishy
{

    shared_ptr<Qt3DCore::QTransform> createTransform(const vector3 &translation, const vector3 &rotation, const vector3 &scale) {
        auto res = make_shared<Qt3DCore::QTransform>();
        auto quaternion = Qt3DCore::QTransform::fromEulerAngles(rotation);
        res->setRotation(quaternion);
        res->setTranslation(translation);
        res->setScale3D(scale);
        return res;
    }


}