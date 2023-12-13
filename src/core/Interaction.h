#pragma once
#include <complex.h>

#include "Geometry.h"

namespace fishy
{
	class Interaction
	{
	public:
		Interaction() = default;

		Interaction(const vector3& position, const vector3& normal, const vector3& w_o)
			: position(position), normal(normal), w_o(w_o) {}

	private:
		vector3 position;
		vector3 normal;
		vector3 w_o;
	};
}
