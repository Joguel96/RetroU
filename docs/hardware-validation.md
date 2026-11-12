# Validación en Wii U de la tanda GX2

Esta guía cubre los cambios compilados el 12 de noviembre de 2026. Las tres
WUHB están en `dist/wiiu/apps/`; Cemu y la compilación no cuentan como prueba
en hardware. Antes de cada ronda, conserva una copia de
`sd:/retrou/retrou_rpx.log` y sus versiones `.1`, `.2`, `.3` si aparece un
fallo. El arranque rota esos archivos.

## Preparación

1. Copia `RetroNESU`, `RetroSNESU` y `RetroGBAU` de `dist/wiiu/apps/` a
   `sd:/wiiu/apps/`, conservando los nombres de carpeta y WUHB.
2. Conserva tus ROMs, saves y configuraciones actuales. Prueba primero con
   una ROM de cada consola que ya haya funcionado.
3. Anota la consola, ROM, pasos exactos y última línea del log si algo falla.

## Navegación y biblioteca

1. En cada app, abre Ajustes con Plus. Comprueba los dos paneles, entra en
   Personalizar y vuelve con B paso a paso.
2. Cambia entre Cards, Lista y Carrusel. En Lista y Carrusel, mueve el filtro
   alfabético con L/R y abre el buscador con Minus. En Cards, Minus debe abrir
   el sidebar. El cambio de vista debe persistir tras reiniciar la app.
3. Prueba D-Pad y stick L, GamePad y Pro Controller. En el GamePad, toca una
   card, una fila, una letra y una opción de Ajustes. El toque debe producir
   la misma acción visible que A o las flechas.
4. En el buscador, escribe dos caracteres: B borra uno; con búsqueda vacía,
   B sale. X acepta y sale.
5. Activa y desactiva Fondo dinámico. Prueba sus esperas de 0, 1, 3, 5 y
   10 segundos dejando fija la selección; el fondo de la ROM debe aparecer
   aproximadamente tras el valor elegido. Comprueba también el Carrusel.
6. En NES, comprueba que las portadas del carrusel conservan su orientación
   vertical. En SNES, revisa que la portada central y las laterales muestran
   la imagen completa, incluidos los bordes de las cajas.
7. En Lista y Carrusel, marca una ROM con X: debe aparecer su estrella.
   Con L/R, recorre «All», la estrella de Favoritos, «#» y A–Z; la estrella
   también debe responder al touch. Comprueba que Favoritos muestra solo las
   ROMs marcadas y que «All» restaura la lista completa.
8. En GBA Cards, recorre hasta P y el juego 144; continúa más allá. Si se
   cierra, conserva `retrou_rpx.log` antes de volver a abrir la app. Compara
   `gba_cards_focus` (índice, memoria libre y título), `gba_cover_load`/
   `gba_cover_ready`, `gba_cards_rendered` y `main_exit_clean`.

## Controles

1. En Ajustes → Controles, usa «Cambiar orden de mandos». Pulsa A en el
   dispositivo elegido para Mando 1 y, en NES/SNES, para Mando 2. GBA debe
   pedir solo Mando 1. Comprueba el orden dentro del juego.
2. En Mapear botones, cambia una acción y reinicia la app. Confirma que el
   cambio persiste. Comprueba un Pro/Classic si está disponible.
3. En el editor, pulsa Plus. Guarda un perfil con Y, cambia una asignación,
   carga el perfil con A y confirma que recupera el valor guardado. Repite
   para GamePad y Pro/Classic.
4. Entra a una ROM, abre el menú con Minus+Plus y elige Controles. Cambia una
   acción, cierra el juego y vuelve a entrar: el cambio debe persistir solo
   para esa ROM. Otra ROM debe conservar el mapa general.
5. Tras una partida, el log debe incluir `input summary` y los eventos
   `rom_controls_*`, `global_controls_*` o `control_profile_*` que correspondan.
6. En Ajustes → Controles cambia por separado las combinaciones «Menú GamePad»,
   «Menú Pro / Classic» y «Menú Wii Remote / Nunchuk». Reinicia otra app y
   confirma que el ajuste global persiste. Prueba cada combinación durante una
   partida; Minus+Plus debe seguir abriendo el menú desde el mismo mando.
   Pulsar Minus en un mando y Plus en otro no debe abrirlo. Comprueba en el log
   `menu_shortcut_saved` y `core_menu_shortcut_opened` con su origen.

## Guardados y Assets

1. Durante una partida, guarda en los cuatro slots y comprueba la miniatura
   de cada uno; carga un slot anterior y confirma que restaura el juego.
2. En un juego que use SRAM, crea un guardado interno, sal del juego y de la
   app; vuelve a abrir y confirma que el guardado interno sigue presente.
   Comprueba `sd:/retrou/saves/<consola>/save/` y, si existe, el `.bak`.
3. Con auto-descarga apagada, reinicia y comprueba que la metadata disponible
   se actualiza. En Assets, descarga primero portadas y luego backdrops;
   verifica contador/barra y que no se mezclan los dos tipos.
4. Prueba «Solo faltantes» y «Descargar todo» con conexión. Comprueba en la SD
   `sd:/retrou/thumbnails/<consola>/` y
   `sd:/retrou/backdrops/<consola>/`. Si una descarga falla, localiza la línea
   `GX2 asset: fallo` en `retrou_rpx.log` para identificar el recurso y el
   error HTTP/red; `GX2 asset: indice` debe indicar entradas mayores que cero.
   Interrumpe una tanda y reanúdala: el nuevo total debe contar las imágenes
   que aún faltan, sin metadata ni archivos presentes. «Procesadas» incluye
   intentos fallidos; comprueba el contador «Fallos» y `pendientes=` en el log.
   Pulsa Descargar inmediatamente después de abrir la app: debe indicar
   «En cola» si sigue actualizando metadata y luego iniciar las imágenes.
5. Descarga portadas con la app abierta y vuelve a Cards, Lista y Carrusel sin
   reiniciarla. Cada vista debe mostrar las nuevas imágenes cuando termine el
   trabajo. Confirma `covers_refreshed` en `retrou_rpx.log`.

## N64

`RetroN64U.wuhb` sigue pendiente de prueba en hardware (N64-1). El port de
Our64 a la interfaz RetroU (N64-2) está fuera de la autorización de esta
tanda y no se tocó.

## Trazabilidad y reversión

| Commit | Cambio | Log de compilación |
|---|---|---|
| `9d05cdf` | Mapeo, navegación, guardados, Assets y staging | `native/build/logs/build-20261112-005205.log` |
| `f1dfa27` | Vistas y Ajustes en dos paneles | `native/build/logs/build-20261112-075850.log` |
| `5ff602b` | Controles por ROM, perfiles y orden | `native/build/logs/build-20261112-081407.log` |
| `d2d3ad7` | Espera configurable del fondo | `native/build/logs/build-20261112-084054.log` |
| `6cd6789` | Creación de carpetas de configuración en SD limpia | `native/build/logs/build-20261112-084956.log` |
| `98c3d79` | Diagnóstico y reemplazo de descargas | `native/build/logs/build-20261112-085535.log` |
| `62317fc` | Lectura segura de máscaras de controles | `native/build/logs/build-20261112-090940.log` |
| `7fdf860` | Reinicio del temporizador al cambiar de ROM | `native/build/logs/build-20261112-091305.log` |
| `a2d87ae` | Carrusel NES/SNES, Favoritos en Lista/Carrusel, recarga de portadas y atajos del menú por mando | `native/build/logs/build-20261112-130827.log` |
| `9bfce0e` | Favoritos junto a All, progreso de imágenes pendientes y diagnóstico de cierre GBA | `native/build/logs/build-20261112-142320.log` |
| `2838306` | Recorte de títulos UTF-8 y traza del título enfocado en GBA P | `native/build/logs/build-20261112-143059.log` |
| `67f44e5` | Rutas seguras GBA, índice de assets y cola de descargas manuales | `native/build/logs/build-20261112-150558.log` |

Los logs y WUHB están ignorados por Git: conserva una copia antes de limpiar
`native/build/` o `dist/`. Para retirar un cambio, revierte su commit con Git,
recompila con `native/build-retrou-gx2.sh` y copia de nuevo la WUHB a la SD.
La reversión del código no borra configuraciones ni guardados de la SD.

Marca cada ID en `PENDIENTES.md` con ✅ y fecha solo después de confirmar su
resultado en una Wii U. Si falla, conserva el log y deja el ID en 🔧.
