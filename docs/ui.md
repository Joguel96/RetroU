# Interfaz RetroU (React)

La interfaz vive en `web/` y se extrajo de `Joguel96/retroui`. Es la
especificación visual y de interacción de RetroU: cristal, carátulas, consolas,
ajustes, favoritos y modales.

## Stack

- Vite 8
- React 19 + TypeScript
- Tailwind CSS 4
- `lucide-react` para iconografía

Se eliminaron del proyecto original el backend Express, Gemini y las
dependencias de servidor. Los datos de demostración están en
`web/src/data/mockData.ts`.

## Navegación con mando

El teclado y el ratón están desactivados por defecto:

- Se bloquean los eventos `keydown`/`keyup` reales (`event.isTrusted`).
- El ratón queda desactivado con `pointer-events: none` en el contenedor.
- El foco se dibuja con la clase `.gamepad-focused` y `scrollIntoView`.

Implementación: `web/src/gamepad/useGamepadNavigation.ts`.

| Mando Wii U | Acción |
|---|---|
| D-Pad / stick izquierdo | mover el foco espacialmente |
| A | activar el elemento enfocado |
| B | volver / cerrar modal / ir a Inicio |
| X | detalle del juego enfocado |
| Y | favorito del juego enfocado |
| L / R | vista anterior / siguiente (Inicio, Consolas, Ajustes) |
| Start (+) | Ajustes |
| Select (-) | volver / Inicio |

Los sliders (`input[type=range]`) se ajustan con izquierda/derecha cuando están
enfocados. Los `select` cambian con arriba/abajo. El buscador de texto queda
fuera del foco porque en Wii U no hay teclado.

### Fallback de desarrollo

Para revisar la UI en un navegador de escritorio con teclado y ratón:

```text
http://localhost:3000/?keyboard=1
```

Sin ese parámetro la app es solo gamepad, igual que el objetivo final.

## Ejecutar

```sh
cd web
npm install
npm run dev
```

Build de producción:

```sh
npm run build
```

## Límite de arquitectura

React **no se puede ejecutar de forma nativa en Wii U**: WUT no incluye un
WebView. Este proyecto es la especificación ejecutable de la interfaz y el banco
de pruebas de navegación con mando. La app nativa Wii U (C++ + WUT + SDL2/GX2)
debe portar estas vistas y el mismo mapa de botones. La lógica de lanzamiento de
`.rpx` y descarga de carátulas está documentada en
[`retroarch-reference.md`](retroarch-reference.md).
