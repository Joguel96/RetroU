# AGENTS.md — RetroU

Este archivo es la fuente de contexto para cualquier agente que trabaje en
RetroU. Léelo antes de proponer o tocar código.

## 1. Regla principal de cambios

**No modificar código, configuración, assets, documentación funcional ni
scripts sin autorización explícita del usuario.**

Antes de cualquier cambio hay que:

1. Explicar en 2–5 líneas:
   - qué archivo(s) se van a tocar;
   - qué comportamiento cambia;
   - qué riesgo tiene.
2. Esperar la autorización del usuario.
3. Solo entonces editar.
4. Compilar/verificar lo que se haya tocado.
5. Reportar qué se cambió y qué no se pudo probar.

Una petición del tipo “hazlo” autoriza el alcance descrito, no cambios
colaterales. No hacer refactors, limpiezas, renombrados ni “mejoras” no
pedidas. Si aparece un problema adicional, describirlo y pedir permiso antes
de arreglarlo.

La investigación de solo lectura (leer archivos, logs, documentación, ejecutar
builds de diagnóstico sin editar) no requiere autorización, salvo que el
usuario indique lo contrario.

## 2. Objetivo del proyecto

RetroU es un frontend propio para Nintendo Wii U (Aroma) para ejecutar juegos
retro, gestionar catálogo, carátulas, controles, guardados y ajustes.

RetroArch se usa **como referencia técnica**, no como base de código. No
reintroducir RetroArch en el árbol ni convertirlo en un fork.

Requisitos históricos del usuario:

- Interfaz con identidad RetroU, no la UI de RetroArch.
- Navegación solo con mando; teclado/ratón solo como fallback de desarrollo.
- Todo el ecosistema de datos bajo `retrou`, no `retroarch`.
- Un solo core cargado a la vez.
- Se prefieren soluciones simples y estables antes que arquitecturas
  experimentales.
- La app debe sentirse como RetroU, aunque por debajo cambie de RPX como
  RetroArch.

## 3. Arquitectura actual (`main`)

Modelo elegido: **una WUHB independiente por consola**, como las apps virtuales
de Switch. No hay launcher ni carpeta de cores en la SD.

NES, SNES y GBA contienen:

```text
RetroU frontend + un core Libretro enlazado estáticamente
```

N64 se compila desde este proyecto con el motor Not64 + GLideN64 extraído
de Our64 y audio AX, sin Libretro. La versión anterior con UI de Our64 se
conserva como referencia, pero ya no se compila.

Apps:

| Target | Core | Sistema |
|---|---|---|
| `retrou_nes_gx2` | fceumm | NES |
| `retrou_snes_gx2` | snes9x | SNES |
| `retrou_gba_gx2` | mgba | GBA |
| `retrou_n64_gx2` | Not64 + GLideN64 + AX | N64 |

Salidas:

```text
native/build/retrou_nes_gx2.wuhb
native/build/retrou_snes_gx2.wuhb
native/build/retrou_gba_gx2.wuhb
native/build/retrou_n64_gx2.wuhb
```

Distribución:

```text
dist/wiiu/apps/RetroNESU/RetroNESU.wuhb
dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
dist/wiiu/apps/RetroN64U/RetroN64U.wuhb
```

La salida anterior con UI de Our64 ya no se compila ni se distribuye.

Desde la publicación inicial se incluyen también RPX por consola para
Homebrew Launcher/Tiramisu dentro del ZIP del release. No son cores para
otro frontend; Tiramisu queda pendiente de validación física (REL-2).
No se distribuye `RetroU.wuhb` launcher.

## 4. Datos y configuración

```text
sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/

sd:/retrou/config/theme.conf              tema compartido

sd:/retrou/config/nes/
  nes.conf
  control-map.conf
  video.conf
  audio.conf
  view.conf
  profiles/
  rom-maps/
sd:/retrou/config/snes/
  snes.conf
  control-map.conf
  video.conf
  audio.conf
  view.conf
sd:/retrou/config/gba/
  gba.conf
  control-map.conf
  video.conf
  audio.conf
  view.conf

sd:/retrou/config/n64/
  control-map.conf
  n64-ports.conf

sd:/retrou/saves/<consola>/save/          SRAM de juegos
sd:/retrou/snap/<consola>/                slots de guardado de la app
```

Las carpetas de ROMs se aceptan en minúscula (`nes`, `snes`, `gba`) y también
en mayúscula por compatibilidad. La ruta canónica es minúscula.

`theme.conf` es común. `<consola>.conf`,
`control-map.conf`, `video.conf`, `audio.conf` y `view.conf` son independientes por app.

`audio.conf` usa `v1 <frecuencia> <búfer>`: frecuencia 0 = automática
(GBA 48 kHz, NES/SNES 32 kHz), 1 = 32 kHz, 2 = 48 kHz; búfer 0/1/2 =
512/1024/2048 muestras. Los cambios en Sonido se aplican al reiniciar la app
porque el dispositivo SDL no se cierra durante la sesión (AUD-3).

`video.conf` admite un cuarto valor para la salida: 0 = TV + GamePad,
1 = solo TV, 2 = solo GamePad. Los archivos anteriores de tres valores usan 0.

`control-map.conf` usa el formato `v2` estilo Our64 (acción del core → una o
varias máscaras de botones físicos): secciones `[gamepad]` y `[pro]`, líneas
`p1`/`p2` y una línea por acción. Las acciones sin asignación se escriben como
`none`, para que `X Unmap` sobreviva al reinicio. Los conf en formato `v1` o
viejo (una línea de 13 ints) se migran al cargar y se reescriben al primer
cambio.

Una máscara inválida en un mapa `v2` produce el valor predeterminado y
`control_map_parse_failed` en `retrou_rpx.log`, sin cerrar la app.

`profiles/` contiene tres mapas por GamePad o Pro/Classic y consola.
`rom-maps/` guarda los cambios de cada ROM por hash FNV-1a de su ruta; el mapa
general se restaura al cerrar el juego.

Ya no se usa `sd:/retrou/cores`, HBL, Mocha ni `launch/last.txt`.

## 5. Cores

Los cores no se bajan del buildbot como `.rpx`; esos `.rpx` son
RetroArch+core completos.

Nuestros cores se compilan desde fuente como `.a`:

```text
.cores/fceumm/fceumm_libretro_wiiu.a
.cores/snes9x/libretro/snes9x_libretro_wiiu.a
.cores/mgba/build/mgba_libretro.a
```

Script reproducible:

```text
native/cores/build-static-cores.sh
```

`.cores/` está ignorado por git. El script:

- clona fceumm, snes9x y mgba;
- compila fceumm y snes9x con `make platform=wiiu`;
- configura mgba con CMake y compila `mgba_libretro`;
- aplica el workaround `USE_FILE32API` en el minizip de mgba.

CMake añade fuentes de `libretro-common` necesarias para los cores estáticos:

- `compat_strl`
- `file_path`, `file_path_io`
- `file_stream`, `file_stream_transforms`
- `trans_stream`, `trans_stream_pipe`, `trans_stream_deflate`
- `memory_stream`
- `encoding_deflate`
- `stdstring`
- `vfs_implementation`

No añadir dependencias nuevas sin autorización.

## 6. RPL — rama experimental

El intento de cargar `.rpl` en proceso está en:

```text
experiment/rpl-loader
```

Estado:

- el loader descomprime secciones y aplica relocaciones;
- llegó a `OSCodegenCopy`, que falla;
- el userland de Wii U no permite ejecutar memoria normal;
- `OSDynLoad_Acquire` de Aroma no carga `.rpl` sueltos de la SD;
- el RPL no es la ruta principal y no debe meterse en `main`.

No extender el RPL sin petición explícita. Si se retoma, hacerlo en su rama.

## 7. UI y rendimiento

- UI de cristal propia, en español, guiada por `Joguel96/retroui`.
- Navegación con D-Pad/stick, A, B, Plus.
- No hay barra lateral de consolas; `Plus` abre Ajustes en dos paneles.
- La biblioteca ofrece cards, lista y carrusel; lista/carrusel tienen filtro
  alfabético con L/R, Favoritos junto a «All» y estrellas en sus ROMs. NES usa
  portadas verticales en carrusel y SNES presenta la imagen completa.
  `Y` cambia entre las tres vistas desde la biblioteca.
  En Lista, izquierda/derecha del D-pad o stick L desplaza cinco juegos
  conservando la fila seleccionada, con límites al inicio y final; arriba/abajo
  mueve un juego (NAV-11, confirmado por el usuario el 2026-11-23).
  La vista, el fondo dinámico y su espera configurable
  (0/1/3/5/10 s; 3 s por defecto) se guardan en `view.conf`.
- `Minus` abre el sidebar de filtros solo en cards; en lista/carrusel abre
  el buscador. Las cards permiten entre 3 y 6 columnas.
- HOME abre el menú RetroU durante una ROM y el menú HOME de Wii U en biblioteca
  o Ajustes. «Cerrar aplicación» desde el menú rápido sale al sistema.
- «Controles» en ese menú abre el editor de la ROM. En Ajustes → Controles,
  «Cambiar orden de mandos» asigna los puertos al pulsar A en cada dispositivo.
- El menú de juego pausa el core.
- La UI se dibuja en el bucle principal cuando la app está en primer plano.
- Las fuentes Lato van embebidas en los WUHB con `objcopy`.

## 8. Logs

```text
sd:/retrou/retrou_rpx.log      app estática (init/run; .1/.2/.3 anteriores)
sd:/retrou/retrou_rpl.log      RPL (solo rama experimental)
```

`retrou_rpx.log` registra también el índice de nombres de Libretro y fallos de
descarga HTTP/red por recurso (`GX2 asset`); el reemplazo forzado conserva el
archivo existente si falla. GBA registra selección, memoria libre y render de
Cards para diagnosticar el cierre cercano al juego 144. Las descargas manuales
solicitadas mientras se actualiza metadata se ponen en cola.

Ya no se usa `launcher.log` ni `launch/last.txt`.

## 9. Directorios y datos

```text
sd:/wiiu/apps/RetroNESU/RetroNESU.wuhb
sd:/wiiu/apps/RetroSNESU/RetroSNESU.wuhb
sd:/wiiu/apps/RetroGBAU/RetroGBAU.wuhb
sd:/wiiu/apps/RetroN64U/RetroN64U.wuhb

sd:/retrou/roms/nes/
sd:/retrou/roms/snes/
sd:/retrou/roms/gba/
sd:/retrou/roms/n64/
sd:/retrou/n64/retrou_n64.log

sd:/retrou/config/theme.conf
sd:/retrou/config/<consola>/<consola>.conf
sd:/retrou/config/<consola>/control-map.conf
sd:/retrou/config/<consola>/video.conf
sd:/retrou/config/<consola>/view.conf
sd:/retrou/config/<consola>/profiles/
sd:/retrou/config/<consola>/rom-maps/

sd:/retrou/saves/<consola>/save/
sd:/retrou/snap/<consola>/
```

No usar rutas `retroarch/` ni `retrou/cores/`.

## 10. Build

Toolchain:

```text
C:/devkitPro
WUT
SDL2 / SDL2_ttf
CMake wrapper: /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake
```

Build:

```sh
cd native
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

En Windows se usa MSYS2 bash para CMake/make. N64 usa
`native/build-retrou-n64.sh` y se copia a
`dist/wiiu/apps/RetroN64U/`; no entra en el build CMake por defecto.

No commitear:

- `native/build/`
- `.cores/`
- `dist/`
- logs
- artefactos `.rpx`, `.wuhb`, `.elf`, `.a`

## 11. Estado funcional y limitaciones

**La lista canónica de pendientes es `PENDIENTES.md`** (raíz del proyecto):
antes de empezar una actividad hay que revisarlo, y cada pendiente terminado
**y verificado en hardware** se marca como ✅ allí (con fecha). Este resumen
solo mantiene el estado general.

- NES/SNES/GBA: WUHB estática generada y compilada.
- N64: `retrou_n64_gx2` enlaza el motor Not64 + GLideN64 extraído de
  `C:/proyectos/Our64` dentro del frontend RetroU. El build separado
  `native/build-retrou-n64.sh` deja la WUHB oficial en
  `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`; la versión anterior con UI de
  Our64 queda solo como referencia. El usuario confirmó en Wii U una ROM
  jugable, cuatro tarjetas de mando, configuración, buscador, descarga de
  assets y carrusel con portadas. El 2026-11-21 confirmó que el menú rápido
  abre durante una ROM y guarda/carga estados con miniatura (N64-12). El usuario
  confirmó el input y HOME durante partida el 2026-11-22. HOME después de
  cerrar N64 y Cerrar aplicación siguen pendientes de verificar con la
  corrección del registro repetido de callbacks WHB (N64-2/7/11, NAV-8).
- **Probado en hardware por el usuario**: HOME durante partida, audio del
  core y NES/SNES/GBA con ROMs reales. El 2026-11-12 confirmó además las
  pruebas restantes de navegación, controles, guardados y Assets. El
  2026-11-13 confirmó UI-2, UI-10, CTRL-8, AS-6, CTRL-9, AUD-3 y APP-1.
  Reportó ruido en el audio de GBA. La conversión lineal AUD-1 no lo resolvió;
  AUD-2 solicita 48 000 Hz y usa remuestreo sinc, confirmado en Wii U el
  2026-11-20.
- **Input del core implementado**: A/B/X/Y/L/R, Start/Select y D-pad/stick.
- **Input UI**: GamePad (VPAD) + Pro Controller (WPAD) con detección en
  caliente. HOME durante una ROM abre el menú rápido de RetroU (pendiente de
  verificar en las cuatro apps tras el cambio de 2026-11-21).
- **Controles implementados y probados**: asignación de mandos a dos puertos
  del core en NES/SNES y uno en GBA, y remapeo estilo Our64 con múltiples
  botones por acción, `Y Add`, `X Unmap`, `- Reset`, captura con cuenta atrás y
  migración de `v1`/conf viejo; también orden de mandos, perfiles, mapas por ROM
  y apertura del menú rápido con HOME. La reconexión de Pro/Classic funciona
  (CTRL-8, confirmado en Wii U). GBA aplica al core el Pro elegido como Mando 1
  aunque la ROM tenga un mapa propio (CTRL-9, confirmado en Wii U).
- SRAM automática y cuatro slots con miniatura: probados en hardware
  (SAVE-1/2).
- Touch, búsqueda, vistas de biblioteca y descargas separadas de
  portadas/backdrops con progreso: probados en hardware (NAV-1/7, UI-3..9,
  AS-1..5). El atajo `Y Vista` (UI-10) está confirmado en Wii U.
  GBA ya recorre más allá del juego 144. Salir al menú de Wii U
  durante descargas activas funciona (AS-6, confirmado en Wii U).
- En la SD `E:/retrou/roms` se renombraron 262 ROMs por CRC Libretro el
  2026-11-20, con migración de guardados, portadas, favoritos y mapas
  asociados. Los manifiestos y el log de reversión están descritos en
  `docs/rom-renaming.md`. El usuario verificó ROM-1 en Wii U; las ROMs sin
  CRC conocido y los destinos duplicados no se tocaron.
- El 2026-11-21 se retiraron 29 ROMs duplicadas (13 Europa con alternativa
  USA y 16 copias de contenido idéntico). El respaldo verificable está en
  `native/build/rom-dedupe-20261121/`; ver `docs/rom-deduplication.md` (ROM-2).
  Donkey Kong 64 ya tenía nombre canónico, pero su portada era texto con
  extensión PNG; se reemplazó en la SD. `retrou_assets` ahora valida la
  cabecera PNG antes de saltar archivos existentes (AS-7), confirmado en Wii U.
  Una nueva incidencia de N64 requiere preferir el archivo real de Libretro
  sobre variantes `.png` que contienen texto (AS-8; pendiente de probar).
- Ajustes en dos paneles: nueva distribución según
  `ejemplos/Disposicion de menu.png`, confirmada en Wii U (UI-2).
- Audio GBA: mGBA informa 65 536 Hz. AUD-1 (lineal hacia 32 000 Hz) falló en
  Wii U. AUD-2 solicita 48 000 Hz y filtra con sinc antes de la salida SDL;
  el usuario confirmó que el audio funciona en Wii U el 2026-11-20.
- Sonido permite configurar frecuencia y búfer de salida por app NES/SNES/GBA
  en `audio.conf`; AUD-3 está confirmado en Wii U. La WUHB de N64 usa
  audio AX en Our64 y no lee este archivo.
- Los nombres visibles de las WUHB de NES, SNES y GBA ya son `RetroU NES`,
  `RetroU SNES` y `RetroU GBA` (APP-1, confirmado en Wii U).
- Referencias visuales de UI en `ejemplos/` (raíz del proyecto).
- No afirmar que algo está probado en hardware si no lo está.

## 12. Convenciones

- Español para textos de usuario, logs de UI y documentación de producto.
- Identificadores de código en inglés.
- Cambios pequeños y localizados.
- No reformatear archivos completos.
- No cambiar la arquitectura sin autorización.
- Mantener `AGENTS.md`, `README.md` y `native/README.md` actualizados cuando
  cambie el comportamiento.
- Si una compilación falla, investigar y reportar; no ocultar errores.
- Si se requiere hardware, decirlo explícitamente y no inventar resultados.

## 13. Protocolo para cambios futuros

Antes de tocar código, presentar al usuario:

0. el/los pendiente(s) de `PENDIENTES.md` que cubre la actividad (si no
   está registrado, agregarlo primero);
1. archivos que se crearán/modificarán;
2. explicación corta del cambio;
3. cómo se probará;
4. riesgos.

Esperar autorización. Después implementar en un cambio acotado y verificable.

Al terminar una actividad: si quedó probada en hardware, marcar su ID como
✅ en `PENDIENTES.md` (con fecha) y ajustar el resumen de la sección 11 si
cambia el estado general. Si no, dejarla 🔧/⬜ y documentar qué falta
verificar.

Validación del 2026-11-22: N64-13 confirmado en Wii U; HOME durante ROM
funciona. Siguen pendientes HOME/cierre de aplicación después de N64 y la
transición de arranque. Se evita duplicar callbacks WHB tras reiniciar GX2
y se mantiene el scanout apagado hasta el primer frame del motor.
Controles usa la misma navegación y orden en las cuatro apps, con cuatro
puertos N64, dos NES/SNES y uno GBA. La captura y lectura GamePad excluyen
los bits de emulación VPAD antes de añadir las direcciones de los sticks.
La cola de audio regula el ritmo del core para evitar acumular y descartar
muestras en partidas largas; su estabilidad sostenida queda pendiente (AUD-4).

Ronda del 2026-11-23: HOME (N64-11/NAV-8), controles (CTRL-11/12/13) e
integración N64 (N64-2/7) confirmados por el usuario en Wii U. Nuevos fallos:
segunda ROM N64 (N64-14), corrupción al salir de N64 (UI-12), ruido de GBA
incluso sin ROM (AUD-4) y un bloqueo aislado al volver de GBA al sistema (APP-2).
El adaptador AX del motor conserva la instancia que SDL ya tiene abierta y
reinicia el ring por ROM; el plugin gráfico se cierra antes de destruir GX2.
La transición mantiene las pantallas ocultas hasta tener el frame de biblioteca.
Los búferes SDL se redondean a ciclos AX de 3 ms (p. ej. 512 → 576 muestras);
UI y juego alimentan el audio por separado y el dispositivo se pausa al ceder
primer plano. Estas correcciones quedan pendientes de validar en hardware.

Actualización posterior del 2026-11-23: UI-12, AUD-4 (unos 30 minutos de
juego GBA) y APP-2 confirmados en Wii U. N64-14 sigue fallando antes de una
segunda entrada en el log del motor. La WUHB diagnóstica registra `n64_startup`
en `retrou_rpx.log`: entrada del hilo, validación del archivo/core y apertura
del log, publicados mediante atómicos y sin I/O en el hilo de arranque.
Estos hitos localizan el bloqueo; todavía no constituyen una corrección.

N64-14, diagnóstico del 2026-11-23: la segunda ROM alcanza la ejecución, pero
queda negra unos dos minutos según el usuario. Se detectó que el adaptador
reiniciaba los IDs de tareas de vídeo sin reiniciar los del puente RSP.
Se conservan ambos contadores monotónicos y se drenan las tareas con
`gfx_reset()` antes de cerrar el vídeo, siguiendo el teardown de Our64.
El arranque comprueba y registra la igualdad de contadores antes de ejecutar.
Pendiente validar en Wii U. APP-3 registra un fallo aislado anterior al splash:
la copia SD coincide por SHA-256 y ese intento no dejó un nuevo `main_enter`.

Ronda final N64 del 2026-11-23: el usuario confirmó N64-14 y APP-3 en Wii U.
La reapertura de ROM y el arranque funcionan. Ajustes de biblioteca confirmados:
cambiar de letra reinicia la selección al primer juego (NAV-9); N64 Lista
muestra la portada horizontal completa (UI-13). En las cuatro apps, touch
actúa al soltar: un tap selecciona otra ROM y un tap sobre la seleccionada
la ejecuta. Deslizar verticalmente navega Cards/Lista y horizontalmente el
carrusel, sin ejecutar ROMs; los huecos entre tarjetas/filas no se activan
(NAV-10). El usuario confirmó NAV-9, UI-13 y NAV-10 el 2026-11-23.
NAV-11 añade avance por páginas de cinco juegos en Lista en las cuatro apps;
el usuario lo confirmó el 2026-11-23. Sigue la ronda final DOC-2.

Publicación autorizada el 2026-11-23 bajo xoloDev: declarar asistencia de IA,
licenciar las aportaciones propias como MIT y conservar licencias ajenas,
incluida la condición no comercial de Snes9x. Incluir WUHB, RPX, licencias y
fuentes correspondientes en el release (REL-1); validar Tiramisu en hardware
(REL-2). La hoja de ruta CORE-1 abarca todos los cores Libretro para Wii U,
inventariados en `docs/libretro-wiiu-roadmap.md`, no solo MSX/PS1/Neo Geo.
El usuario pidió agrupar motores alternativos dentro del RetroU correspondiente,
con selector de core, símbolos aislados y un motor activo; los estados deben
separarse por motor (CORE-2). Es alcance futuro, no funcionalidad de v0.1.0.
Los cores multisistema se conservarán agrupados por familias coherentes,
por ejemplo RetroU SEGA, según lo autorizado por el usuario. Consolidar el
inventario de grupos principales antes de fijar el número final de apps.

Release v0.1.0 publicado el 2026-11-23 en Joguel96/RetroU: cuatro WUHB,
cuatro RPX, fuentes correspondientes y SHA256SUMS, con hashes remotos
verificados. La transferencia a xolodev fue solicitada por API; falta que la
cuenta destinataria la acepte. No afirmar ownership transferido hasta verlo
en GitHub. Los RPX en Tiramisu siguen sin prueba física (REL-2).
