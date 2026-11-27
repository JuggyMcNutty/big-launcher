#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "../../input/action.hpp"

namespace Foyer::ui {
    class Node;
    class Engine;

    // Spatial D-pad focus. Candidates are focusable, visible descendants of
    // the active scope (top layer). Moves prefer, in order:
    //   1. the remembered child of the nearest `focus_memory` ancestor group
    //      we're entering (rows restore their last tile);
    //   2. the candidate in the half-plane of the direction with the smallest
    //      weighted distance (primary-axis gap + 2x cross-axis offset).
    class Focus {
    public:
        static Node* current();
        static void set(Node *n, bool notify = true);
        // Bumped on every focus change; lets the engine tell pointer-driven
        // focus changes apart from D-pad/keyboard ones.
        static uint64_t generation();
        static void node_destroyed(Node *n);
        static void set_scope(Node *scope);
        static Node* scope();

        // Returns the new focus or nullptr if nothing in that direction.
        static Node* move(Engine &e, Action dir);
        // Pick the best candidate (for tests / initial focus).
        static Node* first_in(Node *root);
        static void collect(Node *root, std::vector<Node*> &out);
        static Node* best_in_direction(const SDL_FRect &from, Action dir, const std::vector<std::pair<Node*, SDL_FRect>> &cands);
    };
}
