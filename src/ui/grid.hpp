#pragma once

// =============================================================================
// grid.hpp - Cuadrícula navegable de juegos (caratulas) para la UI
// =============================================================================
//
// Grid de 6 columnas x 4 filas visibles (24 celdas) en el espacio virtual
// de la TV (1280x720); el Renderer escala rects y texto al GamePad (854x480).
//
// Cada celda es un placeholder con el titulo truncado mientras no exista el
// backend GX2 que muestre las GX2Texture de TextureLoader (fase 2). La
// navegacion es D-Pad en 4 direcciones con wrap; A confirma, B vuelve.
//
// Layout (coordenadas virtuales TV):
//   fila 0-1:  header  "SNES (12) - Super Mario World"
//              hint    "D-Pad: navegar   A: elegir   B: volver"
//   grid y=64: 4 filas de (144 alto + 16 gap); 6 cols de (112 + 24)
//              total 792x640 -> centrado en x=244, termina en y=704

#include <cstddef>
#include <cstdint>
#include <vector>

namespace retroU {

struct RomInfo;

// Resultado del input: None = sin cambio (no repintar).
enum class GridAction : uint8_t {
    None,
    Moved,    // cambio de seleccionado
    Activate, // A sobre un item
    Back,     // B (volver a la lista de consolas)
};

class GameGrid {
public:
    // Items: punteros al vector fuente (Scanner::romsFor). Deben seguir
    // vivos mientras el grid los use (no re-escanear sin re-setear).
    void setItems(std::vector<const RomInfo *> items);

    bool empty() const { return m_items.empty(); }
    const RomInfo *selectedItem() const;

    // Procesa el trigger de VPAD; devuelve la accion ejecutada.
    GridAction onInput(uint32_t trigger);

    // Dibuja header + celdas + pie sobre el renderer (no-op si vacio:
    // el llamador dibuja su propio estado de "sin juegos").
    void draw(class Renderer &r);

private:
    static constexpr int kCols        = 6;
    static constexpr int kVisibleRows = 4;
    static constexpr int kCellW       = 112;
    static constexpr int kCellH       = 144;
    static constexpr int kGapX        = 24;
    static constexpr int kGapY        = 16;
    static constexpr int kGridX       = 244; // (1280 - 6*112 - 5*24) / 2
    static constexpr int kGridY       = 64;
    static constexpr int kFontCellW   = 12;  // aprox celda de fuente OSScreen
    static constexpr int kFontCellH   = 24;

    std::vector<const RomInfo *> m_items;
    size_t m_selected = 0;
    int    m_firstRow = 0; // primera fila visible (scroll vertical)
};

} // namespace retroU
