#pragma once

#include <memory>
#include <vector>

#include "../input/action.hpp"

namespace Foyer {
    class App;

    // A screen owns a region of the UI and receives all input while on top of
    // the stack. Screens are value-owned by the stack (RAII).
    class Screen {
    public:
        explicit Screen(App &app): app_(app) {}
        virtual ~Screen() = default;

        virtual void on_show() {}
        virtual void on_hide() {}
        virtual void update() {}
        virtual void draw() = 0;

        // Input. Return true if the action was handled.
        virtual bool on_action(Action action) { (void) action; return false; }
        virtual bool on_text(const std::string &text) { (void) text; return false; }

        // Back handling: return true if consumed (screen did not pop).
        virtual bool on_back() { return false; }

        App& app() { return app_; }

    protected:
        App &app_;
    };

    // LIFO stack of screens. Input is dispatched to the top screen only.
    class ScreenStack {
    public:
        void push(std::unique_ptr<Screen> screen) {
            if (!screens_.empty())
                screens_.back()->on_hide();
            screens_.push_back(std::move(screen));
            if (screens_.size())
                screens_.back()->on_show();
        }
        void pop() {
            if (screens_.empty()) return;
            screens_.back()->on_hide();
            screens_.pop_back();
            if (!screens_.empty())
                screens_.back()->on_show();
        }
        void replace(std::unique_ptr<Screen> screen) {
            while (!screens_.empty()) screens_.pop_back();
            push(std::move(screen));
        }
        Screen* top() { return screens_.empty() ? nullptr : screens_.back().get(); }
        const Screen* top() const { return screens_.empty() ? nullptr : screens_.back().get(); }
        bool empty() const { return screens_.empty(); }
        size_t size() const { return screens_.size(); }
        void clear() { while (!screens_.empty()) screens_.pop_back(); }

    private:
        std::vector<std::unique_ptr<Screen>> screens_;
    };
}
