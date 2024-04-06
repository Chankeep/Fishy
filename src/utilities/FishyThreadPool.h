//
// Created by chankeep on 4/4/2024.
//

#ifndef FISHY_FISHYTHREADPOOL_H
#define FISHY_FISHYTHREADPOOL_H

#include <QRunnable>
#include <QThreadPool>
#include "../core/Fishy.h"
#include "../core/Integrator.h"

namespace Fishy
{
    inline std::atomic renderSignal = 0;
    inline bool isFirstThread = true;

    class FragRenderer : public QRunnable
    {
    public:
        FragRenderer() = delete;
        FragRenderer(const vector2 &begin, const vector2 &end,
                std::shared_ptr<Scene> scene,
                std::shared_ptr<Camera> camera,
                std::shared_ptr<Sampler> originalSampler,
                std::shared_ptr<Film> film);

        virtual ~FragRenderer() = default;

        void run() override;
        void resetSignal();

    private:
        const vector2 begin;
        const vector2 end;
        std::shared_ptr<Film> film;
        std::shared_ptr<Scene> scene;
        std::shared_ptr<Camera> camera;
        std::shared_ptr<Integrator> integrator;
        std::shared_ptr<Sampler> originalSampler;

    };


    class FishyThreadPool
    {

    };

} // Fishy

#endif //FISHY_FISHYTHREADPOOL_H
