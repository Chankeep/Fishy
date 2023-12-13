#pragma once
#include "Geometry.h"


namespace fishy
{
	class Frame
	{
	public:
        Frame() = default;
		Frame(const vector3& n) : n(n.normalized())
		{
			SetFromNormal();
		}

		vector3 ToLocal(const vector3& worldVec3) const
		{
			return {
				dot(t, worldVec3),
				dot(b, worldVec3),
				dot(n, worldVec3)
			};
		}

		vector3 ToWorld(const vector3& localVec3) const
		{
			return t * localVec3.x() + b * localVec3.y() + n * localVec3.z();
		}

		void SetFromNormal()
		{
			vector3 tmp_t = qAbs(n.x() > 0.99f) ? vector3(0, 1, 0) : vector3(1, 0, 0);
			b = cross(n, tmp_t).normalized();
			t = cross(b, n).normalized();
		}

		const vector3& tangent() const { return t; }
		const vector3& binormal() const { return b; }
		const vector3& normal() const { return n; }

	private:
		vector3 t{1, 0, 0};
		vector3 b{0, 1, 0};
		vector3 n{0, 0, 1};
	};
}
