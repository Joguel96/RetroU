# PLAN DE TRABAJO ANATÓMICO

> **Estado Global:** EN PROCESO (N64-2 permanece pendiente)
> **Orquestador:** Codex
> **Fecha:** 2026-11-13
> **Ejecución:** el usuario autorizó TASK-001–011, luego TASK-012–016 y finalmente TASK-017–021 el 2026-11-13. AUD-2 y AUD-3 de GBA quedan aplazados.
> **Avance:** TASK-001–020 tienen implementación, compilación y documentación. TASK-017 arrancó en Cemu y creó el log N64. TASK-021 depende de la matriz en Wii U, aún sin confirmar; N64-2 permanece 🔧. El ajuste visual de configuración se registró como N64-3 para después.

## Registro de commits de esta ronda

| Tarea | Repositorio | Commit |
|---|---|---|
| Plan | RetroU | `beeda76` |
| TASK-001 | RetroU | `432a1e5` |
| TASK-002 | Our64 | `93d365d` |
| TASK-003 | Our64 | `9ae8d2c` |
| TASK-004 | Our64 | `fc0b3ed` |
| TASK-005 | Our64 | `826e701` |
| TASK-006 | Our64 | `e855885` |
| TASK-007 | Our64 | `d2c11a5` |
| TASK-008 | Our64 | `c4a3cf3` |
| TASK-009 | Our64 | `7122e99` |
| TASK-010 | Our64 | `a2072c5` |
| TASK-011 | Our64 | `26c26a3` |
| Render bajo demanda de TASK-010 | Our64 | `ad07ca5` |
| TASK-012 | Our64 | `cd50bfa` |
| TASK-013 | Our64 | `73ca4d3` |
| TASK-014 | Our64 | `f7acfc5` |
| TASK-015 | Our64 | `5992648` |
| TASK-016 | Our64 | `0773b5f` |
| Contraste de diálogos TASK-015 | Our64 | `7b72e99` |
| TASK-017 | Our64 | `869861d` |
| Compatibilidad WUT 1.9.2 usada por TASK-017 | Our64 | `29de243` |
| TASK-018 | RetroU | `d1cc7c2` |
| TASK-019 | RetroU | `9cf0e3b` |
| TASK-020 | RetroU | `ac8062d` |

Compilación final: `C:/proyectos/Our64/player/build-release/logs/build-20261113-212647.log`.
WUHB copiada: `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`, SHA-256
`9d5a7dabeb0b079f3e14192bf84437cfef5450ca4889e37bba43c37ea9b3190b`.
La WUHB anterior se conservó como `RetroN64U.previous.wuhb`, SHA-256
`5f822245644f1634f05ff982561383b8d0bc15068328219d45118ee072b42177`.
La prueba de capturas de escritorio no pudo arrancar en MSYS2 por falta de
`freetype2`, `libpng`, `libturbojpeg` y `zlib`; la compilación Wii U y la prueba
portátil `test_covers` sí pasaron. Cemu arrancó y mostró la biblioteca, pero el
perfil de entrada disponible no permitió abrir ajustes para inspección visual.
La nueva WUHB volvió a arrancar en Cemu y generó `sd:/retrou/n64/retrou_n64.log`.
TASK-016 amplió los cambios a la biblioteca y al menú de descarga porque el
flujo anterior solo enumeraba juegos sin portada y omitía fondos faltantes.

---

## Tareas

### [ ] Tarea 1: Fijar contrato y línea base del port

- **ID:** TASK-001
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Documentar la frontera entre el motor N64 funcional y la interfaz RetroU, junto con la procedencia y las rutas de datos.
- **Archivos / Recursos Afectados:**
  - `docs/n64-port-contract.md`
- **Instrucción para el Worker Local:**
  > "Crear únicamente `C:/proyectos/RetroU/docs/n64-port-contract.md`. Registrar el hash y estado de `C:/proyectos/Our64`, el hash de `C:/proyectos/RetroU`, el checksum de `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb` y las licencias de Our64, Not64, GLideN64 y libgui. Definir que la WUHB N64 conserva Not64 + GLideN64 + audio AX, `video_call`/`ui_show`, hasta reemplazar solo la presentación y el flujo de menús. Precisar rutas destino: `sd:/retrou/roms/n64`, `sd:/retrou/n64/{config,saves,states,art,backdrops}` y `sd:/retrou/n64/retrou_n64.log`; preservar los datos de `sd:/wiiu/our64`. Describir una WUHB independiente `RetroU N64` y commits reversibles en el repositorio donde se edite cada archivo. No implementar el port. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] El documento contiene hashes, licencias, frontera técnica, rutas y estrategia de reversión sin cambios de código.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 2: Hacer trazable la compilación N64

- **ID:** TASK-002
- **Depende de:** TASK-001
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Guardar un log por compilación y comprobar el staging de la WUHB N64.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/tools/build-retrou-n64.sh`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/tools/build-retrou-n64.sh`: registrar comando, revisión Git, fecha, resultado, checksum de entrada y salida en un log con fecha dentro del build ignorado; conservar `make -C player RELEASE=1` y el destino `C:/proyectos/RetroU/dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`. Fallar si la compilación o copia falla. No tocar NES, SNES ni GBA. Compilar la línea base y confirmar checksum de origen y destino idénticos. Crear un commit descriptivo en Our64. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Compilación base exitosa, log conservado y WUHB copiada con checksum idéntico.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 3: Mover la raíz de datos N64

- **ID:** TASK-003
- **Depende de:** TASK-002
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Establecer `sd:/retrou/n64` como raíz de escritura de Our64 y preservar los datos anteriores.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/bench/source/report.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/bench/source/report.c` para que `report_sd_root()` monte la SD y cree `sd:/retrou/n64` en lugar de `sd:/wiiu/our64`; mantener el guardado incremental del log. No borrar ni mover datos antiguos. Registrar fallo de montaje. Compilar y verificar que `report_sd_root()` alimenta las rutas existentes sin truncamientos. Crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] El arranque escribe bajo `sd:/retrou/n64` y deja intacto `sd:/wiiu/our64`.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 4: Abrir la biblioteca N64 desde RetroU

- **ID:** TASK-004
- **Depende de:** TASK-003
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Leer ROMs desde la ruta canónica de RetroU y conservar acceso a la colección anterior.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/library_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/library_wiiu.c`: usar `sd:/retrou/roms/n64` por defecto; si no hay ROMs allí y existe `sd:/wiiu/our64/roms`, ofrecer esa carpeta como origen compatible sin copiar ROMs. Mantener lectura USB opcional, favoritos, orden y carga diferida de portadas; escribir datos nuevos solo en `sd:/retrou/n64`. Limitar tamaños de rutas y registrar carpeta elegida y número de ROMs. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Catálogo abre ROMs de la ruta nueva y de la antigua, sin duplicarlas ni borrar archivos.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 5: Preservar partidas N64 existentes

- **ID:** TASK-005
- **Depende de:** TASK-003
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Leer guardados antiguos y escribir los nuevos bajo RetroU.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/saves_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/saves_wiiu.c`: buscar primero `sd:/retrou/n64/saves/<id>.<ext>` y, solo si falta, leer `sd:/wiiu/our64/saves/<id>.<ext>`; al guardar, escribir de forma segura en la ruta nueva sin eliminar el original. Conservar formatos `.eep`, `.sra`, `.fla` y `.mpk`, límites de nombre y log de ruta usada. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Una partida antigua se carga y se guarda después en RetroU sin modificar su archivo original.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 6: Preservar estados N64 existentes

- **ID:** TASK-006
- **Depende de:** TASK-003
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Leer autosaves y slots anteriores sin cambiar su formato.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/states_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/states_wiiu.c`: preferir `sd:/retrou/n64/states/<id>`, usar `sd:/wiiu/our64/states/<id>` solo como lectura compatible si falta el nuevo, y escribir estados nuevos únicamente en RetroU. Mantener los cuatro slots, miniaturas, autosave y garantías de escritura actuales. Registrar carga y fallo con ruta. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Un estado antiguo restaura el juego y un estado nuevo reaparece tras reiniciar.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 7: Ajustar el puente gráfico a las pantallas RetroU

- **ID:** TASK-007
- **Depende de:** TASK-004, TASK-005, TASK-006
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Conservar la sincronización GX2 de Our64 al mostrar y ocultar los menús nuevos.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/ui_wiiu.cpp`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/ui_wiiu.cpp`: adaptar carga de recursos y ciclo de pantalla de RetroU manteniendo `video_call`, `video_set_hold`, último frame congelado, TV/GamePad y restauración del estado GX2 de GLideN64. No sustituir `video.cpp`, Not64 ni audio AX. Verificar entrada y salida repetidas del menú con logs y compilar; crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Las pantallas se muestran y vuelven al juego sin perder imagen ni bloquear la GPU.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 8: Definir el modelo de vistas RetroU N64

- **ID:** TASK-008
- **Depende de:** TASK-007
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Exponer al dibujado la grilla, la lista, el carrusel y los estados de assets sin mezclar lógica de emulación.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/ui.h`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/ui.h`: ampliar `roms_view`/`ui_view` con modo grilla/lista/carrusel, rango visible, selección, generación de portadas y backdrop, progreso de descarga y estado de sidebar, manteniendo el contrato C/C++ de `ui_show` y el tamaño de memoria acotado. No incorporar un segundo renderizador ni cambiar Not64. Compilar las pruebas de interfaz existentes y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] El modelo compila en C y C++, representa las tres vistas y limita recursos visibles.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 9: Alimentar las vistas de biblioteca RetroU

- **ID:** TASK-009
- **Depende de:** TASK-004, TASK-008
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Publicar selección, filtros, recursos visibles y progreso de assets en el modelo de vista N64.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/library_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/library_wiiu.c`: llenar el nuevo `roms_view` de `ui.h` para grilla/lista/carrusel, favoritos junto a All, índice #/A-Z, búsqueda, rango visible, portada y backdrop del seleccionado y progreso de descarga. Conservar lectura diferida con límite de memoria, procesar `covers_take_saved`/`library_cover_saved` y la generación de backdrops para refrescar durante la sesión. No bloquear el hilo de dibujo con lectura SD. Compilar, comprobar catálogo grande y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Los cambios de filtro, portada y backdrop actualizan la vista sin reiniciar y sin crecimiento continuo de memoria.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 10: Dibujar la biblioteca y navegación RetroU

- **ID:** TASK-010
- **Depende de:** TASK-007, TASK-008, TASK-009
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Sustituir la presentación de Our ROMs por la biblioteca RetroU.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/ui_draw.cpp`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/ui_draw.cpp`: dibujar biblioteca RetroU con grilla, lista, carrusel, estrella junto a All, índice #/A-Z, búsqueda, portadas y backdrop sin recorte; consumir el modelo de vista de `ui.h` y preservar el render bajo demanda. Mantener el motor de imagen de libgui en esta ronda, sin abrir un segundo dueño de GX2. Comparar con `C:/proyectos/RetroU/native/src/gx2/gx2_core_test_main.cpp` y las referencias de `C:/proyectos/RetroU/ejemplos`. Ejecutar las pruebas de dibujo de Our64, compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Capturas de biblioteca N64 coinciden con la jerarquía visual RetroU y las tres vistas caben en TV y GamePad.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 11: Cambiar el flujo de arranque y regreso

- **ID:** TASK-011
- **Depende de:** TASK-010
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Abrir directamente la biblioteca RetroU y regresar allí al cerrar un juego.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/main.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/main.c`: sustituir el prompt inicial de Our64 por biblioteca RetroU; conservar selección TV/GamePad en ajustes, carga de ROM, cierre ProcUI, guardado ordenado y retorno a biblioteca tras salir del core. Registrar etapas `boot`, `library`, `load`, `run`, `pause`, `save` y `exit` en el log N64. No alterar emulación, audio AX ni utilidades de diagnóstico. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Arranque → biblioteca → juego → menú → biblioteca → HOME funciona sin cierre inesperado.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 12: Unificar navegación y mandos N64

- **ID:** TASK-012
- **Depende de:** TASK-011
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Navegar la interfaz RetroU con GamePad, Pro, Classic, Wiimote y Nunchuk sin perder los cuatro puertos N64.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/input_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/input_wiiu.c`: traducir navegación RetroU con D-Pad/sticks, A, B, Plus y atajos de menú por familia de mando, manteniendo reconexión, orden y hasta cuatro puertos del core N64. Conservar el esquema N64 de A/B/Z/C/Start/stick; no leer directamente `control-map.conf` v2 de NES/SNES/GBA. Registrar conexión, desconexión y reasignación. Probar la lógica pura de `input.c`, compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Un Pro o Classic reconectado conserva su puerto y puede abrir el menú sin tomar el GamePad.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 13: Integrar controles y ajustes N64

- **ID:** TASK-013
- **Depende de:** TASK-012
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Incorporar a RetroU los ajustes de orden y mapeo propios de N64.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/setup_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/setup_wiiu.c`: ofrecer desde el sidebar RetroU controles N64, cambiar orden de 1–4 jugadores, capturar y añadir/desasignar botones por GamePad/Pro/Classic/Wiimote/Nunchuk, perfiles y acceso a sonido/video/juegos. Guardar configuración N64 en `sd:/retrou/n64/config`, con lectura compatible de `sd:/wiiu/our64/config` cuando no exista la nueva. No sobrescribir ajustes de NES/SNES/GBA. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Orden, mapeo y ajustes N64 persisten tras reiniciar y no afectan a las otras WUHB.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 14: Integrar el menú en partida

- **ID:** TASK-014
- **Depende de:** TASK-012
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Dar al menú pausado N64 el flujo de RetroU conservando sus operaciones de guardado.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/pause_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/pause_wiiu.c`: organizar el menú en partida como RetroU, con continuar, cuatro slots con miniatura, cargar, guardar, controles y salir a biblioteca o Wii U. Mantener pausa del core y confirmaciones antes de operaciones destructivas; no cambiar el formato `.o64s`. Registrar cada operación y resultado. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Guardar, cargar, reanudar y salir funcionan de forma repetida sin perder el frame ni el audio.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 15: Dibujar ajustes y menú en partida

- **ID:** TASK-015
- **Depende de:** TASK-013, TASK-014
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Completar la identidad visual RetroU en ajustes, controles y pausa.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/ui_draw.cpp`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/ui_draw.cpp`: dibujar sidebar, ajustes, orden, mapeo N64 y menú en partida en estilo RetroU; mantener feedback de captura, botón seleccionado sobre imagen de control, miniaturas y leyendas según dispositivo. Conservar estados `ui.h`, contraste y escala TV/GamePad. Ejecutar `tests/ui_shots.cpp`, inspeccionar capturas, compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Las pantallas previstas están completas y son legibles en TV y GamePad, sin controles cortados.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 16: Completar portadas y backdrops

- **ID:** TASK-016
- **Depende de:** TASK-010
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Descargar portadas y backdrops faltantes, actualizar la vista y detener las tareas al salir.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/source/covers_wiiu.c`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/source/covers_wiiu.c`: mantener descarga de portadas faltantes bajo `sd:/retrou/n64/art`, añadir backdrops faltantes bajo `sd:/retrou/n64/backdrops`, informar terminadas/fallidas y publicar generación nueva para que la biblioteca refresque sin reinicio. Conservar la cancelación y unión del hilo al salir al menú Wii U, la tabla de identificadores de Our64 y el límite de memoria. No descargar a `retroarch`. Registrar inicio, progreso, cancelación y resultado. Compilar y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] Portada y backdrop aparecen sin reiniciar y el cierre con descarga activa termina sin colgar la consola.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [x] Tarea 17: Finalizar nombre, recursos y log N64

- **ID:** TASK-017
- **Depende de:** TASK-015, TASK-016
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Empaquetar la WUHB como RetroU N64 con recursos y log separados.
- **Archivos / Recursos Afectados:**
  - `C:/proyectos/Our64/player/Makefile`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/Our64/player/Makefile`: mantener `APP_NAME`/`APP_SHORTNAME` como `RetroU N64`, integrar los recursos autorizados de `C:/proyectos/RetroU/assets/console/n64` y `assets/controles/n64.png`, compilar el log como `retrou_n64.log` dentro de `sd:/retrou/n64`, y conservar Not64, GLideN64, AX y sus licencias. No incluir SDL/Libretro ni alterar los otros targets. Compilar RELEASE=1 y crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [x] WUHB arranca en Cemu, se identifica como RetroU N64 y genera log N64 propio.
- **Implementación real:** además del Makefile se registraron los tres PNG de
  metadata, la imagen del mando integrada y el filtro de logs `INPUT:`,
  `PAUSE:` y `ASSETS:`; los cuatro parches WUT existentes quedaron en un
  commit separado para que el código de la WUHB tenga una revisión fija.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [x] Tarea 18: Actualizar el resumen del proyecto

- **ID:** TASK-018
- **Depende de:** TASK-017
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Documentar arquitectura, rutas y alcance real de RetroU N64.
- **Archivos / Recursos Afectados:**
  - `README.md`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/RetroU/README.md`: describir N64 como WUHB independiente basada en Not64 + GLideN64 + AX y con interfaz RetroU; registrar build en Our64, staging, rutas nuevas bajo `retrou`, lectura compatible de datos anteriores y prueba en hardware pendiente o confirmada según evidencia. Aclarar que N64 no usa Libretro y que AUD-2/AUD-3 de GBA siguen pendientes. Crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [x] README coincide con el binario construido y no declara verificación inexistente.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [x] Tarea 19: Actualizar instrucciones de compilación nativa

- **ID:** TASK-019
- **Depende de:** TASK-017
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Explicar el build y la recuperación de N64 sin mezclarlo con CMake Libretro.
- **Archivos / Recursos Afectados:**
  - `native/README.md`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/RetroU/native/README.md`: documentar que NES/SNES/GBA salen de CMake y que N64 sale de `C:/proyectos/Our64/tools/build-retrou-n64.sh`, con versión fijada, log de build, destino `dist/wiiu/apps/RetroN64U/RetroN64U.wuhb`, log runtime N64 y procedimiento de volver al hash/WUHB previo. No sugerir un core Libretro N64. Crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [x] Las instrucciones de build reproducen N64 sin tocar los tres targets GX2.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [x] Tarea 20: Documentar la ronda de hardware N64

- **ID:** TASK-020
- **Depende de:** TASK-017
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Dejar una matriz reproducible de validación antes de sustituir la WUHB funcional.
- **Archivos / Recursos Afectados:**
  - `docs/hardware-validation.md`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/RetroU/docs/hardware-validation.md`: agregar pruebas N64 en Wii U para arranque, ROMs `.z64/.n64/.v64`, grilla/lista/carrusel, favoritos, búsqueda, portadas, GamePad/Pro/Classic/Wiimote/Nunchuk, 1–4 jugadores, reconexión, atajos de menú, guardado SRAM, cuatro estados, audio AX, retorno y HOME, salida con descarga activa y datos antiguos. Anotar hash de cada WUHB, hash del commit Our64 y ruta `sd:/retrou/n64/retrou_n64.log`. No afirmar que pasó ninguna prueba no realizada. Crear commit descriptivo. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [x] La matriz diferencia claramente pruebas en Cemu, en hardware y pendientes.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.

---

### [ ] Tarea 21: Cerrar N64-2 tras confirmación real

- **ID:** TASK-021
- **Depende de:** TASK-020, TASK-018, TASK-019 y confirmación de hardware del usuario
- **Herramientas MCP / Skills:** `file_system, git`
- **Objetivo:** Marcar terminado N64-2 solo después de superar la matriz de hardware.
- **Archivos / Recursos Afectados:**
  - `PENDIENTES.md`
- **Instrucción para el Worker Local:**
  > "Modificar solo `C:/proyectos/RetroU/PENDIENTES.md` una vez que el usuario confirme toda la ronda N64 en Wii U: marcar N64-2 como ✅ con fecha y hashes de build/commits. Si falla algo, dejar N64-2 🔧, describir el fallo y conservar la WUHB N64 anterior como rollback. No cambiar AUD-2 ni AUD-3: el audio GBA se retomará en otra actividad. Crear commit descriptivo y mostrar el hash. Imprimir `[TASK_COMPLETE]` al finalizar."
- **Verificación de Éxito:**
  - [ ] N64-2 solo figura ✅ con evidencia de hardware; AUD-2 y AUD-3 siguen abiertos.
- **Criterio de Parada:** Imprimir `[TASK_COMPLETE]` al finalizar.
