#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <SDL3/SDL.h>

namespace Foyer {
    // Generic tween engine. Replaces the old Shift/Press queues. Each animation
    // drives a float from start to end over a duration with optional easing and
    // a per-frame callback.
    class Anim {
    public:
        enum class Ease { Linear, OutQuad, OutCubic, InOutQuad };

        struct Tween {
            int id = 0;
            std::function<void(float)> on_update;
            std::function<void()> on_complete;
            float start = 0.f, end = 0.f, current = 0.f;
            Uint64 start_time = 0;
            Uint32 duration = 200;
            Ease ease = Ease::OutQuad;
            bool done = false;
        };

        // Start a tween. Returns an id.
        int add(std::function<void(float)> on_update, float start, float end,
                Uint32 duration, Ease ease = Ease::OutQuad,
                std::function<void()> on_complete = nullptr);
        void update();              // call once per frame
        bool active() const;
        void cancel_all();
        void cancel(int id);

    private:
        std::vector<std::unique_ptr<Tween>> tweens_;
        int next_id_ = 1;
        static float ease_value(Ease e, float t);
    };
}
