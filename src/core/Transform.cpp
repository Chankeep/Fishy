#include "Transform.h"

namespace fishy
{
	Transform Translate(const vector3& delta)
	{
		const matrix4x4 m(1, 0, 0, delta.x(),
			0, 1, 0, delta.y(),
			0, 0, 1, delta.z(),
			0, 0, 0, 1);
		const matrix4x4 mInv(1, 0, 0, -delta.x(),
			0, 1, 0, -delta.y(),
			0, 0, 1, -delta.z(),
			0, 0, 0, 1);
		return { m, mInv };
	}

	Transform Scale(float x, float y, float z)
	{
		const matrix4x4 m(x, 0, 0, 0,
			0, y, 0, 0,
			0, 0, z, 0,
			0, 0, 0, 1);
		const matrix4x4 mInv(1 / x, 0, 0, 0,
			0, 1 / y, 0, 0,
			0, 0, 1 / z, 0,
			0, 0, 0, 1);
		return { m, mInv };
	}

	Transform RotateX(float theta)
	{
		const float sinTheta = qSin(qDegreesToRadians(theta));
		const float cosTheta = qCos(qDegreesToRadians(theta));

		const matrix4x4 m(1, 0, 0, 0,
			0, cosTheta, -sinTheta, 0,
			0, sinTheta, cosTheta, 0,
			0, 0, 0, 1);

		return { m, m.transposed() };
	}

	Transform RotateY(float theta)
	{
		const float sinTheta = qSin(qDegreesToRadians(theta));
		const float cosTheta = qCos(qDegreesToRadians(theta));

		const matrix4x4 m(cosTheta, 0, sinTheta, 0,
			0, 1, 0, 0,
			-sinTheta, 0, cosTheta, 0,
			0, 0, 0, 1);

		return { m,m.transposed() };
	}

	Transform RotateZ(float theta)
	{
		const float sinTheta = qSin(qDegreesToRadians(theta));
		const float cosTheta = qCos(qDegreesToRadians(theta));

		const matrix4x4 m(cosTheta, -sinTheta, 0, 0,
			sinTheta, cosTheta, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);

		return { m,m.transposed() };
	}

	Transform Rotate(float theta, const vector3& axis)
	{
		return {};
	}

	Transform LookAt(const vector3& pos, const vector3& look, const vector3& up)
	{
		return {};
	}

	Transform Perspective(float fov, float zNear, float zFar)
	{
		return {};
	}

	Transform Orthographic(float zNear, float zFar)
	{
		return {};
	}
}
