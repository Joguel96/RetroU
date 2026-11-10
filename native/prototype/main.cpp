// RetroU PC prototype: 5-column grid, shared focus, scroll and virtualization.
// The Wii U apps are not touched by this target.
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

constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;
constexpr int kColumns = 5;
constexpr int kCardW = 190;
constexpr int kCardH = 250;
constexpr int kCoverH = 150;

struct Color {
    uint8_t r, g, b, a;
};

constexpr Color kText = {241, 245, 249, 255};
constexpr Color kMuted = {148, 163, 184, 255};
constexpr Color kGold = {245, 180, 40, 255};
constexpr Color kFocus = {77, 163, 255, 255};
constexpr Color kPanel = {15, 23, 42, 255};

struct Game {
    std::string title;
    std::string developer;
    std::string genre;
    int year;
    bool featured;
    bool favorite;
    Color gradientTop;
    Color gradientMid;
    Color gradientBottom;
};

const std::vector<Game> kGames = {
    {"Super Mario World", "Nintendo EAD", "Plataformas", 1990, true, true,
     {34, 197, 94, 255}, {245, 158, 11, 255}, {147, 51, 234, 255}},
    {"Super Metroid", "Nintendo R&D1", "Aventura", 1994, false, true,
     {15, 118, 110, 255}, {59, 130, 246, 255}, {76, 29, 149, 255}},
    {"The Legend of Zelda", "Nintendo EAD", "Aventura", 1991, true, false,
     {22, 163, 74, 255}, {234, 179, 8, 255}, {180, 83, 9, 255}},
    {"Chrono Trigger", "Square", "RPG", 1995, true, false,
     {120, 53, 15, 255}, {217, 119, 6, 255}, {69, 26, 3, 255}},
    {"Donkey Kong Country", "Rare", "Plataformas", 1994, false, false,
     {13, 148, 136, 255}, {245, 158, 11, 255}, {124, 45, 18, 255}},
    {"Mega Man X", "Capcom", "Acción", 1993, false, true,
     {30, 64, 175, 255}, {59, 130, 246, 255}, {88, 28, 135, 255}},
    {"Final Fantasy VI", "Square", "RPG", 1994, false, false,
     {76, 29, 149, 255}, {147, 51, 234, 255}, {30, 27, 75, 255}},
    {"EarthBound", "Ape / HAL", "RPG", 1994, false, false,
     {190, 24, 93, 255}, {244, 114, 182, 255}, {88, 28, 135, 255}},
    {"F-Zero", "Nintendo EAD", "Carreras", 1990, false, false,
     {30, 58, 138, 255}, {14, 165, 233, 255}, {15, 23, 42, 255}},
    {"Star Fox", "Nintendo EAD", "Shooter", 1993, false, false,
     {15, 23, 42, 255}, {71, 85, 105, 255}, {148, 163, 184, 255}},
    {"Kirby Super Star", "HAL Laboratory", "Plataformas", 1996, false, true,
     {219, 39, 119, 255}, {251, 146, 60, 255}, {126, 34, 206, 255}},
    {"Street Fighter II", "Capcom", "Lucha", 1992, false, false,
     {153, 27, 27, 255}, {239, 68, 68, 255}, {69, 10, 10, 255}},
    {"Pilotwings", "Nintendo EAD", "Simulación", 1990, false, false,
     {14, 116, 144, 255}, {34, 197, 94, 255}, {15, 23, 42, 255}},
    {"ActRaiser", "Quintet", "Acción", 1990, false, false,
     {88, 28, 135, 255}, {192, 38, 211, 255}, {30, 27, 75, 255}},
};

struct Layout {
    int margin;
    int gap;
    int cardW;
    int cardH;
    int top;
    int visibleRows;
};

Layout makeLayout(int width, int height)
{
    Layout l;
    l.margin = std::max(20, width / 10);
    l.gap = width >= 1000 ? 16 : 12;
    l.cardW = std::min(kCardW, (width - l.margin * 2 - l.gap * (kColumns - 1)) / kColumns);
    l.cardH = l.cardW * kCardH / kCardW;
    l.top = 92;
    int bottom = 58;
    int available = height - l.top - bottom;
    l.visibleRows = std::max(1, (available + l.gap) / (l.cardH + l.gap));
    return l;
}

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
        : renderer_(renderer), fontPath_(fontPath) {}

    ~TextCache()
    {
        for (auto &item : cache_) SDL_DestroyTexture(item.second);
        for (auto &item : fonts_) TTF_CloseFont(item.second);
    }

    TTF_Font *font(int size)
    {
        auto it = fonts_.find(size);
        if (it != fonts_.end()) return it->second;
        TTF_Font *f = TTF_OpenFont(fontPath_.c_str(), size);
        if (!f) std::fprintf(stderr, "TTF_OpenFont(%s): %s\n", fontPath_.c_str(), TTF_GetError());
        fonts_[size] = f;
        return f;
    }

    int draw(const std::string &text, int size, Color color, int x, int y)
    {
        char key[256];
        std::snprintf(key, sizeof(key), "%d:%u:%u:%u:%s", size, color.r, color.g, color.b,
                      text.c_str());
        SDL_Texture *texture = nullptr;
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            texture = it->second;
        } else {
            TTF_Font *f = font(size);
            if (!f) return 0;
            SDL_Color c{color.r, color.g, color.b, color.a};
            SDL_Surface *surface = TTF_RenderUTF8_Blended(f, text.c_str(), c);
            if (!surface) return 0;
            texture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_FreeSurface(surface);
            cache_[key] = texture;
        }
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

class GradientCache {
public:
    explicit GradientCache(SDL_Renderer *renderer) : renderer_(renderer) {}

    ~GradientCache()
    {
        for (auto &item : cache_) SDL_DestroyTexture(item.second);
    }

    SDL_Texture *get(const Game &game, int w, int h, int radius)
    {
        uint32_t key = (static_cast<uint32_t>(game.gradientTop.r) << 24) |
                       (static_cast<uint32_t>(game.gradientMid.g) << 16) |
                       (static_cast<uint32_t>(game.gradientBottom.b) << 8) |
                       static_cast<uint32_t>(w);
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                                 SDL_TEXTUREACCESS_STATIC, w, h);
        if (!texture) return nullptr;

        std::vector<uint32_t> pixels(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y) {
            float t = static_cast<float>(y) / std::max(1, h - 1);
            Color c;
            if (t < 0.5f) {
                float u = t * 2.0f;
                c = {static_cast<uint8_t>(game.gradientTop.r + (game.gradientMid.r - game.gradientTop.r) * u),
                     static_cast<uint8_t>(game.gradientTop.g + (game.gradientMid.g - game.gradientTop.g) * u),
                     static_cast<uint8_t>(game.gradientTop.b + (game.gradientMid.b - game.gradientTop.b) * u), 255};
            } else {
                float u = (t - 0.5f) * 2.0f;
                c = {static_cast<uint8_t>(game.gradientMid.r + (game.gradientBottom.r - game.gradientMid.r) * u),
                     static_cast<uint8_t>(game.gradientMid.g + (game.gradientBottom.g - game.gradientMid.g) * u),
                     static_cast<uint8_t>(game.gradientMid.b + (game.gradientBottom.b - game.gradientMid.b) * u), 255};
            }
            for (int x = 0; x < w; ++x) {
                int inset = 0;
                if (y < radius) {
                    int dy = radius - y - 1;
                    inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
                }
                uint8_t alpha = (x < inset || x >= w - inset) ? 0 : 255;
                pixels[static_cast<size_t>(y) * w + x] =
                    (static_cast<uint32_t>(alpha) << 24) |
                    (static_cast<uint32_t>(c.r) << 16) |
                    (static_cast<uint32_t>(c.g) << 8) | c.b;
            }
        }
        SDL_UpdateTexture(texture, nullptr, pixels.data(), w * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        cache_[key] = texture;
        return texture;
    }

private:
    SDL_Renderer *renderer_;
    std::map<uint32_t, SDL_Texture *> cache_;
};

void drawCard(SDL_Renderer *r, TextCache &text, GradientCache &gradients,
              SDL_Texture *icon, const Game &game, int x, int y, int w, int h, bool focused)
{
    int coverH = h * 60 / 100;
    int radius = std::max(10, w / 16);

    fillRoundRect(r, x - 5, y - 5, w + 10, h + 10, radius + 4, Color{0, 0, 0, 95});
    if (focused) {
        fillRoundRect(r, x - 3, y - 3, w + 6, h + 6, radius + 3, kFocus);
    }
    fillRoundRect(r, x, y, w, h, radius, kPanel);

    SDL_Texture *gradient = gradients.get(game, w - 2, coverH - 1, radius);
    if (gradient) {
        SDL_Rect cover{x + 1, y + 1, w - 2, coverH - 1};
        SDL_RenderCopy(r, gradient, nullptr, &cover);
    }

    if (icon) {
        int iconSize = std::max(42, w / 3);
        SDL_Rect dst{x + w / 2 - iconSize / 2, y + coverH / 2 - iconSize / 2,
                     iconSize, iconSize};
        SDL_RenderCopy(r, icon, nullptr, &dst);
    }

    if (game.favorite) {
        drawStar(r, x + w - 24, y + 22, 11, 4, kGold);
    }

    if (game.featured) {
        fillRoundRect(r, x + 10, y + coverH - 28, 78, 18, 6, Color{15, 23, 42, 170});
        text.draw("Destacado", 10, Color{255, 246, 212, 255}, x + 17, y + coverH - 25);
    }

    fillRect(r, x, y + coverH, w, h - coverH, Color{9, 14, 24, 255});
    fillRect(r, x + 10, y + coverH, w - 20, 1, Color{255, 255, 255, 28});

    int pad = 14;
    std::string title = text.ellipsize(game.title, 16, w - pad * 2);
    text.draw(title, 16, kText, x + pad, y + coverH + 12);
    text.draw(game.developer + " · " + std::to_string(game.year), 12, kMuted,
              x + pad, y + coverH + 38);
    text.draw(game.genre, 12, Color{203, 213, 225, 255}, x + pad, y + coverH + 68);
}

} // namespace

int main(int, char **)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("RetroU - grid prototype",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWindowW, kWindowH, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (!window || !renderer) {
        std::fprintf(stderr, "window/renderer: %s\n", SDL_GetError());
        return 1;
    }

    TextCache text(renderer, "content/ui.ttf");
    GradientCache gradients(renderer);
    SDL_Texture *icon = IMG_LoadTexture(renderer, "assets/icon.png");

    SDL_GameController *controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }

    Layout layout = makeLayout(kWindowW, kWindowH);
    int focus = 0;
    int scrollRow = 0;
    bool running = true;

    auto ensureVisible = [&]() {
        int row = focus / kColumns;
        int rows = static_cast<int>(kGames.size()) / kColumns + 1;
        if (row < scrollRow) scrollRow = row;
        if (row >= scrollRow + layout.visibleRows) {
            scrollRow = row - layout.visibleRows + 1;
        }
        scrollRow = std::max(0, std::min(scrollRow, std::max(0, rows - layout.visibleRows)));
    };

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                        focus = std::max(0, focus - 1);
                        ensureVisible();
                        break;
                    case SDLK_RIGHT:
                        focus = std::min(static_cast<int>(kGames.size()) - 1, focus + 1);
                        ensureVisible();
                        break;
                    case SDLK_UP:
                        focus = std::max(0, focus - kColumns);
                        ensureVisible();
                        break;
                    case SDLK_DOWN:
                        focus = std::min(static_cast<int>(kGames.size()) - 1, focus + kColumns);
                        ensureVisible();
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    default:
                        break;
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        focus = std::max(0, focus - 1);
                        ensureVisible();
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        focus = std::min(static_cast<int>(kGames.size()) - 1, focus + 1);
                        ensureVisible();
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        focus = std::max(0, focus - kColumns);
                        ensureVisible();
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        focus = std::min(static_cast<int>(kGames.size()) - 1, focus + kColumns);
                        ensureVisible();
                        break;
                    default:
                        break;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int col = (event.button.x - layout.margin) / (layout.cardW + layout.gap);
                int row = (event.button.y - layout.top) / (layout.cardH + layout.gap);
                if (col >= 0 && col < kColumns) {
                    int index = (scrollRow + row) * kColumns + col;
                    if (index >= 0 && index < static_cast<int>(kGames.size())) {
                        focus = index;
                        ensureVisible();
                    }
                }
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

        // Header.
        text.draw("RetroSNES U", 30, kText, 40, 22);
        text.draw(std::to_string(kGames.size()) + " juegos", 13, kMuted, 42, 60);
        text.draw("5 COLUMNAS", 12, kMuted, kWindowW - 150, 30);

        // Virtualized rows: only the visible rows are drawn.
        int first = scrollRow;
        int last = std::min(static_cast<int>((kGames.size() + kColumns - 1) / kColumns),
                            scrollRow + layout.visibleRows);
        for (int row = first; row < last; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                int index = row * kColumns + col;
                if (index >= static_cast<int>(kGames.size())) break;
                int x = layout.margin + col * (layout.cardW + layout.gap);
                int y = layout.top + (row - scrollRow) * (layout.cardH + layout.gap);
                drawCard(renderer, text, gradients, icon, kGames[index], x, y,
                         layout.cardW, layout.cardH, index == focus);
            }
        }

        // Footer.
        fillRect(renderer, 0, kWindowH - 52, kWindowW, 52, Color{7, 12, 22, 230});
        fillRect(renderer, 0, kWindowH - 52, kWindowW, 1, Color{255, 255, 255, 26});
        text.draw("D-PAD  NAVEGAR", 12, kMuted, 40, kWindowH - 32);
        text.draw("A  JUGAR      X  FAVORITO", 12, kMuted, 210, kWindowH - 32);
        text.draw("PLUS  MENÚ", 12, kGold, kWindowW - 130, kWindowH - 32);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyTexture(icon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
