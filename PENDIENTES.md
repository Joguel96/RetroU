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
| CTRL-7 | Configurar por tipo de mando la combinación global que abre el menú durante el core; función histórica retirada por NAV-8 a petición del usuario | ✅ 2026-11-12; sustituida 2026-11-21 |
| CTRL-8 | Al desconectar y reconectar Wii U Pro Controller o Classic Controller durante la app, conservar o restaurar su asignación a Mando 1/Mando 2 y su mapeo; comprobar también la navegación de UI tras reconectar | ✅ 2026-11-13 |
| CTRL-9 | GBA: al elegir Wii U Pro Controller como Mando 1, una ROM con mapa propio debe conservar ese mando durante el juego; el archivo por ROM solo modifica botones, no el orden global | ✅ 2026-11-13 |
| CTRL-10 | Presentar la asignación de mandos como tarjetas por puerto con «Pulsa A para conectar», selección visible y acciones de mapeo y orden, según `ejemplos/organizacion de controles para retrou.png`. El usuario confirmó en Wii U que la asignación y el orden de mandos ya funcionan correctamente. | ✅ 2026-11-20 |
| CTRL-12 | Navegación y orden como N64, con dos puertos NES/SNES y uno GBA. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |
| CTRL-13 | Direcciones de sticks y etiquetas corregidas. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |
| CTRL-11 | N64: mapa del mando seleccionado visible en Controles del menú rápido. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |

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
| UI-11 | En Vídeo, elegir salida TV, GamePad o ambas para NES/SNES/GBA; guardar por consola. El usuario confirmó los modos corregidos en Wii U. | ✅ 2026-11-21 |
| UI-13 | N64: portadas horizontales completas en la vista Lista, sin recorte vertical. | ✅ 2026-11-23 |
| UI-12 | Transiciones al abrir y cerrar ROM sin corrupción visual. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |
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
| NAV-9 | Lista/carrusel: cambiar letra reinicia selección y desplazamiento al primer juego, con L/R y touch. | ✅ 2026-11-23 |
| NAV-10 | Touch en biblioteca: primer tap selecciona y otro tap sobre la selección ejecuta; deslizar navega sin lanzar ROMs. | ✅ 2026-11-23 |
| NAV-11 | Lista en las cuatro apps: izquierda/derecha del D-pad o stick L desplaza cinco juegos conservando la fila seleccionada; avance limitado en los extremos. Arriba/abajo sigue moviendo un juego. | ✅ 2026-11-23 |
| NAV-8 | HOME abre el menú RetroU durante la ROM y el menú del sistema en biblioteca/ajustes. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |

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
| ROM-1 | Se renombraron 262 ROMs identificadas por CRC en `E:/retrou/roms` y se migraron 225 archivos asociados; 10 colisiones y 80 ROMs sin CRC conocido quedaron intactas. Los manifiestos en `docs/` y `E:/retrou/` permiten revertir. El usuario confirmó el resultado en Wii U. | ✅ 2026-11-21 |
| ROM-2 | Se retiraron 29 ROMs duplicadas de la SD: 13 versiones europeas con alternativa USA y 16 copias de contenido idéntico. Las copias y el manifiesto de restauración están en `native/build/rom-dedupe-20261121/`. El usuario confirmó el resultado en Wii U. | ✅ 2026-11-21 |
| AS-7 | Validar la firma PNG antes de considerar una portada o fondo como descargado. Se sustituyó el archivo inválido de Donkey Kong 64; el usuario confirmó el resultado en Wii U. | ✅ 2026-11-21 |
| AS-8 | N64: priorizar la edición normal y probar alternativas si Libretro entrega texto con extensión `.png`. El usuario confirmó las descargas en Wii U; los fallos restantes son principalmente hacks, demos y ROMs de prueba. | ✅ 2026-11-21 |

## 8. Documentación / cierre

Guía de la ronda en Wii U: `docs/hardware-validation.md`.

| ID | Pendiente | Estado |
|---|---|---|
| DOC-1 | Actualizar `README.md` y `native/README.md` con la arquitectura WUHB actual, controles `v2`, guardados, Assets y logs; funciones documentadas confirmadas en hardware, nuevas correcciones identificadas como pendientes | ✅ 2026-11-12 |
| DOC-2 | Validación final en hardware de todo el conjunto: el usuario confirmó la ronda completa de las cuatro WUHB. | ✅ 2026-11-23 |
| DOC-3 | Retirados el prototipo web, dependencias/build y referencias de uso. Guía actualizada para la UI nativa, crédito visual conservado y cuatro targets compilados. Sin cambios ejecutables ni prueba adicional de hardware aplicable. | Hecho 2026-11-23 (verificación local) |
| REL-1 | Release v0.1.0 publicado en `https://github.com/xoloDev-games/RetroU/releases/tag/v0.1.0`, con IA, créditos/licencias, cuatro WUHB, cuatro RPX y fuentes. Transferencia aceptada por el usuario y propiedad de `xoloDev-games` verificada en GitHub. | ✅ 2026-11-23 (publicación y propiedad verificadas) |
| REL-2 | Los cuatro RPX mediante Homebrew Launcher en Tiramisu: arranque, ROM, controles, audio, HOME, guardados y cierre confirmados por el usuario en Wii U. | ✅ 2026-11-23 |
| REL-3 | Botón Sponsor publicado mediante `.github/FUNDING.yml`, enlazado al perfil GitHub Sponsors de Joguel96 y verificado con `fundingLinks` en GitHub. | Hecho 2026-11-23 (verificación en GitHub) |
| CORE-1 | Futura ampliación a todos los cores Libretro para Wii U; inventario en `docs/libretro-wiiu-roadmap.md`. Revisar licencias, compilar desde fuente e integrar y probar cada motor. | ⬜ |
| CORE-2 | Empaquetar los motores alternativos de cada sistema en su RetroU, con selector de core y un solo motor activo; aislar símbolos Libretro y separar savestates por motor. Incluye alternativas para NES/SNES/GBA. | ⬜ |

## 9. N64 (RetroU N64)

| ID | Pendiente | Estado |
|---|---|---|
| N64-1 | Probar `RetroN64U.wuhb` en hardware (generada desde Our64, rebrandeada); el usuario confirmó que es funcional | ✅ 2026-11-12 |
| N64-2 | Integración del motor N64 en el frontend RetroU confirmada por el usuario. La reapertura de ROM se sigue por separado en N64-14. | ✅ 2026-11-23 |
| N64-3 | Comparar en Wii U física la pantalla de configuración de N64 Dev con la de RetroU NES/SNES/GBA: secciones, opciones y navegación. El usuario confirmó el resultado en hardware. | ✅ 2026-11-20 |
| N64-4 | En el buscador N64, B borra un carácter y solo sale si la consulta está vacía; X acepta y cierra. El usuario confirmó el resultado en hardware. | ✅ 2026-11-20 |
| N64-5 | Usar `retrou_assets` en el frontend N64 de RetroU, con descarga automática y progreso como las otras consolas. El usuario confirmó la descarga de assets en Wii U; `retrou_rpx.log.3` registra portadas y fondos descargados de N64. | ✅ 2026-11-20 |
| N64-6 | Integrar en Controles RetroU el flujo de «Pulsa A para conectar» por puerto. N64 Dev asigna directamente el mando nuevo al siguiente puerto libre con A; el usuario confirmó este flujo y el orden de mandos en Wii U. NES/SNES/GBA conservan su flujo anterior. | ✅ 2026-11-20 |
| N64-7 | Frontend RetroU con Not64 + GLideN64, sin interfaz Our64. Confirmado por el usuario; nueva regresión de segunda ROM en N64-14. | ✅ 2026-11-23 |
| N64-8 | Igualar las portadas laterales del carrusel N64 al carrusel SNES: área horizontal de 270×260 y ajuste completo de la imagen. El usuario confirmó el resultado en Wii U con carátulas descargadas. | ✅ 2026-11-20 |
| N64-9 | Controles N64: cuatro tarjetas, A asigna en la pantalla principal, izquierda/derecha mueve el foco y el jugador seleccionado permanece con borde blanco. El usuario confirmó en Wii U las tarjetas, los marcadores y «Cambiar orden» tras corregir las pulsaciones repetidas y el cierre al asignar todos los mandos conectados. | ✅ 2026-11-20 |
| N64-10 | Abrir y navegar el menú rápido de RetroU desde una ROM N64. El usuario confirmó la navegación en Wii U; la combinación usada entonces fue sustituida por HOME en NAV-8. | ✅ 2026-11-21; apertura con HOME pendiente en NAV-8 |
| N64-11 | HOME y cierre de aplicación tras una ROM N64 funcionan. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |
| N64-12 | Guardar/cargar los cuatro estados N64 y mostrar su miniatura. El usuario confirmó el guardado y la miniatura en Wii U. | ✅ 2026-11-21 |
| N64-13 | El juego recibe botones y sticks desde el arranque y al volver del menú. Confirmado por el usuario en Wii U. | ✅ 2026-11-22 |
| N64-14 | Abrir/cerrar la misma ROM y otra distinta en la misma sesión, conservando sincronización RSP/vídeo. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |

## 10. Audio del core

| ID | Pendiente | Estado |
|---|---|---|
| AUD-1 | Conversión lineal de 65 536 Hz a la salida de 32 000 Hz para el core GBA | ❌ 2026-11-13: probada en Wii U; el ruido persistió |
| AUD-2 | Corregir el ruido de GBA siguiendo la ruta de RetroArch: salida Wii U a 48 000 Hz y remuestreo con filtro sinc antes de encolar; el usuario confirmó que el audio funciona en Wii U. | ✅ 2026-11-20 |
| AUD-4 | Audio GBA sin distorsión, también en UI. El usuario confirmó aproximadamente 30 minutos de juego sin errores. | ✅ 2026-11-23 |
| APP-2 | Salida de GBA al menú Wii U. Confirmada por el usuario en hardware tras corregir la pausa de audio. | ✅ 2026-11-23 |
| APP-3 | Arranque N64 sin bloqueo anterior al splash. Confirmado por el usuario en Wii U. | ✅ 2026-11-23 |
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
