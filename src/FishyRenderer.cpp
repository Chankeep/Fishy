#include "FishyRenderer.h"

#include "cameras/PerspectiveCamera.h"
#include "core/Integrator.h"
#include "samplers/TrapezoidalSampler.h"

namespace Fishy
{
    FishyRenderer::FishyRenderer(QWidget *parent)
            : QMainWindow(parent)
    {
        ui.setupUi(this);

    }

    FishyRenderer::~FishyRenderer()
    {
    }

    bool FishyRenderer::render()
    {
	    constexpr int width = 800;
	    constexpr int height = 600;
        Film film({static_cast<float>(width), static_cast<float>(height)}, "render_image.png");
        int samplersPerPixel = 3;
        std::unique_ptr<Sampler> originalSampler = std::make_unique<TrapezoidalSampler>(samplersPerPixel);
        
        std::unique_ptr<Camera> cam = std::make_unique<PerspectiveCamera>(
                vector3(0, 0, -19), vector3(0, 0, 1).normalized(), vector3(0, 1, 0), 80, film.Resolution()
                );
        
        auto scene = Scene::CreateSmallptScene();
        std::unique_ptr<Integrator> integrator = std::make_unique<PathIntegrator>();
        integrator->Render(scene, *cam, *originalSampler, film);
        

        return film.store_image();

    }
}