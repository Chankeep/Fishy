#pragma once

#include "Geometry.h"

#include <QImage>

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
    class Shape;
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
}
