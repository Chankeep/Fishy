#pragma once

#include "Geometry.h"

namespace fishy
{
	class Transform
	{
	public:
		Transform() = default;
		Transform(const matrix4x4& m) : m(m), mInv(m.inverted()) {}
		Transform(const matrix4x4& m, const matrix4x4& mInv) : m(m), mInv(mInv) {}
		Transform(const float* mat)
		{
			m = matrix4x4(mat);
			mInv = m.inverted();
		}

		bool operator==(const Transform& t)
		{
			return m == t.m && mInv == t.mInv;
		}

		bool isIdentity() const
		{
			return m.isIdentity();
		}

		const matrix4x4& GetMatrix() const { return m; }
		const matrix4x4& GetInverseMatrix() const { return mInv; }

		friend Transform Inverse(const Transform& t)
		{
			return { t.m, t.mInv };
		}

		friend Transform Transpose(const Transform& t)
		{
			return { t.m.transposed(), t.mInv.transposed() };
		}
	private:

		matrix4x4 m;
		matrix4x4 mInv;
	};

	Transform Translate(const vector3& delta);
	Transform Scale(float x, float y, float z);
	Transform RotateX(float theta);
	Transform RotateY(float theta);
	Transform RotateZ(float theta);
	Transform Rotate(float theta, const vector3& axis);
	Transform LookAt(const vector3& pos, const vector3& look, const vector3& up);
	Transform Perspective(float fov, float zNear, float zFar);
	Transform Orthographic(float zNear, float zFar);
}
