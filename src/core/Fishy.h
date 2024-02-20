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
    class Scene;
    class Integrator;
    class Ray;
    class Transform;
    struct Interaction;
    class FishyShape;
    class Sphere;
    class Primitive;
    class GeometricPrimitive;
    class Camera;
    struct CameraSample;
    class Sampler;
    class RandomSamper;
    class TrapezoidalSamper;
    class Film;
    class BSDF;
    class Material;
    class MatteMaterial;
    class Light;
    class ModelLoader;
}
