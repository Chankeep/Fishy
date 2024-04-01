#pragma once

#include "Geometry.h"
#include "omp.h"

#include <QImage>
#include <Qt3DCore>
#include <Qt3DExtras/Qt3DExtras>
#include <Qt3DRender/Qt3DRender>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>
#include <utility>

namespace Fishy
{
    enum FMaterialType
    {
        Matte,
        Mirror,
        Glass,
        Glossy
    };

    enum FShapeType
    {
        FPlane,
        FSphere,
        FTriangle
    };

    class Scene;
    class Integrator;
    class Ray;
    class Transform;
    struct Interaction;
    class FishyShape;
    class Plane;
    class Sphere;
    class Triangle;
    class Primitive;
    class GeometricPrimitive;
    class Camera;
    class PerspectiveCamera;
    struct CameraSample;
    class Sampler;
    class RandomSamper;
    class TrapezoidalSamper;
    class Film;
    class BSDF;
    class Material;
    class MatteMaterial;
    class MirrorMaterial;
    class GlassMaterial;
    class Light;
    class PointLight;
    class AreaLight;
    class ModelLoader;
    class SceneManager;

}
