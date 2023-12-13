#pragma once
#include "common.h"

namespace fishy
{
	class Film final
	{
	public:
		Film(const vector2& resolution, QString filename) : fullResolution(resolution), filename(std::move(filename)),
			renderImage(std::make_unique<QImage>()) {}

		int width() const { return static_cast<int>(fullResolution.x()); }
		int height() const { return static_cast<int>(fullResolution.y()); }
		vector2 Resolution() const { return fullResolution; }

		const QRgb& operator()(int x, int y)
		{
			return renderImage->pixel(x, y);
		}

		void setColor(int i, const QRgb& c)
		{
			renderImage->setColor(i, c);
		}

		void add_color(int i, const QRgb& delta)
		{
			renderImage->setColor(i, renderImage->color(i) + delta);
		}

		virtual bool store_image() const;

	private:
		const vector2 fullResolution;
		const QString filename;

		std::unique_ptr<QImage> renderImage;
	};
}
