# Hoja de ruta: todos los cores Libretro para Wii U

Inventario consultado el **2026-11-23** en el [buildbot oficial](https://buildbot.libretro.com/nightly/nintendo/wiiu/latest/).

El índice contiene **130 cores/variantes**: tres ya integrados en RetroU
(FCEUmm, Snes9x y mGBA) y **127 pendientes**. No son necesariamente
sistemas distintos: incluye variantes, motores de juegos y cores antiguos.
N64 usa Not64 + GLideN64 procedentes de Our64 y queda fuera de este conteo.

## Una aplicación por sistema, con selector de core

CORE-2: los motores alternativos se compilarán y empaquetarán dentro del RetroU
del sistema correspondiente, con una opción para cambiar de core. Se mantendrá
un solo motor activo: cerrar y liberar el anterior antes de iniciar otro.
Habrá que aislar los símbolos `retro_*` de las bibliotecas estáticas, definir
las capacidades de cada motor y separar sus estados; no se debe cargar un
savestate de un core en otro. La compatibilidad de SRAM también se revisará.

Por ejemplo, RetroU NES agrupará FCEUmm/fixNES/Nestopia/QuickNES; RetroU SNES,
las variantes Snes9x; RetroU GBA, mGBA/gpSP/Ardens/VBA Next/VBA-M, según las
licencias y la viabilidad de cada port. Este release aún contiene un motor
por app. Que un core emule varios sistemas no obliga a mezclarlos en una app.
Se agruparán familias coherentes cuando el motor cubra varios sistemas: por
ejemplo, **RetroU SEGA** para Master System/Game Gear/Mega Drive/Sega CD/32X,
con Genesis Plus GX y PicoDrive como alternativas y capacidades visibles.
El usuario autorizó agrupar estos sistemas dentro de una app; no se requiere
una aplicación distinta para cada variante de hardware. Las 57 agrupaciones
iniciales se consolidarán al diseñar las familias de aplicaciones.

La agrupación inicial de los metadatos oficiales da **60 grupos principales**,
de los que tres ya tienen app Libretro, dejando **57 grupos pendientes**.
Es un conteo de planificación: incluye computadoras y plataformas virtuales,
excluye juegos/motores de juegos/reproductores y no equivale a 57 consolas.
Los cores multisistema y las familias arcade requieren una clasificación
adicional antes de fijar el número final de aplicaciones. Véase el
[desglose por metadatos](libretro-wiiu-systems.md).

Objetivo futuro CORE-1: integrar todos estos motores en aplicaciones RetroU,
revisando licencia, port Wii U, BIOS/datos, mandos, guardados, audio y rendimiento
de cada uno. Entregar WUHB/RPX y probar en hardware antes de declarar soporte.
Se compilará desde fuente; los RPX del buildbot son RetroArch+core, no bibliotecas
para inyectar en RetroU. La presencia en el índice no garantiza viabilidad del port.

Ejemplos incluidos en el alcance: blueMSX/fMSX (MSX), PCSX ReARMed (PlayStation),
FB Alpha/FBNeo y NeoCD (Neo Geo), además de todos los demás listados.

| Core del buildbot | Estado RetroU |
|---|---|
| `2048` | Futuro |
| `81` | Futuro |
| `a5200` | Futuro |
| `amiarcadia` | Futuro |
| `anarch` | Futuro |
| `ardens` | Futuro |
| `arduous` | Futuro |
| `atari800` | Futuro |
| `bk` | Futuro |
| `bluemsx` | Futuro |
| `cannonball` | Futuro |
| `cap32` | Futuro |
| `chailove` | Futuro |
| `clownmdemu` | Futuro |
| `crocods` | Futuro |
| `dosbox_svn` | Futuro |
| `DoubleCherryGB` | Futuro |
| `ecwolf` | Futuro |
| `fbalpha` | Futuro |
| `fbalpha2012` | Futuro |
| `fbalpha2012_cps1` | Futuro |
| `fbalpha2012_cps2` | Futuro |
| `fbalpha2012_cps3` | Futuro |
| `fbalpha2012_neogeo` | Futuro |
| `fbneo` | Futuro |
| `fceumm` | Integrado |
| `fixgb` | Futuro |
| `fixnes` | Futuro |
| `fmsx` | Futuro |
| `freechaf` | Futuro |
| `freeintv` | Futuro |
| `frodo` | Futuro |
| `fuse` | Futuro |
| `gam4980` | Futuro |
| `gambatte` | Futuro |
| `gearboy` | Futuro |
| `gearcoleco` | Futuro |
| `geargrafx` | Futuro |
| `gearlynx` | Futuro |
| `gearsystem` | Futuro |
| `geartowns` | Futuro |
| `genesis_plus_gx` | Futuro |
| `genesis_plus_gx_wide` | Futuro |
| `gme` | Futuro |
| `gong` | Futuro |
| `gpsp` | Futuro |
| `gw` | Futuro |
| `handy` | Futuro |
| `hatari` | Futuro |
| `hatari2014` | Futuro |
| `hatarib` | Futuro |
| `jaxe` | Futuro |
| `jumpnbump` | Futuro |
| `lowresnx` | Futuro |
| `lutro` | Futuro |
| `mame2000` | Futuro |
| `mame2003` | Futuro |
| `mame2003_midway` | Futuro |
| `mame2003_plus` | Futuro |
| `mednafen_lynx` | Futuro |
| `mednafen_ngp` | Futuro |
| `mednafen_pce` | Futuro |
| `mednafen_pce_fast` | Futuro |
| `mednafen_supergrafx` | Futuro |
| `mednafen_vb` | Futuro |
| `mednafen_wswan` | Futuro |
| `mgba` | Integrado |
| `minivmac` | Futuro |
| `mojozork` | Futuro |
| `mrboom` | Futuro |
| `mu` | Futuro |
| `nekop2` | Futuro |
| `neocd` | Futuro |
| `nestopia` | Futuro |
| `np2kai` | Futuro |
| `numero` | Futuro |
| `nxengine` | Futuro |
| `o2em` | Futuro |
| `oberon` | Futuro |
| `pcsx_rearmed` | Futuro |
| `picodrive` | Futuro |
| `pocketcdg` | Futuro |
| `pokemini` | Futuro |
| `potator` | Futuro |
| `prboom` | Futuro |
| `prosystem` | Futuro |
| `puae` | Futuro |
| `puae2021` | Futuro |
| `quasi88` | Futuro |
| `quicknes` | Futuro |
| `race` | Futuro |
| `reminiscence` | Futuro |
| `retro8` | Futuro |
| `sameboy` | Futuro |
| `scummvm` | Futuro |
| `snes9x` | Integrado |
| `snes9x2002` | Futuro |
| `snes9x2005` | Futuro |
| `snes9x2005_plus` | Futuro |
| `snes9x2010` | Futuro |
| `squirreljme` | Futuro |
| `stella2014` | Futuro |
| `superbroswar` | Futuro |
| `tgbdual` | Futuro |
| `theodore` | Futuro |
| `tia` | Futuro |
| `tic80` | Futuro |
| `tyrquake` | Futuro |
| `uw8` | Futuro |
| `uzem` | Futuro |
| `vaporspec` | Futuro |
| `vba_next` | Futuro |
| `vbam` | Futuro |
| `vecx` | Futuro |
| `vice_x128` | Futuro |
| `vice_x64` | Futuro |
| `vice_x64sc` | Futuro |
| `vice_xcbm2` | Futuro |
| `vice_xcbm5x0` | Futuro |
| `vice_xpet` | Futuro |
| `vice_xplus4` | Futuro |
| `vice_xscpu64` | Futuro |
| `vice_xvic` | Futuro |
| `vitaquake2` | Futuro |
| `vitaquake2-rogue` | Futuro |
| `vitaquake2-xatrix` | Futuro |
| `vitaquake2-zaero` | Futuro |
| `wasm4` | Futuro |
| `x1` | Futuro |
| `xrick` | Futuro |

El catálogo puede cambiar. Actualizar este inventario antes de comenzar cada ronda;
no prometer una fecha ni distribuir un core antes de resolver sus condiciones.
