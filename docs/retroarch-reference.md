# RetroArch como referencia para RetroU

Este documento resume lo que se conserva conceptualmente del fork de RetroArch
que se usó como prototipo. El árbol de RetroArch **no forma parte del proyecto**
a partir de este punto; estas notas son la referencia para la app nativa.

Base consultada: `libretro/RetroArch` commit `fbe4d9a` (Wii U, Aroma).

## 1. Ejecución de un core `.rpx`

RetroArch para Wii U no carga cores como bibliotecas dinámicas: cada core es un
`.rpx` completo (RetroArch + core enlazado). Hay dos caminos.

### 1.1 Cargar el `.rpx` en memoria (HBL)

Archivos de referencia en RetroArch:

- `os/wiiu/hbl.c` / `os/wiiu/hbl.h`
- `frontend/drivers/platform_wiiu.c` (`frontend_wiiu_exec`)

El protocolo de argumentos es:

```c
#define MEM_BASE     0x00800000
#define ARGV_PTR     (*(void* volatile *)(MEM_BASE + 0x1300 + 0x80))
#define ARGV_MAGIC   (((u32)'_' << 24) | ((u32)'a' << 16) | ((u32)'r' << 8) | (u32)'g')

struct argv_block {
   u32   magic;          /* ARGV_MAGIC */
   u32   argc;
   char *argv[N + 1];
   char  args[];         /* argv[0] = core .rpx, argv[1] = ROM */
};
```

`getApplicationEndAddr()` devuelve el final del RPX actual; ahí se construye el
bloque. Después se llama:

```c
HBL_loadToMemory(core_path, args_size);
ARGV_PTR = param;
```

`HBL_loadToMemory` copia el `.rpx` a memoria con `SC0x25_KernelCopyData`, una
llamada que **WUT no exporta**. RetroArch la resuelve con imports propios. Si se
usa este camino, hay que replicar ese import/stub y el mapa de memoria de HBL.
Además, el `.rpx` cargado debe leer `ARGV_PTR` al arrancar (RetroArch lo hace).

Ventaja: permite pasar `core.rpx` + `rom` en `argv`.
Riesgo: depende del entorno HBL y de una API no pública; es frágil en Aroma/WUHB.

### 1.2 Lanzar el `.rpx` como título (Aroma)

Para una app Aroma es más limpio usar `sysapp`:

```c
/* sysapp.rpl, OSDynLoad */
int _SYSLaunchTitleByPathFromLauncher(const char *path, int len, int zero);
```

RetroArch la usa en algunos flujos; en WUT la declaración pública puede tener
otra firma, así que hay que resolverla con `OSDynLoad_Acquire`/`OSDynLoad_FindExport`
y comprobar la firma instalada.

Esta API **no acepta argv**. Consecuencias:

- Se puede lanzar el core `.rpx` standalone; el core abre su propio menú.
- Para lanzar core + ROM hay que usar el camino HBL/argv, o un core parcheado que
  lea la ROM de un archivo de lanzamiento (por ejemplo
  `sd:/retrou/launch/last.txt`) antes de mostrar el menú.

**Decisión para RetroU:** el frontend propio no debe intentar enlazar cores
estáticamente. Debe descubrir `.rpx` en `sd:/retrou/cores`, lanzarlos con sysapp
y, si se necesita contenido directo, implementar el camino HBL/argv de forma
aislada y documentada.

### 1.3 Layout de cores

```text
sd:/retrou/cores/*.rpx
sd:/retrou/cores/info/*.info
```

Los `.info` son opcionales para el lanzador propio, pero permiten mostrar nombre,
sistema y extensiones sin abrir el RPX. Si no existen, se puede inferir el core
por el nombre (`snes9x`, `fceumm`, `mgba`, ...).

## 2. Carátulas

RetroArch descarga desde el servidor público de thumbnails:

```text
http://thumbnails.libretro.com/<Sistema>/Named_Boxarts/<Imagen>.png
http://thumbnails.libretro.com/<Sistema>/Named_Titles/<Imagen>.png
http://thumbnails.libretro.com/<Sistema>/Named_Snaps/<Imagen>.png
```

`<Sistema>` es el nombre del sistema de la base de datos libretro, por ejemplo:

```text
Nintendo - Super Nintendo Entertainment System
Nintendo - Nintendo Entertainment System
Nintendo - Game Boy Advance
Sony - PlayStation
Sega - Mega Drive - Genesis
Nintendo - Nintendo 64
```

`<Imagen>` es el título sin extensión, normalizado: se eliminan/reemplazan
caracteres no válidos (`:`, `/`, `\`, `?`, `*`, `"`, `<`, `>`, `|`), se recortan
espacios y se codifica la URL. RetroArch usa la misma ruta para guardar en local:

```text
sd:/retrou/thumbnails/<Sistema>/Named_Boxarts/<Imagen>.png
```

No requiere TLS: el servidor de thumbnails es HTTP. Si se usa libcurl en la app
nativa, compilar con `wiiu-curl` o usar sockets propios; para una primera versión
basta descargar el PNG y escribirlo en la ruta local.

## 3. ProcUI (HOME y cierre)

Patrón mínimo verificado con `libwhb` (`wut/libraries/libwhb/src/proc.c`):

```c
ProcUIInit(&saveCallback); /* saveCallback llama OSSavesDone_ReadyToRelease */

for (;;) {
   ProcUIStatus status = ProcUIProcessMessages(TRUE);

   if (status == PROCUI_STATUS_EXITING)
      break;
   if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
      ProcUIDrawDoneRelease();
      continue;
   }
   if (status != PROCUI_STATUS_IN_FOREGROUND)
      continue;

   /* dibujar un frame / ejecutar lógica */
}

ProcUIShutdown();
```

Imports necesarios en un RPX que no enlaza `libwut` directamente:

```c
IMPORT(ProcUIInit);
IMPORT(ProcUIProcessMessages);
IMPORT(ProcUIDrawDoneRelease);
IMPORT(ProcUIShutdown);
```

En la app nueva se puede usar `WHBProcInit`/`WHBProcIsRunning` de `libwut` o el
pump explícito. Lo importante es no dejar el bucle sin `ProcUIProcessMessages`.

## 4. Empaquetado WUHB

```sh
wuhbtool app.rpx RetroU.wuhb \
  --name=RetroU --short-name=RetroU --author=joguel96 \
  --icon=assets/icon.png \
  --tv-image=assets/tv-splash.png \
  --drc-image=assets/drc-splash.png
```

- `icon.png`: 128x128.
- `tv-splash.png`: 1280x720.
- `drc-splash.png`: 854x480.
- WUHB no tiene campos de versión ni descripción en `meta.ini`; Aroma muestra
  nombre, autor e icono.
- El `.rpx` dentro del WUHB se ejecuta como título, no como app HBL.

## 5. Qué NO reutilizar

- El árbol completo de RetroArch, sus menús Ozone/XMB/MaterialUI y su sistema de
  configuración: son enormes y arrastran el port Wii U.
- El parser del core updater: espera líneas `fecha crc archivo`, mientras el
  buildbot devuelve HTML h5ai; por eso fallaba en “todo”.
- La extracción de assets/cores del updater de RetroArch en Wii U.
- Los Makefiles multi-plataforma y el enlazado estático de cores.

## 6. Referencias

- https://github.com/devkitPro/wiiu-examples
- https://github.com/devkitPro/wut
- https://github.com/devkitPro/sdl
- https://github.com/libretro/RetroArch (`frontend/drivers/platform_wiiu.c`,
  `os/wiiu/hbl.c`, `tasks/task_pl_thumbnail_download.c`)
