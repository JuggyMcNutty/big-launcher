#include <algorithm>
#include <cmath>
#include <limits>

#include "focus.hpp"
#include "node.hpp"
#include "engine.hpp"

namespace Foyer::ui {
    static Node *g_focus = nullptr;
    static Node *g_scope = nullptr;

    Node* Focus::current() { return g_focus; }
    Node* Focus::scope() { return g_scope; }
    void Focus::set_scope(Node *s) { g_scope = s; }

    void Focus::node_destroyed(Node *n)
    {
        if (g_focus == n) g_focus = nullptr;
        if (g_scope == n) g_scope = nullptr;
    }

    void Focus::set(Node *n, bool notify)
    {
        if (n == g_focus) return;
        Node *old = g_focus;
        g_focus = n;
        // remember in groups
        if (n)
            for (Node *p = n->parent(); p; p = p->parent())
                if (p->focus_memory) p->remembered = n;
        if (notify) {
            if (old && old->on_focus_changed) old->on_focus_changed(false);
            if (n && n->on_focus_changed) n->on_focus_changed(true);
        }
        invalidate();
    }

    void Focus::collect(Node *root, std::vector<Node*> &out)
    {
        if (!root || !root->visible) return;
        if (root->focusable) out.push_back(root);
        for (auto &c : root->children()) collect(c.get(), out);
    }

    Node* Focus::first_in(Node *root)
    {
        if (!root) return nullptr;
        if (root->focus_memory && root->remembered && root->contains(root->remembered) && root->remembered->visible)
            return root->remembered;
        std::vector<Node*> v;
        collect(root, v);
        return v.empty() ? nullptr : v.front();
    }

    Node* Focus::best_in_direction(const SDL_FRect &f, Action dir, const std::vector<std::pair<Node*, SDL_FRect>> &cands)
    {
        float fcx = f.x + f.w / 2, fcy = f.y + f.h / 2;
        Node *best = nullptr;
        float best_score = std::numeric_limits<float>::max();
        for (auto &[n, r] : cands) {
            float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
            float primary, cross;
            const float eps = 1.f;
            switch (dir) {
                case Action::Left:
                    if (r.x + r.w > f.x + eps && cx >= fcx) continue;
                    primary = std::max(0.f, f.x - (r.x + r.w));
                    cross = std::max(0.f, std::max(r.y - (f.y + f.h), f.y - (r.y + r.h)));
                    break;
                case Action::Right:
                    if (r.x < f.x + f.w - eps && cx <= fcx) continue;
                    primary = std::max(0.f, r.x - (f.x + f.w));
                    cross = std::max(0.f, std::max(r.y - (f.y + f.h), f.y - (r.y + r.h)));
                    break;
                case Action::Up:
                    if (r.y + r.h > f.y + eps && cy >= fcy) continue;
                    primary = std::max(0.f, f.y - (r.y + r.h));
                    cross = std::max(0.f, std::max(r.x - (f.x + f.w), f.x - (r.x + r.w)));
                    break;
                case Action::Down:
                    if (r.y < f.y + f.h - eps && cy <= fcy) continue;
                    primary = std::max(0.f, r.y - (f.y + f.h));
                    cross = std::max(0.f, std::max(r.x - (f.x + f.w), f.x - (r.x + r.w)));
                    break;
                default:
                    continue;
            }
            // Overlapping on the cross axis is strongly preferred; the
            // centre offset breaks ties (closest to straight ahead wins).
            float centre = (dir == Action::Left || dir == Action::Right) ? std::fabs(cy - fcy) : std::fabs(cx - fcx);
            float score = primary + cross * 4.f + centre * 0.05f;
            if (score < best_score) { best_score = score; best = n; }
        }
        return best;
    }

    Node* Focus::move(Engine &e, Action dir)
    {
        Node *scope = g_scope;
        if (!scope) return nullptr;
        std::vector<Node*> all;
        collect(scope, all);
        if (all.empty()) return nullptr;
        if (!g_focus || !scope->contains(g_focus)) {
            Node *n = first_in(scope);
            set(n);
            return n;
        }
        SDL_FRect from = screen_rect(e, *g_focus);
        std::vector<std::pair<Node*, SDL_FRect>> cands;
        for (Node *n : all)
            if (n != g_focus) cands.push_back({n, screen_rect(e, *n)});
        Node *best = best_in_direction(from, dir, cands);
        if (!best) return nullptr;
        // Entering a different focus-memory group: restore its remembered child.
        for (Node *p = best->parent(); p && p != scope; p = p->parent()) {
            if (p->focus_memory) {
                if (!p->contains(g_focus) && p->remembered && p->contains(p->remembered) && p->remembered->visible)
                    best = p->remembered;
                break;
            }
        }
        set(best);
        return best;
    }
}
