# PENDIENTES.md — RetroU

Fuente única de pendientes del proyecto, pensada para que **varios agentes
queden alineados**. Antes de empezar cualquier actividad, revisar este
archivo; al terminar un pendiente **y verificarlo en hardware**, marcarlo
como terminado aquí (y en `AGENTS.md` si cambia el estado general).

## Estados

| Marca | Significado |
|---|---|
| ⬜ | Pendiente |
| 🔧 | En curso (decir quién/rama) |
| ✅ | Terminado **y probado en hardware** (anotar fecha) |
| ❌ | Descartado (anotar por qué) |

Regla del proyecto: **no afirmar que algo está probado en hardware si no lo
está**. Un cambio compilado pero sin verificar sigue siendo ⬜/🔧.

Ronda en Wii U del 2026-11-12: el usuario confirmó las demás pruebas de
navegación, controles, guardados y Assets de NES/SNES/GBA. Detectó dos fallos
nuevos: bloqueo al salir con descargas activas y pérdida de asignación al
reconectar un mando externo. La referencia de Ajustes ya está en
`ejemplos/Disposicion de menu.png`. N64 se confirmó después por separado.

Ronda del 2026-11-13: UI-2, CTRL-8 y AS-6 quedaron confirmados en Wii U.
CTRL-9 también quedó confirmado tras probar la WUHB corregida. El usuario
reportó ruido en el audio del core de GBA. La conversión lineal de AUD-1
se probó y el ruido persistió; AUD-2 reemplaza ese intento.

---

## 1. Confirmado por el usuario (hardware)

| ID | Pendiente | Estado |
|---|---|---|
| OK-1 | Menú HOME durante partida (abrir/cerrar, juego pausado) | ✅ 2026-11-11 |
| OK-2 | Audio del core (silencios/cortes resueltos) | ✅ 2026-11-11 |
| OK-3 | NES/SNES/GBA con ROMs reales en hardware | ✅ 2026-11-11 |
| OK-4 | Savestates y carátulas funcionan | ✅ 2026-11-11 |
| OK-5 | Pro Controller se conecta (WPADEnableURCC/WiiRemote) | ✅ 2026-11-11 |

## 2. Verificaciones del mapeo

| ID | Pendiente | Estado |
|---|---|---|
| VER-1 | Remapeo invertido por botón físico (formato `v1` de `control-map.conf`, migración del conf viejo) — el mapeo se guarda bien | ✅ 2026-11-11 |
| VER-2 | Asignación de mandos a los 2 puertos del core (Mando 1/Mando 2) — GamePad en ambos puertos confirmado (CTRL-6) | ✅ 2026-11-11 |
| VER-3 | Botones del Pro Controller llegando al core con el mapa invertido | ✅ 2026-11-11 |
| VER-4 | HOME no reasigna el mando al GamePad tras abrir/cerrar (sticky states) | ✅ 2026-11-11 |
| VER-5 | Signo del stick L como D-Pad (`+y = arriba`, convención Our64) | ✅ 2026-11-11 |

## 3. Controles

| ID | Pendiente | Estado |
|---|---|---|
| CTRL-1 | Remapeo desde «Controles» en el menú de partida (Minus+Plus), con configuración propia por ROM en `rom-maps/<hash>.conf` y restauración del mapa global al salir | ✅ 2026-11-12 |
| CTRL-2 | Tres perfiles guardables/cargables para GamePad y Pro/Classic por consola en `profiles/`; `Plus` desde el editor abre la lista, `A` carga y `Y` guarda | ✅ 2026-11-12 |
| CTRL-3 | Visual de asignación y mapeo estilo Our64 con UI RetroU, imágenes aportadas, formato `v2`, acciones `none`, un puerto en GBA y fuentes `Stick L`/`Stick R`. «Cambiar orden de mandos» asigna al pulsar A en cada dispositivo. Una máscara inválida usa el mapa predeterminado y se registra en el log. Referencias en `ejemplos/Our64/` | ✅ 2026-11-12 |
| CTRL-4 | Que los mandos externos (Pro, Classic, Wii Remote...) también naveguen la UI (biblioteca/ajustes/menú del core), no solo el GamePad. Implementación: `wpadNavBits()` traduce sus botones a las constantes VPAD (Pro/Classic: A,B,X,Y,L,R,ZL→L,ZR→R,+,-,D-pad; Wii Remote: A,B,+,-,D-pad) y el stick se comporta como D-Pad edge-triggered | ✅ 2026-11-11 |
| CTRL-5 | Diagnóstico temporal de input reducido a un resumen compacto por ROM tras confirmar VER-1..5; se conservan logs de carga y guardado de mapas/perfiles | ✅ 2026-11-12 |
| CTRL-6 | GamePad asignable a **cualquier mando del core** (Mando 1 y Mando 2). `setVpadPort()` + opciones de Mando 2 con GamePad. Extendido: se listan **todos** los mandos conectados (4 canales KPAD + GamePad, sin límite de 2) y se nombran por tipo (Wii U Pro Controller, Classic Controller, Wii Remote, Wii Remote + Nunchuk) con sufijo numérico solo si hay varios del mismo tipo | ✅ 2026-11-11 |
| CTRL-7 | Configurar por tipo de mando la combinación global que abre el menú durante el core, con opciones compatibles para GamePad, Pro/Classic y Wii Remote/Nunchuk; conservar Minus+Plus como emergencia y persistir el ajuste | ✅ 2026-11-12 |
| CTRL-8 | Al desconectar y reconectar Wii U Pro Controller o Classic Controller durante la app, conservar o restaurar su asignación a Mando 1/Mando 2 y su mapeo; comprobar también la navegación de UI tras reconectar | ✅ 2026-11-13 |
| CTRL-9 | GBA: al elegir Wii U Pro Controller como Mando 1, una ROM con mapa propio debe conservar ese mando durante el juego; el archivo por ROM solo modifica botones, no el orden global | ✅ 2026-11-13 |
| CTRL-10 | Presentar la asignación de mandos como tarjetas por puerto con «Pulsa A para conectar», selección visible y acciones de mapeo y orden, según `ejemplos/organizacion de controles para retrou.png`. RetroU conserva uno o dos puertos según consola; N64 Dev ya compila con cuatro y borde blanco del jugador seleccionado, pendiente probar en Wii U. | 🔧 2026-11-18 |

## 4. UI / Apariencia

| ID | Pendiente | Estado |
|---|---|---|
| UI-1 | Redondeo de cards: **bordes superiores rectos, base redondeada** (primera versión estaba invertida; corregida según `ejemplos/Retrou_resultados/error_cards.png`) | ✅ 2026-11-11 |
| UI-2 | Ajustes en dos paneles: secciones a la izquierda y opciones a la derecha según `ejemplos/Disposicion de menu.png`; conservar navegación con mando y touch | ✅ 2026-11-13 |
| UI-3 | Tres vistas de juegos: cards, lista y carrusel, elegibles en Personalizar; lista/carrusel con filtro alfabético y navegación propia. Referencias: `ejemplos/Our64/carrousel.png` y `list.png` | ✅ 2026-11-12 |
| UI-4 | Casilla de «Fondo dinámico» en Ajustes → Personalizar; valor guardado por consola en `view.conf` | ✅ 2026-11-12 |
| UI-5 | Carrusel y lista usan el backdrop de la ROM enfocada cuando el fondo dinámico está activo | ✅ 2026-11-12 |
| UI-6 | Tiempo de aparición del backdrop configurable en Personalizar: 0, 1, 3, 5 o 10 segundos sin cambio de selección; 3 s por defecto y lectura compatible con `view.conf` anterior | ✅ 2026-11-12 |
| UI-7 | Carrusel NES con portadas verticales y SNES con carátulas completas sin recorte, incluida la imagen central y las laterales | ✅ 2026-11-12 |
| UI-8 | En Lista y Carrusel, mostrar estrella en juegos favoritos y Favoritos inmediatamente después de «All» en el filtro superior, navegable con L/R y touch | ✅ 2026-11-12 |
| UI-9 | Investigar el cierre de GBA cerca del juego 144: títulos y rutas de assets normalizados; una sola portada de respaldo y registros de índice, memoria libre y fin de render por selección. El usuario confirmó que la nueva WUHB recorre la biblioteca en hardware | ✅ 2026-11-12 |
| UI-10 | Y cambia entre cards, lista y carrusel desde la biblioteca en NES/SNES/GBA, conserva el juego seleccionado y guarda la vista por consola. Favoritos sigue en la barra superior con L/R | ✅ 2026-11-13 |
| UI-11 | En Vídeo, elegir salida TV, GamePad o ambas para NES/SNES/GBA; guardar por consola y verificar en Wii U | 🔧 2026-11-13 (Codex, main) |
| ART-1 | Nuevos iconos y splash (icon/tv-splash/drc-splash) por consola (NES/SNES/GBA/N64) desde `assets/RetroU_consolas_12_PNG/` → `assets/console/<consola>/`; N64 además en `C:/proyectos/Our64/assets/console/n64/` y regenerar `RetroN64U.wuhb` | ✅ 2026-11-12 |

## 5. Navegación / Input UI

| ID | Pendiente | Estado |
|---|---|---|
| NAV-1 | Touch del GamePad funcionando en la navegación de la UI; taps integrados con las acciones del mando en biblioteca, ajustes, sidebar, buscador, mapeo y menú del core | ✅ 2026-11-12 |
| NAV-2 | ( = CTRL-4, referencia cruzada: navegación con mandos externos ) | ✅ 2026-11-11 |
| NAV-3 | El sidebar solo se aplica en la vista cards y se muestra con **Minus** (en lista/carrusel no existe sidebar) | ✅ 2026-11-11 |
| NAV-4 | **Plus** es el botón que abre Ajustes (ya no el sidebar); **L/R ya no navega entre biblioteca y ajustes** | ✅ 2026-11-11 |
| NAV-5 | **B jerárquico**: B regresa un nivel a la vez hasta volver a la biblioteca (ej.: ajustes→personalizar→vista, B vuelve vista→personalizar, luego personalizar→ajustes, luego ajustes→biblioteca) | ✅ 2026-11-11 |
| NAV-6 | El **stick L navega toda la UI** (biblioteca, Ajustes, sidebar, remapeo, menú del core), no solo el D-Pad. Edge-triggered: un empuje = un paso (igual que el D-Pad) | ✅ 2026-11-11 |
| NAV-7 | El buscador usa B para borrar; con la consulta vacía B sale. X acepta y sale | ✅ 2026-11-12 |

## 6. Savestates

| ID | Pendiente | Estado |
|---|---|---|
| SAVE-1 | Cuatro slots de savestate por juego con miniatura PNG del frame; selector en menú del core | ✅ 2026-11-12 |
| SAVE-2 | SRAM cargada tras abrir ROM y escrita al guardar estado, salir del juego y cerrar app; escritura temporal + rename | ✅ 2026-11-12 |

## 7. Assets (descarga)

| ID | Pendiente | Estado |
|---|---|---|
| AS-1 | Descargas manuales separadas de portadas y backdrops en Assets; buscar nombres reales en el índice de Libretro, registrar errores HTTP/red, poner en cola peticiones durante la actualización de metadata y conservar el archivo anterior si falla un reemplazo forzado | ✅ 2026-11-12 |
| AS-2 | Metadata se actualiza al iniciar aun con auto-descarga apagada | ✅ 2026-11-12 |
| AS-3 | Modo «Solo faltantes» / «Descargar todo» para descarga manual; auto-descarga usa solo faltantes. El contador incluye únicamente imágenes pendientes al reanudar, sin metadata ni archivos existentes | ✅ 2026-11-12 |
| AS-4 | Barra y contador de progreso de descarga en Assets | ✅ 2026-11-12 |
| AS-5 | Actualizar las portadas visibles en Cards, Lista y Carrusel al terminar una descarga, sin reiniciar la app | ✅ 2026-11-12 |
| AS-6 | Al salir de RetroU hacia el menú de Wii U mientras hay descargas activas, terminar o cancelar el trabajo y cerrar sin bloquear la consola; conservar un log del cierre | ✅ 2026-11-13 |

## 8. Documentación / cierre

Guía de la ronda en Wii U: `docs/hardware-validation.md`.

| ID | Pendiente | Estado |
|---|---|---|
| DOC-1 | Actualizar `README.md` y `native/README.md` con la arquitectura WUHB actual, controles `v2`, guardados, Assets y logs; funciones documentadas confirmadas en hardware, nuevas correcciones identificadas como pendientes | ✅ 2026-11-12 |
| DOC-2 | Validación final en hardware de todo el conjunto (ronda de pruebas completa con los 4 WUHB) | ⬜ |

## 9. N64 (RetroU N64)

| ID | Pendiente | Estado |
|---|---|---|
| N64-1 | Probar `RetroN64U.wuhb` en hardware (generada desde Our64, rebrandeada); el usuario confirmó que es funcional | ✅ 2026-11-12 |
| N64-2 | Integrar N64 en el frontend real de RetroU. El usuario confirmó el 2026-11-18 en Wii U que `RetroN64U-Dev` inició Banjo-Kazooie y permitió jugar; el log del motor registra 1 877 VI y salida con resultado 0 al pulsar HOME. La WUHB Dev recompilada tiene SHA-256 `8c9edfcf0477f57fb4b30b98e0277873c17b7d604901e719d6b8dd4c73e33745`; la WUHB provisional en `dist/wiiu/apps/RetroN64U/` sigue separada. El port aún requiere menú durante el juego, salida HOME estable y validación de controles/guardados. | 🔧 2026-11-18 |
| N64-3 | Igualar visualmente la pantalla de configuración N64 con la de RetroU NES/SNES/GBA. El usuario observó que la UI actual difiere y pidió afinarla después de TASK-017–021. | ⬜ |
| N64-4 | En el buscador N64, B borra un carácter y solo sale si la consulta está vacía; X acepta y cierra | 🔧 2026-11-13 (Codex, Our64) |
| N64-5 | Usar `retrou_assets` en el frontend N64 de RetroU: índice, portadas y fondos desde `thumbnails.libretro.com/Nintendo - Nintendo 64/`, con descarga automática y progreso como las otras consolas. | 🔧 2026-11-14 |
| N64-6 | Integrar en Controles RetroU el flujo de «Pulsa A para conectar» por puerto. N64 Dev asigna directamente el mando nuevo al siguiente puerto libre con A; falta prueba en Wii U. NES/SNES/GBA conservan su flujo anterior. | 🔧 2026-11-18 |
| N64-7 | Reemplazar el frontend de Our64 por RetroU y enlazar solo Not64 + GLideN64. La ejecución de ROM ya se verificó en Wii U el 2026-11-18. El target Dev ahora presenta cuatro tarjetas de mando y aplica mapas de 18 acciones al motor; falta validar esta compilación en hardware y conectar el menú/guardados durante la partida. | 🔧 2026-11-18 |
| N64-8 | Igualar las portadas laterales del carrusel N64 al carrusel SNES: área horizontal de 270×260 y ajuste completo de la imagen. Compilado en la WUHB de desarrollo; falta comprobarlo visualmente. | 🔧 2026-11-14 |
| N64-9 | Controles N64: cuatro tarjetas, A asigna en la pantalla principal, izquierda/derecha mueve el foco, jugador seleccionado permanece con borde blanco al usar acciones, y marcadores del mando coinciden con su imagen. Compilado para Dev; falta prueba visual y funcional en Wii U. | 🔧 2026-11-18 |
| N64-10 | Abrir el menú de RetroU desde el juego N64 con la combinación configurada, manteniendo la sesión y sus guardados. La ruta actual no lee el atajo mientras corre el hilo N64; requiere integrar la pausa y el dibujo del menú con GLideN64. | ⬜ 2026-11-18 |
| N64-11 | Evitar el bloqueo al pulsar HOME durante N64. El motor terminó normalmente (1 877 VI, resultado 0), pero la app se congeló después de `n64_gx2_restored`. Dev espera una nueva adquisición del primer plano y registra el primer cuadro; falta repetir la prueba en Wii U. | 🔧 2026-11-18 |

## 10. Audio del core

| ID | Pendiente | Estado |
|---|---|---|
| AUD-1 | Conversión lineal de 65 536 Hz a la salida de 32 000 Hz para el core GBA | ❌ 2026-11-13: probada en Wii U; el ruido persistió |
| AUD-2 | Corregir el ruido de GBA siguiendo la ruta de RetroArch: salida Wii U a 48 000 Hz y remuestreo con filtro sinc antes de encolar; comprobar el resultado en hardware | 🔧 En curso (Codex, rama main); falta Wii U |
| AUD-3 | Configurar frecuencia de salida y tamaño del búfer desde Sonido, guardados en `audio.conf` independiente para NES/SNES/GBA; aplicar al próximo inicio y verificar en Wii U | ✅ 2026-11-13 |

## 11. Nombre de las apps

| ID | Pendiente | Estado |
|---|---|---|
| APP-1 | Mostrar `RetroU NES`, `RetroU SNES` y `RetroU GBA` como nombre de las WUHB, sin `GX2`; verificar en el menú de Wii U | ✅ 2026-11-13 |

## 12. Ejemplos gráficos (referencias Our64)

Carpeta del proyecto raíz: **`ejemplos/`** — ahí se adjuntan capturas/mocks
de referencia (p. ej. el apartado de controles de Our64 para CTRL-3). Ver
`ejemplos/README.md` para el índice esperado.

---

## Protocolo de actualización de este archivo

1. Al **empezar** una actividad: pasar su ID a 🔧 y anotar quién/rama.
2. Al **terminar** el código: seguir 🔧 hasta haberlo probado.
3. Al **verificar en hardware**: pasar a ✅ con fecha y quitar/bajar el
   diagnóstico temporal asociado si aplica.
4. Un pendiente **nunca** se marca ✅ solo porque compila.
5. Nuevos pendientes se agregan con el siguiente ID libre de su sección.
