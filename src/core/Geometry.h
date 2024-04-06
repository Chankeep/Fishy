#pragma once

#include "Fishy.h"

#include <QColor>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include <random>
#include <numbers>

namespace Fishy
{

    constexpr double Infinity = std::numeric_limits<double>::infinity();
    constexpr double Pi = std::numbers::pi;
    constexpr double InvPi = std::numbers::inv_pi;

    using vector2 = QVector2D;
    using vector3 = QVector3D;
    using vector4 = QVector4D;
    using Color = QVector3D;
    using Normal3 = QVector3D;
    using Point3 = QVector3D;

    inline float dot(vector2 v1, vector2 v2)
    {
        return vector2::dotProduct(v1, v2);
    }

    inline float dot(vector3 v1, vector3 v2)
    {
        return vector3::dotProduct(v1, v2);
    }

    inline float dot(vector4 v1, vector4 v2)
    {
        return vector4::dotProduct(v1, v2);
    }

    inline vector3 cross(vector3 v1, vector3 v2)
    {
        return vector3::crossProduct(v1, v2);
    }

    using matrix2x2 = QMatrix2x2;
    using matrix3x3 = QMatrix3x3;
    using matrix4x4 = QMatrix4x4;

    // inline vector3 operator*(const vector3& v, float t)
    // {
    // 	return { v.x() * t, v.y() * t, v.z() * t };
    // }
    //
    // inline vector3 operator*(float t, const vector3& v)
    // {
    // 	return v * t;
    // }

    inline float Clamp(float f)
    {
        if(f > 1.f)
            return 1.f;
        else if(f < 0.f)
            return 0.f;
        return f;
    }

    inline bool isBlack(vector3 v)
    {
        return v.x() == 0.f && v.y() == 0.f && v.z() == 0.f;
    }

    inline QColor color2QColor(const Color& color)
    {
        QColor res = {static_cast<int>(color.x() * 255), static_cast<int>(color.y() * 255), static_cast<int>(color.z() * 255)};
        return res;
    }

    inline float linearToGamma(float x)
    {
        return qSqrt(x);
    }

    inline QRgb Gamma(const vector3& c)
    {
        QColor color = {static_cast<int>(Clamp(linearToGamma(c.x())) * 255), static_cast<int>(Clamp((linearToGamma(c.y()))) * 255), static_cast<int>(Clamp((linearToGamma(c.z()))) * 255)};
        auto rgb = color.rgb();
        return rgb;
    }

    inline QRgb Clamp(const vector3 &v)
    {
        QColor color = {static_cast<int>(Clamp(v.x()) * 255), static_cast<int>(Clamp(v.y()) * 255), static_cast<int>(Clamp(v.z()) * 255)};
        auto rgb = color.rgb();
        return rgb;
    }

    inline QRgba64 Clamp(const vector3 &v, float a)
    {
        QColor color = {static_cast<int>(Clamp(v.x()) * 255), static_cast<int>(Clamp(v.y()) * 255), static_cast<int>(Clamp(v.z()) * 255), static_cast<int>(Clamp(a) * 255)};
        auto rgba = color.rgba64();
        return rgba;
    }

    inline vector2 UniformSampleDisk(const vector2 &random)
    {
        float radius = qSqrt(random[0]);
        float theta = 2 * Pi * random[1];
        return {radius * qCos(theta), radius * qSin(theta)};
    }

    inline vector3 CosineSampleHemisphere(const vector2 &random)
    {
        vector2 pDisk = UniformSampleDisk(random);
        float z = qSqrt(qMax(0.f, 1 - pDisk.x() * pDisk.x() - pDisk.y() * pDisk.y()));
        return {pDisk.x(), pDisk.y(), z};
    }

    inline float GetMaxNum(const vector3 &v)
    {
        if (v.x() > v.y())
        {
            return v.x() > v.z() ? v.x() : v.z();
        }
        return v.y() > v.z() ? v.y() : v.z();
    }

    class Ray
    {
    public:
        Ray() = default;
        Ray(const vector3& o, const vector3& d) : origin(o), direction(d) {}

        vector3 operator()(float t) const { return origin + direction * t; }

        vector3 origin;
        vector3 direction;
    };
}

