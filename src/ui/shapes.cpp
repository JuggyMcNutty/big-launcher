#include "shapes.hpp"
#include "../render/image.hpp"

namespace Foyer {
    static bool inside_rounded(int x, int y, int w, int h, int r)
    {
        if (r <= 0) return true;
        int cx = -1, cy = -1;
        bool corner = false;
        if (x < r && y < r)                 { cx = r;        cy = r;        corner = true; }
        else if (x >= w - r && y < r)       { cx = w - r - 1; cy = r;        corner = true; }
        else if (x < r && y >= h - r)       { cx = r;        cy = h - r - 1; corner = true; }
        else if (x >= w - r && y >= h - r)  { cx = w - r - 1; cy = h - r - 1; corner = true; }
        if (corner) {
            int dx = x - cx, dy = y - cy;
            return dx * dx + dy * dy <= r * r;
        }
        return true;
    }

    static void fill_rounded(SDL_Surface *s, int w, int h, int r, Uint32 color, bool clear)
    {
        Uint8 *px = static_cast<Uint8*>(s->pixels);
        int pitch = s->pitch;
        Uint32 transparent = 0;
        for (int y = 0; y < h; y++) {
            Uint32 *row = reinterpret_cast<Uint32*>(px + y * pitch);
            for (int x = 0; x < w; x++) {
                if (inside_rounded(x, y, w, h, r))
                    row[x] = clear ? transparent : color;
            }
        }
    }

    TexturePtr create_rounded_rect(Renderer &r, int w, int h, int radius, const SDL_Color &color)
    {
        SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        Uint32 fill = SDL_MapSurfaceRGBA(s, color.r, color.g, color.b, color.a);
        fill_rounded(s, w, h, radius, fill, false);
        TexturePtr t = r.create_texture(*s);
        Foyer::free_surface(s);
        return t;
    }

    TexturePtr create_rounded_border(Renderer &r, int w, int h, int radius, int thickness, const SDL_Color &color)
    {
        SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        Uint32 fill = SDL_MapSurfaceRGBA(s, color.r, color.g, color.b, color.a);
        fill_rounded(s, w, h, radius, fill, false);
        // Clear the inner region to make a border
        int iw = w - 2 * thickness, ih = h - 2 * thickness;
        if (iw > 0 && ih > 0) {
            // clear inner rounded rect by painting transparent
            Uint8 *px = static_cast<Uint8*>(s->pixels);
            int pitch = s->pitch;
            for (int y = thickness; y < h - thickness; y++) {
                for (int x = thickness; x < w - thickness; x++) {
                    if (inside_rounded(x - thickness, y - thickness, iw, ih, radius - thickness)) {
                        reinterpret_cast<Uint32*>(px + y * pitch)[x] = 0;
                    }
                }
            }
        }
        TexturePtr t = r.create_texture(*s);
        Foyer::free_surface(s);
        return t;
    }
}
