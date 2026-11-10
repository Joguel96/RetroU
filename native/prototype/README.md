# RetroU grid prototype (PC)

Prototipo aislado de la biblioteca. No forma parte de las WUHB y no modifica el
build de Wii U.

## Qué demuestra

- Ventana SDL2 de 1280×720.
- Grilla de **5 columnas** (TV y GamePad comparten la misma lógica).
- 14 tarjetas cargadas desde `data/games.json`.
- Foco único, navegación con teclado, D-pad o clic/toque.
- Scroll automático y virtualización de filas.
- `X` alterna favorito y lo persiste en `games.json`.
- `Y` filtra solo favoritos.
- El orden por defecto es **ABC**; marcar un favorito no reordena la grilla.
- Tarjeta de 190×250 con degradado prerenderizado, esquinas redondeadas,
  estrella, badge `Destacado`, título, desarrollador/año y género.
- Borde de foco azul.
- Texto cacheado por tamaño/color/string.

La tarjeta no muestra chip de plataforma, divisor interior ni contador de
ejecuciones: la app es de una sola consola.

`SDL_FINGERDOWN` está conectado al mismo hit-test que el ratón. En PC sin
pantalla táctil se prueba con clic; en Wii U se usará el toque real del GamePad.

## Favoritos y filtros

El JSON guarda `favorite` por juego. Al pulsar `X`:

1. se cambia el valor;
2. se reescribe `data/games.json`;
3. se reconstruye la lista visible;
4. el orden ABC se mantiene.

Con `Y` se muestra solo la lista de favoritos. Si no hay ninguno, aparece el
estado vacío.

El orden “favoritos primero” está disponible con `O`, pero el valor inicial
viene de `data/theme.json`:

```json
"sort": { "favoritesFirst": false }
```

Con `favoritesFirst=false` se mantiene ABC. Con `true` se pone primero el grupo
de favoritos y luego ABC.

## Tema y fondos (5.1)

El tema se carga desde:

```text
native/prototype/data/theme.json
```

```cpp
struct Theme {
    Color backgroundTop;
    Color backgroundBottom;
    Color panel;
    uint8_t panelAlpha;
    Color focus;
    Color text;
    Color muted;
    Color accent;

    std::string backgroundImage;
    std::string backgroundType; // color | image | dynamic
    bool blur;
    float blurStrength;         // 0.5
    int dynamicDelayMs;         // 5000
    bool favoritesFirst;
};
```

Teclas del prototipo:

- `B`: activa/desactiva el blur.
- `T`: cambia `color → image → dynamic`.
- `O`: alterna favoritos primero.
- `D`: descarga carátulas de Libretro.

El blur se calcula una vez:

1. cargar la imagen con `SDL2_image`;
2. reducirla a 640×360;
3. box blur separable con radio `blurStrength * 12`;
4. crear textura y cachearla.

No se aplica blur por frame.

Con `backgroundType: "dynamic"`:

- el foco inicia un temporizador de 5 s;
- si el foco no cambió, se usa `backdropPath` del juego;
- si no existe, se usa `coverPath` o el fondo del tema;
- hay crossfade de 500 ms.

`data/backdrops/` contiene imágenes de prueba para demostrar el cambio dinámico.

## Carátulas desde Libretro

Fuente:

- Repositorio: https://github.com/libretro-thumbnails/libretro-thumbnails
- Servidor: https://thumbnails.libretro.com/

Ruta usada por el prototipo:

```text
https://thumbnails.libretro.com/
  Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/
  Named_Boxarts/
  Game%20Name.png
```

`D` intenta el título solo y variantes ` (USA)`, ` (Europe)`, ` (Japan)` y
` (World)`, y guarda en:

```text
data/thumbnails/<id>.png
```

La descarga usa `URLDownloadToFileA` en Windows. En Wii U se sustituirá por el
cliente HTTP correspondiente y la ruta será:

```text
sd:/retrou/thumbnails/<consola>/
```

## Compilar en Windows con MSVC

El prototipo se compiló con MSVC 19.51, SDL2 2.30.10, SDL2_ttf 2.22.0 y
SDL2_image 2.8.2:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cd native\prototype

cl /nologo /std:c++20 /EHsc /MD ^
  /Ithird_party ^
  /Ithird_party\SDL2\SDL2-2.30.10\include ^
  /Ithird_party\SDL2_ttf\SDL2_ttf-2.22.0\include ^
  /Ithird_party\SDL2_image\SDL2_image-2.8.2\include ^
  main.cpp /Fe:card_demo.exe ^
  /link ^
  /LIBPATH:third_party\SDL2\SDL2-2.30.10\lib\x64 ^
  /LIBPATH:third_party\SDL2_ttf\SDL2_ttf-2.22.0\lib\x64 ^
  /LIBPATH:third_party\SDL2_image\SDL2_image-2.8.2\lib\x64 ^
  SDL2.lib SDL2_ttf.lib SDL2_image.lib urlmon.lib
```

Copiar junto al `.exe`:

```text
SDL2.dll
SDL2_ttf.dll
SDL2_image.dll
assets/icon.png
assets/background.png
content/ui.ttf
data/games.json
data/theme.json
data/backdrops/
```

Ejecutar:

```text
card_demo.exe
```

## CMake

`CMakeLists.txt` está preparado para un CMake con generador de Visual Studio o
Ninja. El CMake que viene con devkitPro no expone el generador de Visual Studio,
por eso la compilación local se hizo con `cl` directamente.

En un entorno normal con SDL2 instalado, el objetivo es migrar este prototipo a
`native/src/ui/GameCard` y `native/src/gfx/AssetCache`, compartiendo el código
con el target PC de `native/`.

## Siguiente paso

Integrar el prototipo en `native/src/ui` y `native/src/gfx`, compartiendo el
código con el target PC de `native/`. Después, llevar el mismo tema y la misma
grilla a las apps Wii U.
