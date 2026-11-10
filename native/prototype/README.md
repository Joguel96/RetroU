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

El orden “favoritos primero” no está implementado todavía; queda para 5.1 como
opción de configuración.

## Tema personalizable y fondos (5.1)

El diseño futuro sería:

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
    BackgroundType type;  // Color, Image, Dynamic
    bool blur;
    float blurStrength;   // 0.5
    int dynamicDelayMs;   // 5000
};
```

- `panel` + `panelAlpha` controlan el recuadro translúcido.
- `focus` controla el borde de la card enfocada.
- `backgroundTop/Bottom` controlan el fondo.
- `backgroundImage` permite una imagen propia.
- `blur` + `blurStrength` permiten blur estático una sola vez.
- `Dynamic` cambia al backdrop del juego tras 5 s de foco estable.

El blur no se puede hacer con `backdrop-filter` en SDL2. Se haría **una sola vez**
al cargar la imagen o al cambiar de tema:

1. cargar la imagen con `SDL2_image`;
2. reducirla a 1/4 o 1/8;
3. aplicar un box blur o un blur separable;
4. volver a escalarla;
5. guardar la textura resultante en `AssetCache`.

Sería un blur de fondo, no un blur dinámico detrás de cada panel. Hacerlo por
frame en Wii U sería caro e innecesario para una imagen estática.

`theme.json` sería compartido entre las apps de consola; cada app podría tener
overrides locales en `config/<consola>/`.

## Carátulas desde Libretro (5.1)

La fuente es la librería de thumbnails de Libretro:

- Repositorio: https://github.com/libretro-thumbnails/libretro-thumbnails
- Servidor: https://thumbnails.libretro.com/

Ruta típica:

```text
https://thumbnails.libretro.com/
  Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/
  Named_Boxarts/
  Game%20Name.png
```

Para 5.1 el plan es:

1. resolver el nombre del juego contra `Named_Boxarts`;
2. descargar a `sd:/retrou/thumbnails/<consola>/`;
3. cachear la textura;
4. usar `Named_Snaps` como posible backdrop dinámico;
5. permitir fallback al degradado si no existe imagen.

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
  SDL2.lib SDL2_ttf.lib SDL2_image.lib
```

Copiar junto al `.exe`:

```text
SDL2.dll
SDL2_ttf.dll
SDL2_image.dll
assets/icon.png
content/ui.ttf
data/games.json
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

5.1: tema configurable, fondos estáticos/dinámicos, blur opcional y descarga de
carátulas desde Libretro.
