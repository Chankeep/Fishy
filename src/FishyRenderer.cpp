#include <QHBoxLayout>
#include <QVBoxLayout>
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
        initRender();
        renderQ3D();
//        renderRT();
    }

    FishyRenderer::~FishyRenderer()
    {
    }

    bool FishyRenderer::renderRT()
    {
        int samplersPerPixel = 1;
        std::unique_ptr<Sampler> originalSampler = std::make_unique<TrapezoidalSampler>(samplersPerPixel);

        std::unique_ptr<Integrator> integrator = std::make_unique<PathIntegrator>();
        integrator->Render(scene, *cam, *originalSampler, film);
        return film->store_image();
    }

    bool FishyRenderer::renderQ3D()
    {
        view = new Qt3DExtras::Qt3DWindow();
        view->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
        QWidget * container = QWidget::createWindowContainer(view);
        QSize screenSize = QSize(width, height);
        container->setMinimumSize(QSize(200, 100));
        container->setMaximumSize(screenSize);

        QWidget *widget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(widget);
        QVBoxLayout *vLayout = new QVBoxLayout();
        vLayout->setAlignment(Qt::AlignTop);
        hLayout->addWidget(container, 1);
        hLayout->addLayout(vLayout);

        widget->setWindowTitle(QStringLiteral("Basic shapes"));

        Qt3DRender::QCamera *cameraEntity = view->camera();
        cameraEntity->lens()->setPerspectiveProjection(cam->fov, static_cast<float>(width) / static_cast<float>(height), 0.1f, 10000.0f);
        cameraEntity->setPosition(cam->position);
        cameraEntity->setUpVector(cam->up);
        cameraEntity->setViewCenter(cam->center);

        Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(scene);
        Qt3DRender::QPointLight *pointLight = new Qt3DRender::QPointLight(lightEntity);
        pointLight->setColor("white");
        pointLight->setIntensity(1);
        lightEntity->addComponent(pointLight);
        Qt3DCore::QTransform *lightTransform = new Qt3DCore::QTransform(lightEntity);
        lightTransform->setTranslation(vector3(0, 18 - .5, 0));
        lightEntity->addComponent(lightTransform);

        Qt3DExtras::QFirstPersonCameraController *cameraController = new Qt3DExtras::QFirstPersonCameraController(scene);
        cameraController->setCamera(cameraEntity);

        view->setRootEntity(scene);

        widget->show();
        widget->resize(1000, 800);
        return true;
    }

    bool FishyRenderer::initRender()
    {
        film = new Film({static_cast<float>(width), static_cast<float>(height)}, "render_image.png");
        cam = std::make_unique<PerspectiveCamera>(
                vector3(0, 0, -19), vector3(0, 0, 1).normalized(), vector3(0, 1, 0), 75, film->Resolution()
        );


        scene = Scene::CreateSmallptScene();
        return true;
    }
}