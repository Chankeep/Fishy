#include "Film.h"

namespace Fishy
{
	bool Fishy::Film::store_image() const
	{
		return renderImage->save(filename);
	}
}
