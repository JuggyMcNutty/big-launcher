// UI engine tests: flex layout, dp scaling, focus navigation, springs,
// scroller, asset decode. Uses an offscreen software renderer.
#include "doctest.h"

#include <cstdlib>
#include <cstdio>
#include <fstream>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "../src/ui/engine/engine.hpp"
#include "../src/ui/engine/focus.hpp"
#include "../src/ui/engine/scroller.hpp"
#include "../src/ui/engine/widgets.hpp"

using namespace Foyer::ui;

namespace {
    struct Headless {
        SDL_Window *win = nullptr;
        SDL_Renderer *ren = nullptr;
        Headless(int w = 1920, int h = 1080) {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen,dummy");
            SDL_Init(SDL_INIT_VIDEO);
            TTF_Init();
            win = SDL_CreateWindow("t", w, h, SDL_WINDOW_HIDDEN);
            ren = win ? SDL_CreateRenderer(win, "software") : nullptr;
            if (!ren) {
                // fall back to a surface renderer
                SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
                ren = SDL_CreateSoftwareRenderer(s);
            }
        }
        ~Headless() {
            if (ren) SDL_DestroyRenderer(ren);
            if (win) SDL_DestroyWindow(win);
        }
    };

    struct Page : Layer {
        std::function<void(Engine&, Node&)> fn;
        void build(Engine &e) override { if (fn) fn(e, *root); }
    };

    Node* box(Node *p, float w, float h) {
        auto *n = p->add<Node>();
        n->style.w = Dim::dp(w); n->style.h = Dim::dp(h);
        return n;
    }

    SDL_FRect abs_rect(Engine &e, Node *n) { return screen_rect(e, *n); }
}

TEST_SUITE("Layout") {

TEST_CASE("row with gap, padding and grow") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node root;
    root.style.dir = Dir::Row;
    root.style.pad = Edges::all(10);
    root.style.gap = 20;
    root.style.align = Align::Start;
    Node *a = box(&root, 100, 50);
    Node *b = box(&root, 0, 50); b->style.w = Dim::fill();
    Node *c = box(&root, 200, 80);
    arrange_node(e, root, 0, 0, 1000, 200);
    CHECK(a->frame.x == doctest::Approx(10));
    CHECK(a->frame.w == doctest::Approx(100));
    CHECK(b->frame.x == doctest::Approx(130));
    CHECK(b->frame.w == doctest::Approx(1000 - 20 - 100 - 200 - 40));
    CHECK(c->frame.x == doctest::Approx(1000 - 10 - 200));
    CHECK(c->frame.h == doctest::Approx(80));
}

TEST_CASE("dp scale applies at 4K and 720p") {
    Headless h;
    Engine e(h.ren, 3840, 2160);
    Node root;
    root.style.dir = Dir::Column;
    root.style.align = Align::Start;
    Node *a = box(&root, 100, 50);
    arrange_node(e, root, 0, 0, 3840, 2160);
    CHECK(a->frame.w == doctest::Approx(200));
    e.configure(1280, 720, 100, 0);
    arrange_node(e, root, 0, 0, 1280, 720);
    CHECK(a->frame.w == doctest::Approx(100 * 720.0 / 1080.0 * 1.1));
}

TEST_CASE("percent, aspect, center/end alignment, justify") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node root;
    root.style.dir = Dir::Column;
    root.style.align = Align::Center;
    root.style.justify = Justify::End;
    auto *a = root.add<Node>();
    a->style.w = Dim::pct(50);
    a->style.aspect = 2.f;
    arrange_node(e, root, 0, 0, 800, 600);
    CHECK(a->frame.w == doctest::Approx(400));
    CHECK(a->frame.h == doctest::Approx(200));
    CHECK(a->frame.x == doctest::Approx(200));
    CHECK(a->frame.y == doctest::Approx(400));
}

TEST_CASE("auto container sizes to children; column stretch") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node root;
    root.style.dir = Dir::Column;
    auto *row = root.add<Node>();
    row->style.dir = Dir::Row;
    row->style.gap = 10;
    box(row, 50, 30); box(row, 70, 40);
    arrange_node(e, root, 0, 0, 500, 500);
    CHECK(row->frame.w == doctest::Approx(500));   // stretched in column
    CHECK(row->frame.h == doctest::Approx(40));
    Size s = measure_node(e, *row, 500, 500);
    CHECK(s.w == doctest::Approx(130));
}

TEST_CASE("shrink to fit, but scrollers keep natural size") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node root;
    root.style.dir = Dir::Row;
    Node *a = box(&root, 400, 10), *b = box(&root, 400, 10);
    arrange_node(e, root, 0, 0, 400, 100);
    CHECK(a->frame.w + b->frame.w == doctest::Approx(400));
    Scroller sc(Dir::Row);
    Node *c = box(&sc, 400, 10), *d = box(&sc, 400, 10);
    arrange_node(e, sc, 0, 0, 400, 100);
    CHECK(c->frame.w == doctest::Approx(400));
    CHECK(d->frame.x == doctest::Approx(400));
    CHECK(sc.content_extent() == doctest::Approx(800));
    CHECK(sc.max_offset() == doctest::Approx(400));
}

TEST_CASE("min/max clamp and absolute positioning") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node root;
    root.style.dir = Dir::Row;
    auto *a = root.add<Node>();
    a->style.w = Dim::fill(); a->style.max_w = 300; a->style.h = Dim::dp(10);
    auto *abs = root.add<Node>();
    abs->style.absolute = true; abs->style.x = 50; abs->style.y = 60;
    abs->style.w = Dim::dp(10); abs->style.h = Dim::dp(10);
    arrange_node(e, root, 0, 0, 1000, 100);
    CHECK(a->frame.w == doctest::Approx(300));
    CHECK(abs->frame.x == doctest::Approx(50));
    CHECK(abs->frame.y == doctest::Approx(60));
}

}

TEST_SUITE("Focus") {

TEST_CASE("spatial navigation in a grid") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node *t[2][3];
    auto page = std::make_unique<Page>();
    page->fn = [&](Engine &, Node &root) {
        root.style.dir = Dir::Column;
        root.style.gap = 20;
        root.style.align = Align::Start;
        for (int r = 0; r < 2; r++) {
            auto *row = root.add<Node>();
            row->style.dir = Dir::Row; row->style.gap = 20;
            for (int c = 0; c < 3; c++) {
                t[r][c] = box(row, 100, 100);
                t[r][c]->focusable = true;
            }
        }
    };
    e.push(std::move(page), false);
    e.layout();
    REQUIRE(Focus::current() == t[0][0]);
    e.dispatch(Foyer::Action::Right); CHECK(Focus::current() == t[0][1]);
    e.dispatch(Foyer::Action::Down);  CHECK(Focus::current() == t[1][1]);
    e.dispatch(Foyer::Action::Right); CHECK(Focus::current() == t[1][2]);
    e.dispatch(Foyer::Action::Right); CHECK(Focus::current() == t[1][2]);   // edge: stays
    e.dispatch(Foyer::Action::Up);    CHECK(Focus::current() == t[0][2]);
    e.dispatch(Foyer::Action::Left);  CHECK(Focus::current() == t[0][1]);
}

TEST_CASE("rows remember focus (focus_memory)") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Node *top[4], *bot[4];
    auto page = std::make_unique<Page>();
    page->fn = [&](Engine &, Node &root) {
        root.style.dir = Dir::Column; root.style.gap = 20; root.style.align = Align::Start;
        auto mk = [&](Node *arr[4], float offset) {
            auto *row = root.add<Node>();
            row->style.dir = Dir::Row; row->style.gap = 20;
            row->style.pad.l = offset;
            row->focus_memory = true;
            for (int c = 0; c < 4; c++) { arr[c] = box(row, 100, 100); arr[c]->focusable = true; }
        };
        mk(top, 0); mk(bot, 0);
    };
    e.push(std::move(page), false);
    e.layout();
    e.dispatch(Foyer::Action::Right); e.dispatch(Foyer::Action::Right); e.dispatch(Foyer::Action::Right);
    CHECK(Focus::current() == top[3]);
    e.dispatch(Foyer::Action::Down);  CHECK(Focus::current() == bot[3]);
    e.dispatch(Foyer::Action::Left); e.dispatch(Foyer::Action::Left); e.dispatch(Foyer::Action::Left);
    CHECK(Focus::current() == bot[0]);
    e.dispatch(Foyer::Action::Up);    CHECK(Focus::current() == top[3]);   // restored, not nearest
}

TEST_CASE("activate, bubbling on_action, layers restore focus") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    int activated = 0, bubbled = 0;
    Node *b1 = nullptr, *b2 = nullptr;
    auto page = std::make_unique<Page>();
    page->fn = [&](Engine &, Node &root) {
        root.style.dir = Dir::Row; root.style.align = Align::Start;
        root.on_action = [&](Foyer::Action a) { if (a == Foyer::Action::Menu) { bubbled++; return true; } return false; };
        b1 = box(&root, 50, 50); b1->focusable = true;
        b2 = box(&root, 50, 50); b2->focusable = true;
        b2->on_activate = [&] { activated++; };
    };
    e.push(std::move(page), false);
    e.layout();
    e.dispatch(Foyer::Action::Right);
    e.dispatch(Foyer::Action::Accept);
    CHECK(activated == 1);
    e.dispatch(Foyer::Action::Menu);
    CHECK(bubbled == 1);
    auto modal = std::make_unique<Page>();
    Node *m1 = nullptr;
    modal->fn = [&](Engine &, Node &root) { m1 = box(&root, 10, 10); m1->focusable = true; };
    e.push(std::move(modal), false);
    CHECK(Focus::current() == m1);
    CHECK(e.dispatch(Foyer::Action::Back));
    CHECK(Focus::current() == b2);
    CHECK_FALSE(e.dispatch(Foyer::Action::Back));   // root layer doesn't pop
}

TEST_CASE("best_in_direction prefers aligned candidates") {
    SDL_FRect from{0, 0, 100, 100};
    Node a, b;
    std::vector<std::pair<Node*, SDL_FRect>> c = {{&a, {300, 0, 100, 100}}, {&b, {150, 400, 100, 100}}};
    CHECK(Focus::best_in_direction(from, Foyer::Action::Right, c) == &a);
    CHECK(Focus::best_in_direction(from, Foyer::Action::Down, c) == &b);
    CHECK(Focus::best_in_direction(from, Foyer::Action::Left, c) == nullptr);
}

}

TEST_SUITE("Motion") {

TEST_CASE("spring converges and snaps") {
    Motion::reduce = false; Motion::speed = 1.f;
    Animated a(0.f);
    a.to(1.f);
    CHECK(a.active());
    float prev = 0.f; bool monotonic = true;
    for (int i = 0; i < 120 && a.active(); i++) {
        a.step(1.f / 60.f);
        if (a.get() < prev - 1e-4f) monotonic = false;
        prev = a.get();
    }
    CHECK_FALSE(a.active());
    CHECK(a.get() == doctest::Approx(1.f));
    CHECK(monotonic);   // critically damped: no overshoot oscillation back
    Motion::reduce = true;
    a.to(5.f);
    CHECK(a.get() == doctest::Approx(5.f));
    Motion::reduce = false;
}

TEST_CASE("scroller follows focus") {
    Headless h;
    Engine e(h.ren, 1920, 1080);
    Scroller *sc = nullptr;
    Node *items[10];
    auto page = std::make_unique<Page>();
    page->fn = [&](Engine &, Node &root) {
        root.style.dir = Dir::Column; root.style.align = Align::Start;
        sc = root.add<Scroller>(Dir::Row);
        sc->style.w = Dim::dp(500); sc->style.h = Dim::dp(100);
        sc->margin = 0;
        for (int i = 0; i < 10; i++) { items[i] = box(sc, 100, 100); items[i]->focusable = true; }
    };
    e.push(std::move(page), false);
    e.layout();
    for (int i = 0; i < 7; i++) e.dispatch(Foyer::Action::Right);
    CHECK(Focus::current() == items[7]);
    for (int i = 0; i < 200; i++) e.update(1.f / 60.f);
    CHECK(sc->offset() == doctest::Approx(800 - 500));
    // focus is visible on screen
    SDL_FRect r = abs_rect(e, items[7]);
    CHECK(r.x + r.w <= 500 + 0.5f);
}

}

TEST_SUITE("Assets") {

TEST_CASE("decode svg and png at requested size, blur, cache budget") {
    const char *svg_path = "/tmp/foyer_test.svg";
    {
        std::ofstream f(svg_path);
        f << "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50'><rect width='100' height='50' fill='#f00'/></svg>";
    }
    SDL_Surface *s = AssetCache::decode(svg_path, 200, 200, 0.f, false);
    REQUIRE(s);
    CHECK(s->w == 200);
    CHECK(s->h == 100);   // contain keeps aspect
    Uint8 *p = (Uint8*) s->pixels;
    CHECK(p[0] > 200);    // red
    AssetCache::blur_surface(s, 4.f);
    SDL_DestroySurface(s);
    s = AssetCache::decode(svg_path, 200, 200, 0.f, true);
    REQUIRE(s);
    CHECK(s->h == 200);   // cover fills the box
    SDL_DestroySurface(s);
    CHECK(AssetCache::decode("/nonexistent.png", 10, 10, 0.f, false) == nullptr);

    Headless h;
    AssetCache cache(h.ren, 64 * 1024, 1);
    {
        auto a = cache.request(svg_path, 64, 64);
        for (int i = 0; i < 200 && !a->ready(); i++) { cache.pump(); SDL_Delay(5); }
        CHECK(a->ready());
        auto again = cache.request(svg_path, 64, 64);
        CHECK(again == a);   // cached
    }
    // many sizes overflow the 64 KiB budget; unreferenced ones get evicted
    for (int sz = 72; sz <= 200; sz += 16) {
        auto a = cache.request(svg_path, sz, sz);
        for (int i = 0; i < 200 && !a->ready() && !a->failed(); i++) { cache.pump(); SDL_Delay(2); }
    }
    cache.pump();
    cache.evict();
    CHECK(cache.bytes() <= 64 * 1024 + 200 * 200 * 4);
    std::remove(svg_path);
}

}
