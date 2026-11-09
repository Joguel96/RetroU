# Arquitectura y ajustes de RetroU

## Modelo de ejecución

Objetivo: RetroU permanece abierto y ejecuta juegos dentro, como frontend
Libretro. No es un lanzador de apps `.rpx`.

```text
RetroU (frontend, WUT + SDL2/GX2)
  └── core .rpl cargado en proceso
        ├── retro_init / retro_load_game
        ├── retro_run
        ├── video_refresh  -> textura GX2/SDL
        ├── audio_sample_batch -> cola AX/SDL
        └── input_state <- GamePad/Pro Controller
```

Los cores `.rpx` del buildbot **no sirven** para esto: son RetroArch + core
completos, apps standalone. Los cores de RetroU deben compilarse como `.rpl`
con exports `retro_*` (`wut_create_rpl` + `wut_add_exports`).

### Por qué RPL

- Un solo proceso, cambio de core sin cerrar RetroU.
- Se carga un core a la vez y se descarga al cambiar: menos RAM que multi-core
  estático.
- El coste por frame es el mismo que un core estático; el RPL solo afecta al
  tiempo de carga.
- Permite empaquetar cores por separado y actualizarlos sin recompilar RetroU.

### Condición a validar

Aroma no carga un `.rpl` suelto de la SD con `OSDynLoad_Acquire`; solo resuelve
módulos Aroma ya cargados o RPLs del sistema. Para cores bajo demanda hay que
usar un loader RPL propio (descomprimir secciones, mapear, relocalizar y
resolver imports) o empaquetar el core como módulo Aroma `.wms`.

- **Fallback A:** un core estático integrado en RetroU (como RetroArch Wii U).
  RetroU se queda abierto, pero hay un core por build.
- **Fallback B:** varios cores estáticos con símbolos separados. Más RAM,
  binario grande, pero sin loader dinámico.
- **Híbrido recomendado:** un core integrado de reserva (`fceumm`) y RPL para
  el resto cuando el loader esté validado.

## Ajustes previstos

La UI React (`web/`) ya define la estética. La app nativa tendrá una sección
`Ajustes` con estas familias:

### Vídeo

- Modo de escala: entero, ajuste a pantalla, 4:3, 16:9, core.
- Widescreen:
  - **forzar 16:9** = estirar el frame (no es widescreen real);
  - **widescreen del core** = exponer la opción Libretro del core si existe;
  - **hack por juego** = parche/cheat específico, nunca global.
- Filtro de escalado: nearest, bilinear, sharp-bilinear.
- TV y GamePad independientes: resolución, overscan, rotación, posición.
- VSync, frame delay, frame dup, límite de FPS.
- Shaders/slang si el backend lo permite.

### Memoria y rendimiento

- Core activo y consumo estimado.
- Descargar core al volver al menú (liberar RAM).
- Número de buffers de vídeo (2/3) y tamaño de la textura.
- Caché de texturas/carátulas y límite.
- Métricas de MEM1/MEM2/VRAM (solo lectura) y aviso de memoria baja.
- Precarga de cores frente a carga bajo demanda.

### Audio

- Frecuencia (48 kHz por defecto), latencia/buffer, sincronización audio-vídeo.
- Silenciar en menú, volumen, DSP.

### Controles

- Mapeo GamePad/Pro Controller -> RetroPad.
- Mapeo por core y por juego.
- Deadzone, turbo, hotkeys (guardar, cargar, menú, salir).
- Perfiles y reset.

### Guardado

- SRAM automática por juego.
- Savestates: slots, autoguardado, compresión.
- Ruta `sd:/retrou/saves` y `sd:/retrou/states`.
- Sobrescritura por core/juego.

### Opciones de core

- Variables Libretro (`retro_variable`) expuestas en UI.
- Overrides por core y por juego, persistidos en JSON/CFG.
- Reset a valores del core.

## Persistencia propuesta

```text
sd:/retrou/config/retrou.cfg          ajustes globales
sd:/retrou/config/cores/<core>.json   overrides de core
sd:/retrou/config/games/<game>.json   overrides de juego
sd:/retrou/saves/<core>/<game>.srm
sd:/retrou/states/<core>/<game>.slotN
```

## Fases

1. Spike: compilar `fceumm` como `.rpl`, cargarlo con el loader propio y dibujar
   un frame. Estado: `test.rpl` y `fceumm.rpl` compilan con exports `retro_*`;
   el loader ya descomprime y relocaliza `test.rpl`. Falta probarlo en hardware.
2. Frontend mínimo: vídeo, audio, input, SRAM, savestate.
3. UI nativa de catálogo y ajustes básicos.
4. Memoria, escalado, widescreen y overrides.
5. Carátulas y descarga desde `thumbnails.libretro.com`.
6. Pruebas de frame pacing, latencia de audio e input en hardware.
