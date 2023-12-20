#include "Scene.h"

namespace fishy
{
	bool Scene::Intersect(Ray& ray,  Interaction& isect)
	{
        bool isHit = false;
		for(auto& shape : shapeList)
        {
            if (shape->Intersect(ray, isect))
                isHit = true;

        }
        return isHit;
	}
}