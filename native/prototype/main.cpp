// RetroU PC prototype: theme, static/dynamic background, blur, favorites and
// Libretro cover download. Isolated from the Wii U build.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <urlmon.h>
#endif

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

struct Theme {
    Color backgroundTop{8, 13, 24, 255};
    Color backgroundBottom{3, 6, 13, 255};
    Color panel{11, 18, 32, 255};
    Color focus{77, 163, 255, 255};
    Color text{248, 250, 252, 255};
    Color muted{148, 163, 184, 255};
    Color accent{245, 217, 127, 255};
    uint8_t panelAlpha = 220;

    std::string backgroundImage;
    std::string backgroundType = "dynamic"; // color | image | dynamic
    bool blur = true;
    float blurStrength = 0.5f;
    int dynamicDelayMs = 5000;
    bool favoritesFirst = false;
};

struct Game {
    std::string id;
    std::string title;
    std::string developer;
    std::string genre;
    std::string coverPath;
    std::string backdropPath;
    int year = 0;
    bool featured = false;
    bool favorite = false;
    Color gradientTop{34, 197, 94, 255};
    Color gradientMid{245, 158, 11, 255};
    Color gradientBottom{147, 51, 234, 255};
};

const std::vector<Game> kFallbackGames = {
    {"smw", "Super Mario World", "Nintendo EAD", "Plataformas", "", "", 1990, true, true,
     {34, 197, 94, 255}, {245, 158, 11, 255}, {147, 51, 234, 255}},
    {"metroid", "Super Metroid", "Nintendo R&D1", "Aventura", "", "", 1994, false, true,
     {15, 118, 110, 255}, {59, 130, 246, 255}, {76, 29, 149, 255}},
    {"zelda", "The Legend of Zelda", "Nintendo EAD", "Aventura", "", "", 1991, true, false,
     {22, 163, 74, 255}, {234, 179, 8, 255}, {180, 83, 9, 255}},
    {"chrono", "Chrono Trigger", "Square", "RPG", "", "", 1995, true, false,
     {120, 53, 15, 255}, {217, 119, 6, 255}, {69, 26, 3, 255}},
    {"dkc", "Donkey Kong Country", "Rare", "Plataformas", "", "", 1994, false, false,
     {13, 148, 136, 255}, {245, 158, 11, 255}, {124, 45, 18, 255}},
    {"megamanx", "Mega Man X", "Capcom", "Acción", "", "", 1993, false, true,
     {30, 64, 175, 255}, {59, 130, 246, 255}, {88, 28, 135, 255}},
    {"ff6", "Final Fantasy VI", "Square", "RPG", "", "", 1994, false, false,
     {76, 29, 149, 255}, {147, 51, 234, 255}, {30, 27, 75, 255}},
    {"earthbound", "EarthBound", "Ape / HAL", "RPG", "", "", 1994, false, false,
     {190, 24, 93, 255}, {244, 114, 182, 255}, {88, 28, 135, 255}},
    {"fzero", "F-Zero", "Nintendo EAD", "Carreras", "", "", 1990, false, false,
     {30, 58, 138, 255}, {14, 165, 233, 255}, {15, 23, 42, 255}},
    {"starfox", "Star Fox", "Nintendo EAD", "Shooter", "", "", 1993, false, false,
     {15, 23, 42, 255}, {71, 85, 105, 255}, {148, 163, 184, 255}},
    {"kirby", "Kirby Super Star", "HAL Laboratory", "Plataformas", "", "", 1996, false, true,
     {219, 39, 119, 255}, {251, 146, 60, 255}, {126, 34, 206, 255}},
    {"sf2", "Street Fighter II", "Capcom", "Lucha", "", "", 1992, false, false,
     {153, 27, 27, 255}, {239, 68, 68, 255}, {69, 10, 10, 255}},
    {"pilotwings", "Pilotwings", "Nintendo EAD", "Simulación", "", "", 1990, false, false,
     {14, 116, 144, 255}, {34, 197, 94, 255}, {15, 23, 42, 255}},
    {"actraiser", "ActRaiser", "Quintet", "Acción", "", "", 1990, false, false,
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
    int available = height - l.top - 58;
    l.visibleRows = std::max(1, (available + l.gap) / (l.cardH + l.gap));
    return l;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

Color colorFromJson(const nlohmann::json &j, const char *key, Color fallback)
{
    if (!j.contains(key) || !j[key].is_string()) return fallback;
    std::string s = j[key].get<std::string>();
    if (s.size() != 7 || s[0] != '#') return fallback;
    return {static_cast<uint8_t>(hexDigit(s[1]) * 16 + hexDigit(s[2])),
            static_cast<uint8_t>(hexDigit(s[3]) * 16 + hexDigit(s[4])),
            static_cast<uint8_t>(hexDigit(s[5]) * 16 + hexDigit(s[6])), 255};
}

Theme loadTheme()
{
    Theme t;
    std::ifstream in("data/theme.json");
    if (!in) return t;
    try {
        nlohmann::json j;
        in >> j;
        if (j.contains("background")) {
            const auto &bg = j["background"];
            t.backgroundType = bg.value("type", t.backgroundType);
            t.backgroundTop = colorFromJson(bg, "colorTop", t.backgroundTop);
            t.backgroundBottom = colorFromJson(bg, "colorBottom", t.backgroundBottom);
            t.backgroundImage = bg.value("image", t.backgroundImage);
            t.blur = bg.value("blur", t.blur);
            t.blurStrength = bg.value("blurStrength", t.blurStrength);
            t.dynamicDelayMs = bg.value("dynamicDelayMs", t.dynamicDelayMs);
        }
        if (j.contains("panel")) {
            t.panel = colorFromJson(j["panel"], "color", t.panel);
            t.panelAlpha = static_cast<uint8_t>(j["panel"].value("alpha", 0.86) * 255);
        }
        t.focus = colorFromJson(j, "focus", t.focus);
        t.accent = colorFromJson(j, "accent", t.accent);
        t.text = colorFromJson(j, "text", t.text);
        t.muted = colorFromJson(j, "muted", t.muted);
        if (j.contains("sort")) t.favoritesFirst = j["sort"].value("favoritesFirst", false);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "theme.json: %s\n", e.what());
    }
    return t;
}

std::vector<Game> loadGames()
{
    std::ifstream in("data/games.json");
    if (!in) return kFallbackGames;
    try {
        nlohmann::json root;
        in >> root;
        std::vector<Game> games;
        for (const auto &item : root.at("games")) {
            Game g;
            g.id = item.value("id", "");
            g.title = item.value("title", "");
            g.developer = item.value("developer", "");
            g.genre = item.value("genre", "");
            g.coverPath = item.value("coverPath", "");
            g.backdropPath = item.value("backdropPath", "");
            g.year = item.value("year", 0);
            g.featured = item.value("featured", false);
            g.favorite = item.value("favorite", false);
            g.gradientTop = colorFromJson(item, "gradientTop", g.gradientTop);
            g.gradientMid = colorFromJson(item, "gradientMid", g.gradientMid);
            g.gradientBottom = colorFromJson(item, "gradientBottom", g.gradientBottom);
            if (!g.id.empty() && !g.title.empty()) games.push_back(g);
        }
        return games;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "games.json: %s\n", e.what());
        return kFallbackGames;
    }
}

std::string colorToJson(Color c)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", c.r, c.g, c.b);
    return buffer;
}

void saveGames(const std::vector<Game> &games)
{
    nlohmann::json root;
    root["games"] = nlohmann::json::array();
    for (const Game &g : games) {
        root["games"].push_back({
            {"id", g.id}, {"title", g.title}, {"developer", g.developer},
            {"genre", g.genre}, {"coverPath", g.coverPath}, {"backdropPath", g.backdropPath},
            {"year", g.year}, {"featured", g.featured}, {"favorite", g.favorite},
            {"gradientTop", colorToJson(g.gradientTop)},
            {"gradientMid", colorToJson(g.gradientMid)},
            {"gradientBottom", colorToJson(g.gradientBottom)},
        });
    }
    std::ofstream out("data/games.json", std::ios::trunc);
    if (out) out << root.dump(2) << "\n";
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

void boxBlur(SDL_Surface *surface, int radius)
{
    if (!surface || radius <= 0) return;
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!rgba) return;
    SDL_LockSurface(rgba);
    const int w = rgba->w, h = rgba->h;
    std::vector<uint32_t> src(static_cast<size_t>(w) * h);
    std::vector<uint32_t> tmp(src.size());
    std::memcpy(src.data(), rgba->pixels, src.size() * 4);

    auto blurPass = [&](bool horizontal) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int rr = 0, gg = 0, bb = 0, count = 0;
                for (int d = -radius; d <= radius; ++d) {
                    int sx = horizontal ? std::min(w - 1, std::max(0, x + d)) : x;
                    int sy = horizontal ? y : std::min(h - 1, std::max(0, y + d));
                    uint32_t p = src[static_cast<size_t>(sy) * w + sx];
                    rr += (p >> 16) & 0xff;
                    gg += (p >> 8) & 0xff;
                    bb += p & 0xff;
                    ++count;
                }
                tmp[static_cast<size_t>(y) * w + x] =
                    (0xffu << 24) | ((rr / count) << 16) | ((gg / count) << 8) | (bb / count);
            }
        }
        src.swap(tmp);
    };
    blurPass(true);
    blurPass(false);
    std::memcpy(rgba->pixels, src.data(), src.size() * 4);
    SDL_UnlockSurface(rgba);
    SDL_BlitSurface(rgba, nullptr, surface, nullptr);
    SDL_FreeSurface(rgba);
}

class TextureCache {
public:
    explicit TextureCache(SDL_Renderer *renderer) : renderer_(renderer) {}
    ~TextureCache()
    {
        for (auto &item : cache_) SDL_DestroyTexture(item.second);
    }

    SDL_Texture *get(const std::string &path, bool blur, float strength)
    {
        if (path.empty()) return nullptr;
        char key[512];
        std::snprintf(key, sizeof(key), "%s|%d|%.2f", path.c_str(), blur ? 1 : 0, strength);
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        SDL_Surface *surface = IMG_Load(path.c_str());
        if (!surface) return nullptr;
        if (blur) {
            SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, 640, 360, 32,
                                                                  SDL_PIXELFORMAT_ARGB8888);
            if (scaled) {
                SDL_BlitScaled(surface, nullptr, scaled, nullptr);
                boxBlur(scaled, std::max(1, static_cast<int>(strength * 12.0f)));
                SDL_FreeSurface(surface);
                surface = scaled;
            }
        }
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            cache_[key] = texture;
        }
        return texture;
    }

private:
    SDL_Renderer *renderer_;
    std::map<std::string, SDL_Texture *> cache_;
};

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
        char key[320];
        std::snprintf(key, sizeof(key), "%d:%u:%u:%u:%s", size, color.r, color.g, color.b,
                      text.c_str());
        SDL_Texture *texture = nullptr;
        auto it = cache_.find(key);
        if (it != cache_.end()) texture = it->second;
        else {
            TTF_Font *f = font(size);
            if (!f) return 0;
            SDL_Surface *surface = TTF_RenderUTF8_Blended(f, text.c_str(),
                                                          SDL_Color{color.r, color.g, color.b, color.a});
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
                       (static_cast<uint32_t>(game.gradientBottom.b) << 8) | w;
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

class Background {
public:
    Background(SDL_Renderer *renderer, TextureCache &cache, const Theme &theme)
        : renderer_(renderer), cache_(cache), theme_(theme) {}

    void reload()
    {
        current_ = cache_.get(theme_.backgroundImage, theme_.blur, theme_.blurStrength);
        currentPath_ = theme_.backgroundImage;
        old_ = nullptr;
        transitionStart_ = 0;
    }

    void update(int focus, const Game *game, Uint32 now)
    {
        if (focus != lastFocus_) {
            lastFocus_ = focus;
            focusSince_ = now;
        }
        if (theme_.backgroundType != "dynamic" || !game) return;
        if (now - focusSince_ < static_cast<Uint32>(std::max(0, theme_.dynamicDelayMs))) return;
        std::string path = !game->backdropPath.empty() ? game->backdropPath : game->coverPath;
        if (path.empty()) path = theme_.backgroundImage;
        if (path.empty() || path == currentPath_) return;

        SDL_Texture *next = cache_.get(path, theme_.blur, theme_.blurStrength);
        if (!next) return;
        old_ = current_;
        current_ = next;
        currentPath_ = path;
        transitionStart_ = now;
    }

    void draw(int width, int height)
    {
        if (theme_.backgroundType == "color" || !current_) {
            for (int y = 0; y < height; y += 8) {
                float t = static_cast<float>(y) / std::max(1, height);
                Color c{static_cast<uint8_t>(theme_.backgroundTop.r +
                                             (theme_.backgroundBottom.r - theme_.backgroundTop.r) * t),
                        static_cast<uint8_t>(theme_.backgroundTop.g +
                                             (theme_.backgroundBottom.g - theme_.backgroundTop.g) * t),
                        static_cast<uint8_t>(theme_.backgroundTop.b +
                                             (theme_.backgroundBottom.b - theme_.backgroundTop.b) * t),
                        255};
                fillRect(renderer_, 0, y, width, 8, c);
            }
            return;
        }

        if (old_ && transitionStart_) {
            float t = std::min(1.0f, (SDL_GetTicks() - transitionStart_) / 500.0f);
            SDL_Rect dst{0, 0, width, height};
            SDL_SetTextureAlphaMod(old_, static_cast<Uint8>((1.0f - t) * 255));
            SDL_RenderCopy(renderer_, old_, nullptr, &dst);
            SDL_SetTextureAlphaMod(current_, static_cast<Uint8>(t * 255));
            SDL_RenderCopy(renderer_, current_, nullptr, &dst);
            SDL_SetTextureAlphaMod(current_, 255);
            if (t >= 1.0f) old_ = nullptr;
        } else {
            SDL_Rect dst{0, 0, width, height};
            SDL_RenderCopy(renderer_, current_, nullptr, &dst);
        }
    }

private:
    SDL_Renderer *renderer_;
    TextureCache &cache_;
    const Theme &theme_;
    SDL_Texture *current_ = nullptr;
    SDL_Texture *old_ = nullptr;
    std::string currentPath_;
    int lastFocus_ = -1;
    Uint32 focusSince_ = 0;
    Uint32 transitionStart_ = 0;
};

void drawCard(SDL_Renderer *r, TextCache &text, GradientCache &gradients,
              TextureCache &covers, SDL_Texture *icon, const Theme &theme,
              const Game &game, int x, int y, int w, int h, bool focused)
{
    int coverH = h * 60 / 100;
    int radius = std::max(10, w / 16);
    Color panel = theme.panel;
    panel.a = theme.panelAlpha;

    fillRoundRect(r, x - 5, y - 5, w + 10, h + 10, radius + 4, Color{0, 0, 0, 95});
    if (focused) fillRoundRect(r, x - 3, y - 3, w + 6, h + 6, radius + 3, theme.focus);
    fillRoundRect(r, x, y, w, h, radius, panel);

    SDL_Texture *cover = covers.get(game.coverPath, false, 0);
    if (cover) {
        SDL_Rect dst{x + 1, y + 1, w - 2, coverH - 1};
        SDL_RenderCopy(r, cover, nullptr, &dst);
    } else {
        SDL_Texture *gradient = gradients.get(game, w - 2, coverH - 1, radius);
        if (gradient) {
            SDL_Rect dst{x + 1, y + 1, w - 2, coverH - 1};
            SDL_RenderCopy(r, gradient, nullptr, &dst);
        }
        if (icon) {
            int iconSize = std::max(42, w / 3);
            SDL_Rect dst{x + w / 2 - iconSize / 2, y + coverH / 2 - iconSize / 2,
                         iconSize, iconSize};
            SDL_RenderCopy(r, icon, nullptr, &dst);
        }
    }

    if (game.favorite) drawStar(r, x + w - 24, y + 22, 11, 4, theme.accent);
    if (game.featured) {
        fillRoundRect(r, x + 10, y + coverH - 28, 78, 18, 6, Color{15, 23, 42, 190});
        text.draw("Destacado", 10, Color{255, 246, 212, 255}, x + 17, y + coverH - 25);
    }

    fillRect(r, x, y + coverH, w, h - coverH, Color{9, 14, 24, 255});
    fillRect(r, x + 10, y + coverH, w - 20, 1, Color{255, 255, 255, 28});
    int pad = 14;
    text.draw(text.ellipsize(game.title, 16, w - pad * 2), 16, theme.text,
              x + pad, y + coverH + 12);
    text.draw(game.developer + " · " + std::to_string(game.year), 12, theme.muted,
              x + pad, y + coverH + 38);
    text.draw(game.genre, 12, theme.text, x + pad, y + coverH + 68);
}

bool gameTitleLess(const Game &a, const Game &b)
{
    std::string aa = a.title;
    std::string bb = b.title;
    std::transform(aa.begin(), aa.end(), aa.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(bb.begin(), bb.end(), bb.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return aa < bb;
}

std::string urlEncode(const std::string &value)
{
    std::string out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string libretroName(const std::string &title)
{
    std::string out = title;
    for (char &c : out) {
        if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) c = '_';
    }
    return out;
}

bool downloadFile(const std::string &url, const std::string &path)
{
#ifdef _WIN32
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), path.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
#else
    (void)url;
    (void)path;
    return false;
#endif
}

std::string libretroBoxartUrl(const std::string &title)
{
    const std::string system = "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System";
    return "https://thumbnails.libretro.com/" + system + "/Named_Boxarts/" +
           urlEncode(libretroName(title)) + ".png";
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

    SDL_Window *window = SDL_CreateWindow("RetroU - theme prototype",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWindowW, kWindowH, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    if (!window || !renderer) {
        std::fprintf(stderr, "window/renderer: %s\n", SDL_GetError());
        return 1;
    }

    Theme theme = loadTheme();
    std::vector<Game> games = loadGames();
    std::vector<int> visible;
    bool favoritesOnly = false;
    int focus = 0;
    int scrollRow = 0;
    std::string status = "Listo";

    TextCache text(renderer, "content/ui.ttf");
    GradientCache gradients(renderer);
    TextureCache textures(renderer);
    SDL_Texture *icon = IMG_LoadTexture(renderer, "assets/icon.png");
    Background background(renderer, textures, theme);
    background.reload();

    auto rebuildVisible = [&]() {
        visible.clear();
        for (int i = 0; i < static_cast<int>(games.size()); ++i) {
            if (!favoritesOnly || games[i].favorite) visible.push_back(i);
        }
        std::sort(visible.begin(), visible.end(), [&](int a, int b) {
            if (theme.favoritesFirst && games[a].favorite != games[b].favorite) {
                return games[a].favorite;
            }
            return gameTitleLess(games[a], games[b]);
        });
        if (visible.empty()) {
            focus = 0;
            scrollRow = 0;
        } else {
            focus = std::max(0, std::min(focus, static_cast<int>(visible.size()) - 1));
        }
    };

    auto ensureVisible = [&]() {
        Layout layout = makeLayout(kWindowW, kWindowH);
        int row = focus / kColumns;
        int rows = static_cast<int>((visible.size() + kColumns - 1) / kColumns);
        if (row < scrollRow) scrollRow = row;
        if (row >= scrollRow + layout.visibleRows) scrollRow = row - layout.visibleRows + 1;
        scrollRow = std::max(0, std::min(scrollRow, std::max(0, rows - layout.visibleRows)));
    };

    auto launch = [&]() {
        if (!visible.empty()) status = "Jugando: " + games[visible[focus]].title;
    };

    auto toggleFavorite = [&]() {
        if (visible.empty()) return;
        Game &g = games[visible[focus]];
        g.favorite = !g.favorite;
        status = g.favorite ? "Favorito agregado" : "Favorito quitado";
        saveGames(games);
        rebuildVisible();
        ensureVisible();
    };

    auto downloadCovers = [&]() {
        int downloaded = 0;
        const std::vector<std::string> regions = {"", " (USA)", " (Europe)", " (Japan)", " (World)"};
        for (Game &g : games) {
            if (g.coverPath.empty()) {
                std::string dest = "data/thumbnails/" + g.id + ".png";
                if (!std::filesystem::exists(dest)) {
                    bool ok = false;
                    for (const std::string &region : regions) {
                        if (downloadFile(libretroBoxartUrl(g.title + region), dest)) {
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) continue;
                }
                if (std::filesystem::exists(dest)) {
                    g.coverPath = dest;
                    ++downloaded;
                }
            }
        }
        saveGames(games);
        status = "Carátulas descargadas: " + std::to_string(downloaded);
    };

    rebuildVisible();
    ensureVisible();

    SDL_GameController *controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                        if (!visible.empty()) focus = std::max(0, focus - 1);
                        ensureVisible(); break;
                    case SDLK_RIGHT:
                        if (!visible.empty()) focus = std::min(static_cast<int>(visible.size()) - 1, focus + 1);
                        ensureVisible(); break;
                    case SDLK_UP:
                        if (!visible.empty()) focus = std::max(0, focus - kColumns);
                        ensureVisible(); break;
                    case SDLK_DOWN:
                        if (!visible.empty()) focus = std::min(static_cast<int>(visible.size()) - 1, focus + kColumns);
                        ensureVisible(); break;
                    case SDLK_a:
                    case SDLK_RETURN: launch(); break;
                    case SDLK_x: toggleFavorite(); break;
                    case SDLK_y:
                        favoritesOnly = !favoritesOnly;
                        status = favoritesOnly ? "Filtro: solo favoritos" : "Filtro: todos";
                        rebuildVisible(); ensureVisible(); break;
                    case SDLK_o:
                        theme.favoritesFirst = !theme.favoritesFirst;
                        status = theme.favoritesFirst ? "Orden: favoritos primero" : "Orden: ABC";
                        rebuildVisible(); ensureVisible(); break;
                    case SDLK_b:
                        theme.blur = !theme.blur;
                        status = theme.blur ? "Blur activado" : "Blur desactivado";
                        background.reload(); break;
                    case SDLK_t:
                        if (theme.backgroundType == "color") theme.backgroundType = "image";
                        else if (theme.backgroundType == "image") theme.backgroundType = "dynamic";
                        else theme.backgroundType = "color";
                        status = "Fondo: " + theme.backgroundType;
                        background.reload(); break;
                    case SDLK_d: downloadCovers(); break;
                    case SDLK_ESCAPE: running = false; break;
                    default: break;
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        if (!visible.empty()) focus = std::max(0, focus - 1);
                        ensureVisible(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        if (!visible.empty()) focus = std::min(static_cast<int>(visible.size()) - 1, focus + 1);
                        ensureVisible(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        if (!visible.empty()) focus = std::max(0, focus - kColumns);
                        ensureVisible(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        if (!visible.empty()) focus = std::min(static_cast<int>(visible.size()) - 1, focus + kColumns);
                        ensureVisible(); break;
                    case SDL_CONTROLLER_BUTTON_A: launch(); break;
                    case SDL_CONTROLLER_BUTTON_X: toggleFavorite(); break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        favoritesOnly = !favoritesOnly;
                        status = favoritesOnly ? "Filtro: solo favoritos" : "Filtro: todos";
                        rebuildVisible(); ensureVisible(); break;
                    default: break;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                Layout layout = makeLayout(kWindowW, kWindowH);
                int col = (event.button.x - layout.margin) / (layout.cardW + layout.gap);
                int row = (event.button.y - layout.top) / (layout.cardH + layout.gap);
                if (col >= 0 && col < kColumns) {
                    int index = (scrollRow + row) * kColumns + col;
                    if (index >= 0 && index < static_cast<int>(visible.size())) {
                        if (index == focus) launch();
                        else { focus = index; ensureVisible(); }
                    }
                }
            }
            if (event.type == SDL_FINGERDOWN) {
                Layout layout = makeLayout(kWindowW, kWindowH);
                int px = static_cast<int>(event.tfinger.x * kWindowW);
                int py = static_cast<int>(event.tfinger.y * kWindowH);
                int col = (px - layout.margin) / (layout.cardW + layout.gap);
                int row = (py - layout.top) / (layout.cardH + layout.gap);
                if (col >= 0 && col < kColumns) {
                    int index = (scrollRow + row) * kColumns + col;
                    if (index >= 0 && index < static_cast<int>(visible.size())) {
                        if (index == focus) launch();
                        else { focus = index; ensureVisible(); }
                    }
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        if (!visible.empty()) background.update(focus, &games[visible[focus]], now);
        else background.update(-1, nullptr, now);

        Layout layout = makeLayout(kWindowW, kWindowH);
        background.draw(kWindowW, kWindowH);

        text.draw("RetroSNES U", 30, theme.text, 40, 22);
        text.draw(std::to_string(visible.size()) + " juegos", 13, theme.muted, 42, 60);
        std::string filter = favoritesOnly ? "FAVORITOS: SOLO" : "FAVORITOS: TODOS";
        text.draw(filter, 12, favoritesOnly ? theme.accent : theme.muted, kWindowW - 190, 30);
        text.draw(theme.backgroundType + (theme.blur ? " + BLUR" : ""), 11, theme.muted,
                  kWindowW - 190, 50);

        int first = scrollRow;
        int last = std::min(static_cast<int>((visible.size() + kColumns - 1) / kColumns),
                            scrollRow + layout.visibleRows);
        for (int row = first; row < last; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                int index = row * kColumns + col;
                if (index >= static_cast<int>(visible.size())) break;
                int x = layout.margin + col * (layout.cardW + layout.gap);
                int y = layout.top + (row - scrollRow) * (layout.cardH + layout.gap);
                drawCard(renderer, text, gradients, textures, icon, theme, games[visible[index]],
                         x, y, layout.cardW, layout.cardH, index == focus);
            }
        }

        if (visible.empty()) {
            text.draw("No hay juegos con este filtro", 20, theme.muted,
                      kWindowW / 2 - 150, kWindowH / 2);
        }

        fillRect(renderer, 0, kWindowH - 52, kWindowW, 52, Color{7, 12, 22, 220});
        fillRect(renderer, 0, kWindowH - 52, kWindowW, 1, Color{255, 255, 255, 26});
        text.draw("D-PAD NAVEGAR  A JUGAR  X FAV  Y FILTRO  O ORDEN  B BLUR  T FONDO  D CARÁTULAS",
                  11, theme.muted, 40, kWindowH - 32);
        text.draw(status, 11, theme.accent, kWindowW - 300, kWindowH - 32);

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
