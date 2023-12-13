#include "Film.h"

namespace fishy
{
	bool fishy::Film::store_image() const
	{
		return renderImage->save(filename);
	}
}
