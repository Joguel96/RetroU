# RetroU card prototype (PC)

Prototipo aislado de **una sola tarjeta**. No forma parte de las WUHB y no
modifica el build de Wii U.

## Qué demuestra

- Ventana SDL2 de 1280×720.
- Grilla de **5 columnas** (TV y GamePad comparten la misma lógica).
- 14 tarjetas de prueba.
- Foco único, navegación con teclado o D-pad y scroll automático.
- Virtualización: solo se dibujan las filas visibles.
- Clic de ratón como sustituto de toque en el GamePad.
- Tarjeta de 190×250 con degradado prerenderizado, esquinas redondeadas,
  estrella, badge `Destacado`, título, desarrollador/año y género.
- Borde de foco azul.
- Texto cacheado por tamaño/color/string.

La tarjeta no muestra chip de plataforma, divisor interior ni contador de
ejecuciones: la app es de una sola consola.

Todavía no hay scroll suave animado ni eventos `SDL_Finger` reales; el
prototipo usa teclado, gamepad y clic.

## Tema personalizable (futuro)

Sí es posible. El diseño debe separar un `Theme` de los colores hardcodeados:

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
    int backgroundBlur;
};
```

- `panel` + `panelAlpha` controlan el recuadro translúcido.
- `focus` controla el borde de la card enfocada.
- `backgroundTop/Bottom` controlan el fondo.
- `backgroundImage` permite una imagen propia.
- `backgroundBlur` permite desenfocar esa imagen.

El blur no se puede hacer con `backdrop-filter` en SDL2. Se haría **una sola vez**
al cargar la imagen o al cambiar de tema:

1. cargar la imagen con `SDL2_image`;
2. reducirla a 1/4 o 1/8;
3. aplicar un box blur o un blur separable;
4. volver a escalarla;
5. guardar la textura resultante en `AssetCache`.

Sería un blur de fondo, no un blur dinámico detrás de cada panel. Hacerlo por
frame en Wii U sería caro e innecesario para una imagen estática.

`theme.conf` sería compartido entre las apps de consola; cada app podría tener
overrides locales en `config/<consola>/`.

## Compilar en Windows con MSVC

El prototipo se compiló con MSVC 19.51 y SDL2 2.30.10:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
cd native\prototype

cl /nologo /std:c++20 /EHsc /MD ^
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

Convertir esta tarjeta en `GameGrid`: 6 columnas en TV, 4 en GamePad, foco con
D-pad, scroll vertical y virtualización de filas.
