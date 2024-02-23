#pragma once

#include "Fishy.h"

namespace Fishy
{
    class Film final
    {
    public:
        Film(const vector2 &resolution, QString filename)
        : fullResolution(resolution),
        filename(std::move(filename))
        {
            QSize imageSize = {static_cast<int>(resolution.x()), static_cast<int>(resolution.y())};
            renderImage = std::make_unique<QImage>(imageSize, QImage::Format_ARGB32);
            renderImage->setColorCount(resolution.x() * resolution.y());
            qDebug() << renderImage->colorCount();
        }

        QImage* getImage()
        {
            return renderImage.get();
        }

        int width() const
        {
            return static_cast<int>(fullResolution.x());
        }

        int height() const
        {
            return static_cast<int>(fullResolution.y());
        }

        vector2 Resolution() const
        {
            return fullResolution;
        }

        const QRgb &operator()(int x, int y)
        {
            return renderImage->pixel(x, y);
        }

        void setPixelColor(int x, int y, const QRgb &c)
        {
            renderImage->setPixel(x, y, c);
        }

        void add_color(int i, const QRgb &delta)
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
