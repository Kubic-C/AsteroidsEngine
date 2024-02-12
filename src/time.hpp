#pragma once

#include "includes.hpp"

inline auto startTime = std::chrono::high_resolution_clock::now();

inline std::chrono::high_resolution_clock::time_point nowTp() {
    return std::chrono::high_resolution_clock::now();
}

template<typename measure = std::chrono::seconds>
typename measure::rep now() {
    using namespace std::chrono;

    auto end = nowTp();
    measure dur = duration_cast<measure>(end - startTime);

    return dur.count();
}

template<typename rep, typename period>
rep now() {
    using namespace std::chrono;
    using durationRP = duration<rep, period>;

    auto end = nowTp();
    durationRP dur = duration_cast<durationRP>(end - startTime);

    return dur.count();
}

inline float nowSeconds() {
    return now<float, std::chrono::seconds::period>();
}

template<typename ... func_params>
class Ticker {
public:
    using Function = std::function<func_params...>;

    Ticker()
        : rate(60.0f) {}

    void setRate(float rate) {
       this->rate = rate;
    }

    // first parameter of the function must be a float.
    void setFunction(Function function) {
        this->function = function;
    }

    template<typename ... params>
    void update(params&& ... args) {
        if (lastUpdate == 0.0f) {
            lastUpdate = nowSeconds();
        }

        float now = nowSeconds();
        updateTime = now - lastUpdate;
        callsTodo += updateTime * rate;
        lastUpdate = now;

        while (callsTodo >= 1.0f) {
            callsTodo--;

            now = nowSeconds();
            deltaTime = now - lastTick;
            lastTick = now;

            function(deltaTime, args...);
        }
    }

    float getDeltaTime() {
        return deltaTime;
    }

private:
    Function function = nullptr;
    float rate = 0.0;
    float lastUpdate = 0.0;
    float updateTime = 0.0;
    float callsTodo = 0.0;

    float lastTick = 0.0f;
    float deltaTime = 0.0f;
};