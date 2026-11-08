// =============================================================================
// grid.cpp - Implementacion de la cuadrícula navegable
// =============================================================================
//
// Coordenadas en el espacio virtual de la TV (1280x720): Renderer::drawRect
// escala al DRC y Renderer::drawText escala las celdas de fuente, por lo
// que el layout se dibuja una sola vez en unidades TV.

#include "ui/grid.hpp"
#include "core/systems.hpp"
#include "gfx/renderer.hpp"
#include "fs/scanner.hpp" // RomInfo completo (grid accede a title/system)

#include <vpad/input.h>
#include <cstdio>
#include <cstring>

namespace retroU {

namespace {

// Nombre corto de consola para el header (el largo vive en Menu).
const char *systemShortName(EmuConsole s)
{
    switch (s) {
    case EmuConsole::Atari:   return "Atari";
    case EmuConsole::MSX:     return "MSX";
    case EmuConsole::NES:     return "NES";
    case EmuConsole::SNES:    return "SNES";
    case EmuConsole::Genesis: return "Genesis";
    case EmuConsole::GBA:     return "GBA";
    case EmuConsole::PS1:     return "PS1";
    case EmuConsole::Arcade:  return "Arcade";
    default:                  return "???";
    }
}

// Copia truncada con "..." si no cabe (maxChars incluye los puntos).
void truncate(const char *src, char *dst, size_t maxChars)
{
    if (maxChars == 0) {
        dst[0] = '\0';
        return;
    }
    const size_t len = std::strlen(src);
    if (len <= maxChars) {
        std::memcpy(dst, src, len + 1);
        return;
    }
    if (maxChars <= 3) {
        std::memcpy(dst, src, maxChars);
        dst[maxChars] = '\0';
        return;
    }
    std::memcpy(dst, src, maxChars - 3);
    std::memcpy(dst + maxChars - 3, "...", 4);
}

} // namespace

void GameGrid::setItems(std::vector<const RomInfo *> items)
{
    m_items   = std::move(items);
    m_selected = 0;
    m_firstRow = 0;
}

const RomInfo *GameGrid::selectedItem() const
{
    return (m_selected < m_items.size()) ? m_items[m_selected] : nullptr;
}

GridAction GameGrid::onInput(uint32_t trigger)
{
    if (m_items.empty()) {
        // B sigue funcionando para salir de una vista vacia.
        return (trigger & VPAD_BUTTON_B) ? GridAction::Back
                                         : GridAction::None;
    }

    const size_t count = m_items.size();

    if (trigger & VPAD_BUTTON_A) {
        return GridAction::Activate;
    }
    if (trigger & VPAD_BUTTON_B) {
        return GridAction::Back;
    }

    if (trigger & VPAD_BUTTON_LEFT) {
        const size_t col = m_selected % kCols;
        if (col == 0) {
            // Wrap al final de la misma fila.
            const size_t end = m_selected + kCols - 1;
            m_selected = (end < count) ? end : count - 1;
        } else {
            --m_selected;
        }
        return GridAction::Moved;
    }
    if (trigger & VPAD_BUTTON_RIGHT) {
        const size_t next = m_selected + 1;
        if (next < count && (next % kCols) != 0) {
            m_selected = next;
        } else {
            // Wrap al inicio de la misma fila.
            const size_t col = m_selected % kCols;
            m_selected = (m_selected >= col + 1) ? m_selected - col : 0;
        }
        return GridAction::Moved;
    }
    if (trigger & VPAD_BUTTON_UP) {
        if (m_selected >= kCols) {
            m_selected -= kCols;
        } else {
            // Wrap a la ultima fila (misma columna).
            size_t target = m_selected;
            while (target + kCols < count) {
                target += kCols;
            }
            m_selected = target;
        }
        return GridAction::Moved;
    }
    if (trigger & VPAD_BUTTON_DOWN) {
        const size_t next = m_selected + kCols;
        if (next < count) {
            m_selected = next;
        } else {
            // Wrap a la primera fila (misma columna).
            m_selected %= kCols;
        }
        return GridAction::Moved;
    }

    return GridAction::None;
}

void GameGrid::draw(Renderer &r)
{
    if (m_items.empty()) {
        return;
    }

    // --- Scroll: mantener la fila del seleccionado visible ---------------
    const int selRow = static_cast<int>(m_selected / kCols);
    if (selRow < m_firstRow) {
        m_firstRow = selRow;
    } else if (selRow >= m_firstRow + kVisibleRows) {
        m_firstRow = selRow - kVisibleRows + 1;
    }

    const RomInfo *sel = m_items[m_selected];

    // --- Header ------------------------------------------------------------
    char header[96];
    std::snprintf(header, sizeof(header), "%s (%u) - %s",
                  systemShortName(sel->system),
                  static_cast<unsigned>(m_items.size()), sel->title.c_str());
    r.drawText(2, 0, header);
    r.drawText(2, 1, "D-Pad: navegar   A: elegir   B: volver");

    // --- Celdas ------------------------------------------------------------
    for (int row = 0; row < kVisibleRows; ++row) {
        const size_t base =
            static_cast<size_t>(m_firstRow + row) * kCols;
        if (base >= m_items.size()) {
            break;
        }

        for (int col = 0; col < kCols; ++col) {
            const size_t idx = base + static_cast<size_t>(col);
            if (idx >= m_items.size()) {
                break;
            }

            const int x = kGridX + col * (kCellW + kGapX);
            const int y = kGridY + row * (kCellH + kGapY);

            // Marco de seleccion (4 px alrededor) detras del panel.
            if (idx == m_selected) {
                r.drawRect(x - 4, y - 4, kCellW + 8, kCellH + 8,
                           kColorHighlight);
            }

            // Panel de celda (placeholder hasta el backend GX2 de covers).
            r.drawRect(x, y, kCellW, kCellH, 0x2A2A38FF);

            // Titulo truncado centrado dentro de la celda.
            char label[16];
            const size_t maxChars =
                static_cast<size_t>(kCellW / kFontCellW); // ~9
            truncate(m_items[idx]->title.c_str(), label, maxChars);

            const int textPx = x + (kCellW - static_cast<int>(std::strlen(label)) *
                                                  kFontCellW) /
                                       2;
            const int textPy = y + (kCellH - kFontCellH) / 2;
            r.drawText(static_cast<uint32_t>(textPx / kFontCellW),
                       static_cast<uint32_t>(textPy / kFontCellH), label);
        }
    }
}

} // namespace retroU
