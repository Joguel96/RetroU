// One-card PC prototype for the RetroU library UI.
// This is intentionally a standalone target; it does not touch the Wii U build.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int kWindowW = 640;
constexpr int kWindowH = 480;
constexpr int kCardW = 190;
constexpr int kCardH = 250;
constexpr int kCoverH = 150;

struct Color {
    uint8_t r, g, b, a;
};

constexpr Color kText = {241, 245, 249, 255};
constexpr Color kMuted = {148, 163, 184, 255};
constexpr Color kGreen = {52, 211, 153, 255};
constexpr Color kGold = {245, 180, 40, 255};
constexpr Color kFocus = {77, 163, 255, 255};
constexpr Color kPanel = {15, 23, 42, 255};

void setColor(SDL_Renderer *r, Color c)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillRect(SDL_Renderer *r, int x, int y, int w, int h, Color c)
{
    setColor(r, c);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void fillRoundRect(SDL_Renderer *r, int x, int y, int w, int h, int radius, Color c)
{
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    setColor(r, c);
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius) {
            int dy = radius - row - 1;
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        } else if (row >= h - radius) {
            int dy = row - (h - radius);
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        }
        SDL_RenderDrawLine(r, x + inset, y + row, x + w - inset - 1, y + row);
    }
}

void outlineRoundRect(SDL_Renderer *r, int x, int y, int w, int h, int radius, Color c)
{
    setColor(r, c);
    for (int i = 0; i < 3; ++i) {
        fillRoundRect(r, x + i, y + i, w - i * 2, h - i * 2, radius - i, c);
    }
    // Clear the interior so only the ring remains.
    setColor(r, Color{0, 0, 0, 0});
    (void)r;
}

void drawStar(SDL_Renderer *r, int cx, int cy, int outer, int inner, Color c)
{
    SDL_Vertex vertices[12]{};
    vertices[0].position = {static_cast<float>(cx), static_cast<float>(cy)};
    vertices[0].color = {c.r, c.g, c.b, c.a};
    vertices[0].tex_coord = {0, 0};

    for (int i = 0; i < 10; ++i) {
        float angle = -3.14159265f / 2.0f + i * 3.14159265f / 5.0f;
        int radius = (i % 2 == 0) ? outer : inner;
        vertices[i + 1].position = {
            static_cast<float>(cx + std::cos(angle) * radius),
            static_cast<float>(cy + std::sin(angle) * radius)};
        vertices[i + 1].color = {c.r, c.g, c.b, c.a};
        vertices[i + 1].tex_coord = {0, 0};
    }

    int indices[30];
    for (int i = 0; i < 10; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % 10 + 1;
    }
    SDL_RenderGeometry(r, nullptr, vertices, 11, indices, 30);
}

class TextCache {
public:
    TextCache(SDL_Renderer *renderer, const std::string &fontPath)
        : renderer_(renderer), fontPath_(fontPath)
    {
    }

    ~TextCache()
    {
        for (auto &item : cache_) {
            SDL_DestroyTexture(item.second);
        }
        for (auto &item : fonts_) {
            TTF_CloseFont(item.second);
        }
    }

    TTF_Font *font(int size)
    {
        auto it = fonts_.find(size);
        if (it != fonts_.end()) return it->second;
        TTF_Font *f = TTF_OpenFont(fontPath_.c_str(), size);
        if (!f) {
            std::fprintf(stderr, "TTF_OpenFont(%s): %s\n", fontPath_.c_str(), TTF_GetError());
        }
        fonts_[size] = f;
        return f;
    }

    SDL_Texture *get(const std::string &text, int size, Color color)
    {
        char key[256];
        std::snprintf(key, sizeof(key), "%d:%u:%u:%u:%s", size, color.r, color.g, color.b,
                      text.c_str());
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        TTF_Font *f = font(size);
        if (!f) return nullptr;
        SDL_Color c{color.r, color.g, color.b, color.a};
        SDL_Surface *surface = TTF_RenderUTF8_Blended(f, text.c_str(), c);
        if (!surface) return nullptr;
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);
        cache_[key] = texture;
        return texture;
    }

    int draw(const std::string &text, int size, Color color, int x, int y)
    {
        SDL_Texture *texture = get(text, size, color);
        if (!texture) return 0;
        int w = 0, h = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
        SDL_Rect dst{x, y, w, h};
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
        return w;
    }

    std::string ellipsize(const std::string &text, int size, int maxWidth)
    {
        TTF_Font *f = font(size);
        if (!f) return text;
        int w = 0, h = 0;
        TTF_SizeUTF8(f, text.c_str(), &w, &h);
        if (w <= maxWidth) return text;

        std::string result = text;
        while (!result.empty()) {
            result.pop_back();
            std::string candidate = result + "…";
            TTF_SizeUTF8(f, candidate.c_str(), &w, &h);
            if (w <= maxWidth) return candidate;
        }
        return "…";
    }

private:
    SDL_Renderer *renderer_;
    std::string fontPath_;
    std::map<int, TTF_Font *> fonts_;
    std::map<std::string, SDL_Texture *> cache_;
};

SDL_Texture *makeGradient(SDL_Renderer *renderer, int w, int h, int radius)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STATIC, w, h);
    if (!texture) return nullptr;

    std::vector<uint32_t> pixels(static_cast<size_t>(w) * h);
    const Color top{34, 197, 94, 255};
    const Color middle{245, 158, 11, 255};
    const Color bottom{147, 51, 234, 255};

    for (int y = 0; y < h; ++y) {
        float t = static_cast<float>(y) / std::max(1, h - 1);
        Color c;
        if (t < 0.5f) {
            float u = t * 2.0f;
            c = {static_cast<uint8_t>(top.r + (middle.r - top.r) * u),
                 static_cast<uint8_t>(top.g + (middle.g - top.g) * u),
                 static_cast<uint8_t>(top.b + (middle.b - top.b) * u), 255};
        } else {
            float u = (t - 0.5f) * 2.0f;
            c = {static_cast<uint8_t>(middle.r + (bottom.r - middle.r) * u),
                 static_cast<uint8_t>(middle.g + (bottom.g - middle.g) * u),
                 static_cast<uint8_t>(middle.b + (bottom.b - middle.b) * u), 255};
        }

        for (int x = 0; x < w; ++x) {
            int inset = 0;
            if (y < radius) {
                int dy = radius - y - 1;
                inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
            }
            uint8_t alpha = (x < inset || x >= w - inset) ? 0 : 255;
            uint32_t px = (static_cast<uint32_t>(alpha) << 24) |
                          (static_cast<uint32_t>(c.r) << 16) |
                          (static_cast<uint32_t>(c.g) << 8) | c.b;
            pixels[static_cast<size_t>(y) * w + x] = px;
        }
    }

    SDL_UpdateTexture(texture, nullptr, pixels.data(), w * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

void drawChip(SDL_Renderer *r, TextCache &text, int x, int y, const std::string &label)
{
    int tw = 0, th = 0;
    TTF_Font *f = text.font(10);
    if (f) TTF_SizeUTF8(f, label.c_str(), &tw, &th);
    int w = tw + 18;
    fillRoundRect(r, x, y, w, 22, 7, Color{15, 23, 42, 180});
    text.draw(label, 10, Color{248, 250, 252, 255}, x + 9, y + 5);
}

} // namespace

int main(int, char **)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("RetroU - one card prototype",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWindowW, kWindowH, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!window || !renderer) {
        std::fprintf(stderr, "window/renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    TextCache text(renderer, "content/ui.ttf");
    SDL_Texture *icon = IMG_LoadTexture(renderer, "assets/icon.png");
    if (!icon) {
        std::fprintf(stderr, "IMG_LoadTexture: %s\n", IMG_GetError());
    }
    SDL_Texture *gradient = makeGradient(renderer, kCardW, kCoverH, 16);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }

        // Background.
        for (int y = 0; y < kWindowH; y += 8) {
            float t = static_cast<float>(y) / kWindowH;
            fillRect(renderer, 0, y, kWindowW, 8,
                     Color{static_cast<uint8_t>(8 + 5 * t),
                           static_cast<uint8_t>(13 + 6 * t),
                           static_cast<uint8_t>(24 + 12 * t), 255});
        }
        fillRoundRect(renderer, 120, 40, 400, 40, 14, Color{18, 27, 43, 180});

        const int x = (kWindowW - kCardW) / 2;
        const int y = (kWindowH - kCardH) / 2;

        // Focus shadow and border.
        fillRoundRect(renderer, x - 6, y - 6, kCardW + 12, kCardH + 12, 20,
                      Color{77, 163, 255, 45});
        fillRoundRect(renderer, x - 3, y - 3, kCardW + 6, kCardH + 6, 18, kFocus);
        fillRoundRect(renderer, x, y, kCardW, kCardH, 16, kPanel);

        // Cover.
        if (gradient) {
            SDL_Rect cover{x + 1, y + 1, kCardW - 2, kCoverH - 1};
            SDL_RenderCopy(renderer, gradient, nullptr, &cover);
        }

        // Icon.
        if (icon) {
            SDL_Rect iconRect{x + kCardW / 2 - 32, y + 42, 64, 64};
            SDL_RenderCopy(renderer, icon, nullptr, &iconRect);
        }

        drawChip(renderer, text, x + 10, y + 10, "SNES");
        drawStar(renderer, x + kCardW - 24, y + 22, 11, 4, kGold);

        // Featured badge.
        fillRoundRect(renderer, x + 10, y + 122, 78, 18, 6, Color{15, 23, 42, 170});
        text.draw("Destacado", 10, Color{255, 246, 212, 255}, x + 17, y + 125);

        // Footer panel.
        fillRect(renderer, x, y + kCoverH, kCardW, kCardH - kCoverH, Color{9, 14, 24, 255});
        fillRect(renderer, x + 10, y + kCoverH, kCardW - 20, 1, Color{255, 255, 255, 28});

        std::string title = text.ellipsize("Super Mario World", 16, kCardW - 28);
        text.draw(title, 16, kText, x + 14, y + kCoverH + 12);
        text.draw("Nintendo EAD · 1990", 12, kMuted, x + 14, y + kCoverH + 37);

        fillRect(renderer, x + 14, y + kCoverH + 58, kCardW - 28, 1,
                 Color{255, 255, 255, 24});
        text.draw("Plataformas", 12, Color{203, 213, 225, 255}, x + 14, y + kCoverH + 68);

        std::string runs = "142 ejec.";
        TTF_Font *small = text.font(12);
        int rw = 0, rh = 0;
        if (small) TTF_SizeUTF8(small, runs.c_str(), &rw, &rh);
        text.draw(runs, 12, kGreen, x + kCardW - 14 - rw, y + kCoverH + 68);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (gradient) SDL_DestroyTexture(gradient);
    if (icon) SDL_DestroyTexture(icon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
