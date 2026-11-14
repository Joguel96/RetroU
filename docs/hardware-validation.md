# Validación en Wii U de la tanda GX2

Esta guía cubre las rondas del 12 y 13 de noviembre de 2026. Las tres
WUHB están en `dist/wiiu/apps/`; Cemu y la compilación no cuentan como prueba
en hardware. Antes de cada ronda, conserva una copia de
`sd:/retrou/retrou_rpx.log` y sus versiones `.1`, `.2`, `.3` si aparece un
fallo. El arranque rota esos archivos.

El usuario confirmó en Wii U la navegación, controles, guardados y Assets de
NES/SNES/GBA, y que RetroN64U funciona (N64-1). En la ronda del 2026-11-13
confirmó UI-2, CTRL-8 y AS-6. También confirmó que el Pro llega al core de
GBA aunque la ROM tenga mapa propio (CTRL-9). Reportó ruido en el audio de
GBA (AUD-1); la conversión de frecuencia requiere otra prueba. El usuario
confirmó UI-10, AUD-3 y APP-1 en Wii U física el 2026-11-13. N64-2 sigue
pendiente; los resultados N64 de esta ronda proceden de Cemu.

## Preparación

1. Copia `RetroNESU`, `RetroSNESU` y `RetroGBAU` de `dist/wiiu/apps/` a
   `sd:/wiiu/apps/`, conservando los nombres de carpeta y WUHB.
2. Conserva tus ROMs, saves y configuraciones actuales. Prueba primero con
   una ROM de cada consola que ya haya funcionado.
3. Anota la consola, ROM, pasos exactos y última línea del log si algo falla.

## Navegación y biblioteca

1. En cada app, abre Ajustes con Plus. Comprueba que las secciones ocupan
   la columna izquierda y las opciones la derecha, según
   `ejemplos/Disposicion de menu.png`. Entra en Personalizar, prueba touch en
   ambos paneles y vuelve con B paso a paso.
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

1. En Ajustes → Controles, pulsa A sobre cada puerto para conectarlo y prueba
   también «Cambiar orden de mandos». Pulsa A en el
   dispositivo elegido para Mando 1 y, en NES/SNES, para Mando 2. GBA debe
   pedir solo Mando 1. Comprueba el orden dentro del juego. El acceso directo
   por puerto está en el commit `f0e5c00`; build y staging:
   `native/build/logs/build-20261113-195251.log`.
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
7. Asigna Pro o Classic a Mando 1 (y Mando 2 en NES/SNES), desconéctalo durante
   la biblioteca y luego durante una partida, y reconéctalo. Comprueba que
   conserva puerto, mapa y navegación de la UI, incluso si cambia de canal.
   En el log busca `controller_disconnected`, `controller_rebound` y, si cambió
   de canal mientras aún figuraba el anterior, `controller_restored`.
8. En GBA, elige el Pro como Mando 1 y abre una ROM que ya tenga archivo en
   `rom-maps/`. Prueba D-Pad, A, B y Start con el Pro. El log debe mostrar
   `rom_controls_loaded`, luego `core_ports_global p1=...` con el índice del
   Pro y un `input summary` con `wpad=[<canal>,-1]`.

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
6. Inicia una descarga larga y sal hacia el menú de Wii U. Comprueba que la
   consola responde, que el log termina con `asset_shutdown_begin`,
   `asset_shutdown_complete` y `main_exit_clean` (si la transferencia ya
   había comenzado, también `GX2 asset: cancelado`), y que
   la siguiente apertura puede reanudar «Solo faltantes».

## Audio GBA

Abre en Wii U la misma ROM que producía ruido y escucha música y efectos al
menos durante un minuto. Cambia el volumen del core y reanuda tras abrir el
menú de partida. Confirma que el log muestra
`Audio reusing external device: 65536 Hz -> 32000 Hz` (o la frecuencia real
que SDL haya abierto) y que no hay ruido, aceleración ni cortes. Repite una
partida breve de NES o SNES para detectar una regresión en la ruta directa.

## N64: ronda del port RetroU (N64-2)

La WUHB base de Our64 funcionó en Wii U (N64-1). La WUHB nueva del port
todavía **no** tiene una ronda completa de pruebas en hardware. Cemu confirmó
el arranque y las vistas de biblioteca con 82 ROMs; generó
`sd:/retrou/n64/retrou_n64.log` con líneas `GAME:` e `INPUT:`.
La apariencia de Ajustes difiere de NES/SNES/GBA y queda como pendiente
visual separado.

| Evidencia antes de sustituir la WUHB | Valor |
|---|---|
| Our64, código de la compilación | `d3c56db` (buscador y trazas de Assets; el commit se creó después de compilar el mismo árbol) |
| SHA-256 de la WUHB nueva | `a9321c308f594a0f9e2f3762bdffd58f691fbbec8755a9978f4b691317536cea` |
| SHA-256 de la WUHB previa | `9d5a7dabeb0b079f3e14192bf84437cfef5450ca4889e37bba43c37ea9b3190b` |
| Build | `C:/proyectos/Our64/player/build-release/logs/build-20261114-024624.log` |
| Staging nuevo / reversión | `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb` / `RetroN64U.previous.wuhb` |
| Log en SD | `sd:/retrou/n64/retrou_n64.log` |

La nueva WUHB guarda hitos `ASSETS:` antes del escaneo, al crear el worker,
al iniciar la red y antes/después de cada juego. La reproducción del cierre
con esta WUHB sigue pendiente; la última traza permitirá localizar el fallo.

Conserva una copia de la WUHB previa y del log N64 antes de cada prueba.
Copia solo la nueva `RetroN64U.wuhb` a
`sd:/wiiu/apps/RetroN64U/RetroN64U.wuhb`. Registra modelo de mando,
formato y nombre de ROM, número de jugadores, resultado y última línea del
log en cada fallo. Estas casillas siguen abiertas hasta probarse en Wii U:

| Prueba en Wii U | Resultado esperado | Estado |
|---|---|---|
| Arranque desde el menú, salida a biblioteca y cierre con HOME | Nombre RetroU N64, biblioteca visible; HOME devuelve el control al sistema | ⬜ |
| Cargar una ROM `.z64`, otra `.n64` y otra `.v64` | Las tres arrancan; anotar título y rendimiento por separado | ⬜ |
| Recorrer Cards, Lista y Carrusel, incluidos los extremos del catálogo | Selección, desplazamiento, filtro A–Z y proporciones de portadas correctos | Cemu: «ok»; falta Wii U |
| Buscar, marcar favorito, filtrar por estrella y volver a All | El filtro y las estrellas reflejan el cambio sin reiniciar | Cemu: filtrado/favoritos «ok»; B/X del buscador fallan (N64-4); falta Wii U |
| Descargar portada y backdrop con la app abierta | Progreso y arte se actualizan sin reiniciar; el log registra `ASSETS:` | Cemu: la WUHB provisional de Our64 se cierra. El log `sd:/retrou/n64/retrou_n64.log` llega a `ASSETS: existing network result=00d00000 connected=1` y no registra el siguiente hito. El usuario pidió sustituir por el descargador de RetroU y las URL de Libretro; N64-5 sigue abierto. |
| Navegar y jugar con GamePad, Wii U Pro, Classic, Wii Remote y Nunchuk | Cada mando disponible responde según su perfil y asignación; anotar los no disponibles | ⬜ |
| Asignar y jugar con 1, 2, 3 y 4 jugadores | Cada puerto recibe solo su mando y conserva su orden | ⬜ |
| Desconectar y reconectar Pro/Classic durante biblioteca y partida | El mando recupera su puerto y el juego responde; revisar `INPUT:` | ⬜ |
| Configurar y usar el atajo de menú por familia de mando | Abre y cierra la pausa con el mando asignado; el juego se detiene | ⬜ No ejecutada; el usuario la dejó para hardware |
| Crear y reabrir guardado SRAM del juego | Persiste tras salir y volver a entrar; queda bajo `sd:/retrou/n64/saves/` | ⬜ |
| Guardar/cargar tres slots manuales y probar la cuarta tarjeta Undo | Los tres slots y la reversión de carga, sobrescritura o reset funcionan | ⬜ |
| Escuchar música y efectos mediante AX, pausar y reanudar | Audio estable, sin ruido ni pérdida tras la pausa | ⬜ |
| Volver del juego a la biblioteca, cerrar al menú y usar HOME | No se bloquea ni pierde guardados; revisar `PAUSE:` y `GAME:` | ⬜ |
| Salir mientras se descargan Assets | Termina o cancela el trabajador sin congelar la Wii U; revisar `ASSETS:` | ⬜ |
| Usar datos anteriores de Our64 sin moverlos | Si `sd:/retrou/roms/n64` está vacía, detecta las ROMs antiguas de solo lectura; lee controles/guardados previos cuando faltan los nuevos | ⬜ |
| Revisar Ajustes frente a NES/SNES/GBA | Igualar secciones, contenido y flujo con RetroU, manteniendo la ejecución N64 de Our64 | Cemu: discrepancias visuales y funcionales confirmadas (N64-3/N64-7) |

Los estados son **tres slots manuales más Undo**, no cuatro slots manuales.
Para revertir, cierra la app, restaura `RetroN64U.previous.wuhb` en
`dist/wiiu/apps/RetroN64U/`, verifica el SHA-256 previo de la tabla y
vuelve a copiarla a la SD. Conserva `sd:/retrou/n64/` y el log para
diagnosticar; no presentes una prueba en Cemu como verificación de Wii U.

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
| `a227b86` | Cancelación al salir, restauración de mandos y nueva distribución de Ajustes | `native/build/logs/build-20261112-234215.log` |
| `826b787` | GBA conserva Mando 1 global al abrir una ROM con mapa propio | `native/build/logs/build-20261113-002521.log` |
| `4d73820` | Conversión de audio del core GBA a la frecuencia de SDL | `native/build/logs/build-20261113-003709.log` |

Los logs y WUHB están ignorados por Git: conserva una copia antes de limpiar
`native/build/` o `dist/`. Para retirar un cambio, revierte su commit con Git,
recompila con `native/build-retrou-gx2.sh` y copia de nuevo la WUHB a la SD.
La reversión del código no borra configuraciones ni guardados de la SD.

Marca cada ID en `PENDIENTES.md` con ✅ y fecha solo después de confirmar su
resultado en una Wii U. Si falla, conserva el log y deja el ID en 🔧.
