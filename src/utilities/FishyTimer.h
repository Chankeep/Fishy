//
// Created by chankeep on 4/1/2024.
//

#ifndef FISHY_FISHYTIMER_H
#define FISHY_FISHYTIMER_H

#include <chrono>

class FishyTimer
{
public:
    FishyTimer() = default;
    ~FishyTimer() = default;

    void begin()
    {
        startTime = std::chrono::high_resolution_clock::now();
    }

    void end()
    {
        endTime = std::chrono::high_resolution_clock::now();
        duration = endTime - startTime;
    }

    float elapsedTime()
    {
        return duration.count();
    }


private:
    std::chrono::time_point<std::chrono::steady_clock> startTime, endTime;
    std::chrono::duration<float> duration;
};


#endif //FISHY_FISHYTIMER_H
