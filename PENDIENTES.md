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

Ronda en Cemu del 2026-11-12: el usuario confirmó el flujo de navegación y
biblioteca (pasos 1–5), controles (1–5), guardados y Assets (1–4). La
distribución visual de Ajustes queda pendiente de su próxima referencia.
Estas confirmaciones no cambian el estado de las pruebas en Wii U.

---

## 1. Confirmado por el usuario (hardware)

| ID | Pendiente | Estado |
|---|---|---|
| OK-1 | Menú HOME durante partida (abrir/cerrar, juego pausado) | ✅ 2026-11-11 |
| OK-2 | Audio del core (silencios/cortes resueltos) | ✅ 2026-11-11 |
| OK-3 | NES/SNES/GBA con ROMs reales en hardware | ✅ 2026-11-11 |
| OK-4 | Savestates y carátulas funcionan | ✅ 2026-11-11 |
| OK-5 | Pro Controller se conecta (WPADEnableURCC/WiiRemote) | ✅ 2026-11-11 |

## 2. Verificación pendiente (implementado, sin confirmar)

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
| CTRL-1 | Remapeo desde «Controles» en el menú de partida (Minus+Plus), con configuración propia por ROM en `rom-maps/<hash>.conf` y restauración del mapa global al salir. Compilado; falta probar en hardware | 🔧 En curso (Codex, rama main) |
| CTRL-2 | Tres perfiles guardables/cargables para GamePad y Pro/Classic por consola en `profiles/`; `Plus` desde el editor abre la lista, `A` carga y `Y` guarda. Compilado; falta probar en hardware | 🔧 En curso (Codex, rama main) |
| CTRL-3 | Visual de asignación y mapeo estilo Our64 con UI RetroU, imágenes aportadas, formato `v2`, acciones `none`, un puerto en GBA y fuentes `Stick L`/`Stick R`. «Cambiar orden de mandos» asigna al pulsar A en cada dispositivo. Una máscara inválida usa el mapa predeterminado y se registra en el log. Compilado; falta probar flujo y detalles visuales en hardware. Referencias en `ejemplos/Our64/` | 🔧 En curso (Codex, rama main) |
| CTRL-4 | Que los mandos externos (Pro, Classic, Wii Remote...) también naveguen la UI (biblioteca/ajustes/menú del core), no solo el GamePad. Implementación: `wpadNavBits()` traduce sus botones a las constantes VPAD (Pro/Classic: A,B,X,Y,L,R,ZL→L,ZR→R,+,-,D-pad; Wii Remote: A,B,+,-,D-pad) y el stick se comporta como D-Pad edge-triggered | ✅ 2026-11-11 |
| CTRL-5 | Diagnóstico temporal de input reducido a un resumen compacto por ROM tras confirmar VER-1..5; se conservan logs de carga y guardado de mapas/perfiles. Compilado; falta verificar log en hardware | 🔧 En curso (Codex, rama main) |
| CTRL-6 | GamePad asignable a **cualquier mando del core** (Mando 1 y Mando 2). `setVpadPort()` + opciones de Mando 2 con GamePad. Extendido: se listan **todos** los mandos conectados (4 canales KPAD + GamePad, sin límite de 2) y se nombran por tipo (Wii U Pro Controller, Classic Controller, Wii Remote, Wii Remote + Nunchuk) con sufijo numérico solo si hay varios del mismo tipo | ✅ 2026-11-11 |
| CTRL-7 | Configurar por tipo de mando la combinación global que abre el menú durante el core, con opciones compatibles para GamePad, Pro/Classic y Wii Remote/Nunchuk; conservar Minus+Plus como emergencia y persistir el ajuste. Compilado; falta probar en hardware | 🔧 En curso (Codex, rama main) |

## 4. UI / Apariencia

| ID | Pendiente | Estado |
|---|---|---|
| UI-1 | Redondeo de cards: **bordes superiores rectos, base redondeada** (primera versión estaba invertida; corregida según `ejemplos/Retrou_resultados/error_cards.png`) | ✅ 2026-11-11 |
| UI-2 | Ajustes en dos paneles: opciones a la izquierda y contexto a la derecha. El usuario confirmó el flujo, pero la distribución visual requiere una referencia adicional que aportará en `ejemplos/`. Falta revisión final en hardware | 🔧 En curso (Codex, rama main) |
| UI-3 | Tres vistas de juegos: cards, lista y carrusel, elegibles en Personalizar; lista/carrusel con filtro alfabético y navegación propia. Compilado; falta revisar presentación y flujo en hardware. Referencias: `ejemplos/Our64/carrousel.png` y `list.png` | 🔧 En curso (Codex, rama main) |
| UI-4 | Casilla de «Fondo dinámico» en Ajustes → Personalizar; valor guardado por consola en `view.conf`. Compilado; falta probar en hardware | 🔧 En curso (Codex, rama main) |
| UI-5 | Carrusel y lista usan el backdrop de la ROM enfocada cuando el fondo dinámico está activo. Compilado; falta comprobar en hardware | 🔧 En curso (Codex, rama main) |
| UI-6 | Tiempo de aparición del backdrop configurable en Personalizar: 0, 1, 3, 5 o 10 segundos sin cambio de selección; 3 s por defecto y lectura compatible con `view.conf` anterior. Compilado; falta probar el tiempo elegido en hardware | 🔧 En curso (Codex, rama main) |
| UI-7 | Carrusel NES con portadas verticales y SNES con carátulas completas sin recorte, incluida la imagen central y las laterales. Compilado; falta comprobar en hardware | 🔧 En curso (Codex, rama main) |
| UI-8 | En Lista y Carrusel, mostrar estrella en juegos favoritos y Favoritos inmediatamente después de «All» en el filtro superior, navegable con L/R y touch. Falta comprobar en hardware | 🔧 En curso (Codex, rama main) |
| UI-9 | Investigar y corregir el cierre de GBA al recorrer Cards hasta la letra P; limitar texturas retenidas y registrar la posición antes de cargar portadas. Falta reproducir y validar en Cemu/hardware | 🔧 En curso (Codex, rama main) |
| ART-1 | Nuevos iconos y splash (icon/tv-splash/drc-splash) por consola (NES/SNES/GBA/N64) desde `assets/RetroU_consolas_12_PNG/` → `assets/console/<consola>/`; N64 además en `C:/proyectos/Our64/assets/console/n64/` y regenerar `RetroN64U.wuhb` | ✅ 2026-11-12 |

## 5. Navegación / Input UI

| ID | Pendiente | Estado |
|---|---|---|
| NAV-1 | Touch del GamePad funcionando en la navegación de la UI; taps integrados con las acciones del mando en biblioteca, ajustes, sidebar, buscador, mapeo y menú del core. Falta probar en hardware | 🔧 En curso (Codex, rama main) |
| NAV-2 | ( = CTRL-4, referencia cruzada: navegación con mandos externos ) | ✅ 2026-11-11 |
| NAV-3 | El sidebar solo se aplica en la vista cards y se muestra con **Minus** (en lista/carrusel no existe sidebar) | ✅ 2026-11-11 |
| NAV-4 | **Plus** es el botón que abre Ajustes (ya no el sidebar); **L/R ya no navega entre biblioteca y ajustes** | ✅ 2026-11-11 |
| NAV-5 | **B jerárquico**: B regresa un nivel a la vez hasta volver a la biblioteca (ej.: ajustes→personalizar→vista, B vuelve vista→personalizar, luego personalizar→ajustes, luego ajustes→biblioteca) | ✅ 2026-11-11 |
| NAV-6 | El **stick L navega toda la UI** (biblioteca, Ajustes, sidebar, remapeo, menú del core), no solo el D-Pad. Edge-triggered: un empuje = un paso (igual que el D-Pad) | ✅ 2026-11-11 |
| NAV-7 | El buscador usa B para borrar; con la consulta vacía B sale. X acepta y sale. Falta probar en hardware | 🔧 En curso (Codex, rama main) |

## 6. Savestates

| ID | Pendiente | Estado |
|---|---|---|
| SAVE-1 | Cuatro slots de savestate por juego con miniatura PNG del frame; selector en menú del core. Falta probar en hardware | 🔧 En curso (Codex, rama main) |
| SAVE-2 | SRAM cargada tras abrir ROM y escrita al guardar estado, salir del juego y cerrar app; escritura temporal + rename. Falta probar persistencia en hardware | 🔧 En curso (Codex, rama main) |

## 7. Assets (descarga)

| ID | Pendiente | Estado |
|---|---|---|
| AS-1 | Descargas manuales separadas de portadas y backdrops en Assets; fallos por recurso quedan en `retrou_rpx.log` y el reemplazo forzado conserva el archivo anterior si falla. Falta probar en hardware | 🔧 En curso (Codex, rama main) |
| AS-2 | Metadata se actualiza al iniciar aun con auto-descarga apagada. Falta probar red en hardware | 🔧 En curso (Codex, rama main) |
| AS-3 | Modo «Solo faltantes» / «Descargar todo» para descarga manual; auto-descarga usa solo faltantes. El contador debe incluir únicamente imágenes pendientes al reanudar, sin contar metadata ni archivos existentes. Falta probar en hardware | 🔧 En curso (Codex, rama main) |
| AS-4 | Barra y contador de progreso de descarga en Assets. Falta probar en hardware | 🔧 En curso (Codex, rama main) |
| AS-5 | Actualizar las portadas visibles en Cards, Lista y Carrusel al terminar una descarga, sin reiniciar la app. Compilado; falta probar en hardware | 🔧 En curso (Codex, rama main) |

## 8. Documentación / cierre

Guía de la ronda en Wii U: `docs/hardware-validation.md`.

| ID | Pendiente | Estado |
|---|---|---|
| DOC-1 | Actualizar `README.md` y `native/README.md` con la arquitectura WUHB actual, controles `v2`, guardados, Assets y logs; redactado, pendiente de cierre con pruebas de hardware | 🔧 En curso (Codex, rama main) |
| DOC-2 | Validación final en hardware de todo el conjunto (ronda de pruebas completa con los 4 WUHB) | ⬜ |

## 9. N64 (RetroU N64)

| ID | Pendiente | Estado |
|---|---|---|
| N64-1 | Probar `RetroN64U.wuhb` en hardware (generada desde Our64, rebrandeada) | ⬜ |
| N64-2 | Port completo a la UI RetroU (varias rondas; clon externo en `C:/proyectos/Our64`) | ⬜ |

## 10. Ejemplos gráficos (referencias Our64)

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
