// =============================================================================
// menu.cpp - UI de RetroU: lista de consolas y vista grid de juegos
// =============================================================================
//
// Layout UI (coordenadas virtuales del renderer: la TV es 1280x720 y el
// DRC recibe rects/texto escalados; el texto usa celdas de fuente ~12x24):
//
//   Vista Consoles:
//     [0,0]   RETROU - titulo
//     lista de consolas (fila 4+) con conteo de ROMs: "SNES (12)"
//     barra de seleccion = drawRect detras de la fila resaltada
//     [pie]   A: juegos   HOME: menu del sistema
//
//   Vista Games: la dibuja GameGrid (header + 6x4 celdas + scroll).

#include "ui/menu.hpp"
#include "fs/scanner.hpp"
#include "gfx/renderer.hpp"
#include "core/launcher.hpp"

#include <vpad/input.h> // VPAD_BUTTON_*

#include <cstdio>

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

Menu::Menu(const Scanner &scanner)
    : m_scanner(scanner)
{
}

bool Menu::onInput(uint32_t pressed, uint32_t held, Launcher &launcher)
{
    // 'held' sin uso todavia: la auto-repeticion por hold se agregara
    // con conteo de frames estable en una fase posterior.
    (void)held;

    // --- Vista grid de juegos ---------------------------------------------
    if (m_view == View::Games) {
        switch (m_grid.onInput(pressed)) {
        case GridAction::Back:
            m_view            = View::Consoles;
            m_launchFeedback  = false;
            return true;

        case GridAction::Activate: {
            const RomInfo *rom = m_grid.selectedItem();
            if (rom != nullptr) {
                // Fase Libretro pendiente: el launcher resuelve el core
                // por sistema; el feedback se limpia con B o al volver.
                launcher.launch(rom->system);
                m_launchFeedback = true;
            }
            return true;
        }

        case GridAction::Moved:
            return true;

        case GridAction::None:
            break;
        }
        return false;
    }

    // --- Vista lista de consolas -------------------------------------------
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
        m_currentSystem     = static_cast<EmuConsole>(m_selected);
        m_launchFeedback    = false;
        m_emptyRomFeedback  = false;

        std::vector<const RomInfo *> items =
            m_scanner.romsFor(m_currentSystem);

        if (items.empty()) {
            // Aviso en la propia lista: no se cambia de vista.
            m_emptyRomFeedback = true;
        } else {
            m_grid.setItems(std::move(items));
            m_view = View::Games;
        }
        changed = true;
    }

    if (pressed & VPAD_BUTTON_B) {
        if (m_launchFeedback || m_emptyRomFeedback) {
            m_launchFeedback   = false;
            m_emptyRomFeedback = false;
            changed = true;
        }
    }

    return changed;
}

void Menu::draw(Renderer &r)
{
    // --- Vista grid de juegos ---------------------------------------------
    if (m_view == View::Games) {
        // La franja de feedback queda entre el header (filas 0-1) y el
        // grid (y=64): fila 2.
        if (m_launchFeedback) {
            r.drawText(2, 2,
                       "Core no disponible todavia (fase Libretro pendiente)");
        }
        m_grid.draw(r);
        return;
    }

    // --- Vista lista de consolas -------------------------------------------
    r.drawText(1, 0, "RETROU - frontend retro de Wii U");

    const uint32_t entries = static_cast<uint32_t>(EmuConsole::Count);
    for (uint32_t i = 0; i < entries; ++i) {
        // Barra de seleccion detras de la fila resaltada.
        if (i == m_selected) {
            const uint32_t barX = kBarColumn * kFontCellWidth;
            const uint32_t barY = (kListTopRow + i) * kFontCellHeight;
            r.drawRect(static_cast<int>(barX), static_cast<int>(barY),
                       static_cast<int>(kBarCellWidth * kFontCellWidth),
                       static_cast<int>(kBarCellHeight * kFontCellHeight),
                       kColorHighlight);
        }

        // Nombre + conteo de ROMs escaneadas para ese sistema.
        char line[64];
        std::snprintf(line, sizeof(line), "%s (%u)", consoleName(i),
                      static_cast<unsigned>(m_scanner.countFor(
                          static_cast<EmuConsole>(i))));
        r.drawText(kListColumn, kListTopRow + i, line);
    }

    // --- Pie / estado --------------------------------------------------------
    if (m_emptyRomFeedback) {
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                      "Sin juegos en %s - anade ROMs en retroU/roms/",
                      consoleName(m_selected));
        r.drawText(kListColumn, kListTopRow + entries + 2, msg);
    } else if (m_launchFeedback) {
        r.drawText(kListColumn, kListTopRow + entries + 2,
                   "Core no disponible todavia (fase Libretro pendiente)");
    }

    r.drawText(kListColumn, kListTopRow + entries + 3,
               "A: juegos   HOME: menu del sistema");
}

} // namespace retroU
