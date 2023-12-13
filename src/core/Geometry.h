#pragma once
#include "common.h"

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include <random>
#include <numbers>

namespace fishy
{
	constexpr double Infinity = std::numeric_limits<double>::infinity();
	constexpr double Pi = std::numbers::pi;
	constexpr double InvPi = std::numbers::inv_pi;

	using vector2 = QVector2D;
	using vector3 = QVector3D;
	using vector4 = QVector4D;

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

	template<typename T>
	class Bounds2D
	{
	public:
		Bounds2D()
		{
			T minNum = std::numeric_limits<T>::lowest();
			T maxNum = std::numeric_limits<T>::max();
			pMin = vector2(maxNum, maxNum);
			pMax = vector2(minNum, minNum);
		}
		Bounds2D(const vector2& p) : pMin(p), pMax(p) {}
		Bounds2D(const vector2& p1, const vector2& p2)
		{
			pMin = vector2(std::min(p1.x(), p2.x()), std::min(p1.y(), p2.y()));
			pMax = vector2(std::max(p1.x(), p2.x()), std::max(p1.y(), p2.y()));
		}

		vector2 Diagonal() const
		{
			return pMax - pMin;
		}

		T Area() const
		{
			const vector2 d = Diagonal();
			return d.x() * d.y();
		}

		int MaximumExtent() const
		{
			vector2 diag = Diagonal();
			if (diag.x > diag.y)
				return 0;
			return 1;
		}

		vector2 pMin, pMax;
	};

}

