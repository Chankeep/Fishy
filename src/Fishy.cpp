#include "Fishy.h"

#include "cameras/PerspectiveCamera.h"
#include "core/Integrator.h"
#include "samplers/RandomSampler.h"
namespace fishy
{
    Fishy::Fishy(QWidget *parent)
            : QMainWindow(parent)
    {
        ui.setupUi(this);

    }

    Fishy::~Fishy()
    {
    }

    bool Fishy::render()
    {
	    constexpr int width = 800;
	    constexpr int height = 600;
        Film film({static_cast<float>(width), static_cast<float>(height)}, "render_image.png");
        int samplersPerPixel = 5;
        std::unique_ptr<Sampler> originalSampler = std::make_unique<RandomSampler>(samplersPerPixel);
        
        std::unique_ptr<Camera> cam = std::make_unique<PerspectiveCamera>(
                vector3(0, 0, 50), vector3(0, 0, -1).normalized(), vector3(0, 1, 0), 60, film.Resolution()
                );
        
        auto scene = Scene::CreateSmallptScene();
        std::unique_ptr<Integrator> integrator = std::make_unique<PathIntegrator>();
        integrator->Render(scene, *cam, *originalSampler, film);
        

        return film.store_image();

    }
}