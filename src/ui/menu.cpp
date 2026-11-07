// =============================================================================
// menu.cpp - Menu principal (dibujo y navegacion)
// =============================================================================
//
// Layout UI (coordenadas virtuales del renderer: la TV es 1280x720 y el
// DRC 854x480; el texto de OSScreen usa CELDAS de caracter (~12x24 px):
//
//   [0,0]      RETROU - titulo
//   lista de consolas empezando en fila 4, columna 4
//   barra de seleccion = drawRect detras de la fila resaltada
//   [pie]      A: cargar   B: limpiar   HOME: salir

#include "ui/menu.hpp"
#include "gfx/renderer.hpp"
#include "core/launcher.hpp"

#include <vpad/input.h> // VPAD_BUTTON_*

namespace retroU {

namespace {

constexpr uint32_t kFontCellWidth  = 12; // aprox (fuente interna OSScreen)
constexpr uint32_t kFontCellHeight = 24;

constexpr uint32_t kListColumn = 4;  // celdas de caracter
constexpr uint32_t kListTopRow = 4;

// Dimensiones de la barra de seleccion en celdas de la fuente: se convierten
// a pixeles virtuales para drawRect (sistema logico 1280x720 de la TV).
constexpr uint32_t kBarColumn = kListColumn - 1;
constexpr uint32_t kBarCellWidth  = 90;
constexpr uint32_t kBarCellHeight = 2;

} // namespace

const char *Menu::consoleName(uint32_t index)
{
    // Nombres canonizados de los sistemas objetivo (mismo orden que EmuConsole)
    static const char *kNames[] = {
        "Atari",
        "MSX",
        "NES",
        "SNES",
        "Mega Drive / Genesis",
        "Game Boy Advance",
        "PlayStation 1",
        "Arcade",
    };
    return (index < static_cast<uint32_t>(EmuConsole::Count))
        ? kNames[index]
        : "???";
}

bool Menu::onInput(uint32_t pressed, uint32_t held, Launcher &launcher)
{
    // 'held' sin uso todavia: la auto-repeticion por hold se agregara
    // con conteo de frames estable en una fase posterior.
    (void)held;

    const uint32_t entries = static_cast<uint32_t>(EmuConsole::Count);
    bool changed = false;

    if (pressed & VPAD_BUTTON_UP) {
        m_selected = (m_selected + entries - 1) % entries; // wrap hacia arriba
        changed = true;
    }
    if (pressed & VPAD_BUTTON_DOWN) {
        m_selected = (m_selected + 1) % entries;           // wrap hacia abajo
        changed = true;
    }

    if (pressed & VPAD_BUTTON_A) {
        // Confirmacion: el launcher resolvera el core Libretro asociado.
        // El feedbackCaption se limpia con B o con la proxima navegacion.
        launcher.launch(static_cast<EmuConsole>(m_selected));
        m_launchFeedback = true;
        changed = true;
    }

    if (pressed & VPAD_BUTTON_B) {
        if (m_launchFeedback) {
            m_launchFeedback = false;
            changed = true;
        }
    }

    return changed;
}

void Menu::draw(Renderer &r)
{
    // --- Titulo ------------------------------------------------------------
    r.drawText(1, 0, "RETROU - frontend retro de Wii U");

    // --- Barra de seleccion -------------------------------------------------
    const uint32_t barX = kBarColumn * kFontCellWidth;
    const uint32_t barY = (kListTopRow + m_selected) * kFontCellHeight;
    r.drawRect(
        static_cast<int>(barX),
        static_cast<int>(barY),
        static_cast<int>(kBarCellWidth * kFontCellWidth),
        static_cast<int>(kBarCellHeight * kFontCellHeight),
        kColorHighlight);

    // --- Lista de consolas ---------------------------------------------------
    const uint32_t entries = static_cast<uint32_t>(EmuConsole::Count);
    for (uint32_t i = 0; i < entries; ++i) {
        r.drawText(kListColumn, kListTopRow + i, consoleName(i));
    }

    // --- Pie / estado --------------------------------------------------------
    if (m_launchFeedback) {
        r.drawText(kListColumn,
                   kListTopRow + entries + 2,
                   "Core no disponible todavia (fase Libretro pendiente)");
    }
    r.drawText(kListColumn,
               kListTopRow + entries + 3,
               "A: cargar   B: limpiar   HOME: salir");
}

} // namespace retroU
