#include <cmath>
#include <algorithm>

#include "anim.hpp"

int Foyer::Anim::add(std::function<void(float)> on_update, float start, float end,
                  Uint32 duration, Ease ease, std::function<void()> on_complete)
{
    auto t = std::make_unique<Tween>();
    t->id = next_id_++;
    t->on_update = std::move(on_update);
    t->on_complete = std::move(on_complete);
    t->start = start;
    t->end = end;
    t->current = start;
    t->start_time = SDL_GetTicks();
    t->duration = duration ? duration : 1;
    t->ease = ease;
    tweens_.push_back(std::move(t));
    return tweens_.back()->id;
}

float Foyer::Anim::ease_value(Ease e, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    switch (e) {
        case Ease::Linear:    return t;
        case Ease::OutQuad:   return 1.f - (1.f - t) * (1.f - t);
        case Ease::OutCubic:  { float u = 1.f - t; return 1.f - u * u * u; }
        case Ease::InOutQuad: return t < 0.5f ? 2.f * t * t : 1.f - std::pow(-2.f * t + 2.f, 2) / 2.f;
    }
    return t;
}

void Foyer::Anim::update()
{
    Uint64 now = SDL_GetTicks();
    for (auto it = tweens_.begin(); it != tweens_.end();) {
        Tween &t = **it;
        float progress = (t.duration == 0) ? 1.f
            : static_cast<float>(now - t.start_time) / static_cast<float>(t.duration);
        if (progress >= 1.f)
            progress = 1.f;
        t.current = t.start + (t.end - t.start) * ease_value(t.ease, progress);
        if (t.on_update)
            t.on_update(t.current);
        if (progress >= 1.f) {
            if (t.on_complete)
                t.on_complete();
            it = tweens_.erase(it);
        }
        else
            ++it;
    }
}

bool Foyer::Anim::active() const
{
    return !tweens_.empty();
}

void Foyer::Anim::cancel_all()
{
    tweens_.clear();
}

void Foyer::Anim::cancel(int id)
{
    tweens_.erase(std::remove_if(tweens_.begin(), tweens_.end(),
        [id](const std::unique_ptr<Tween> &t){ return t->id == id; }), tweens_.end());
}
