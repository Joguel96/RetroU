# Renombrado de ROMs en la SD (2026-11-20)

Se identificaron ROMs por CRC en los catálogos Libretro y No-Intro. Se
renombraron 262 archivos en `E:/retrou/roms` y se migraron 225 archivos
asociados: guardados, portadas, fondos y un mapa por ROM. También se actualizó
una ruta de `favorites.conf`. Los conteos de archivos por consola no cambiaron.
Ochenta ROMs sin CRC reconocido y diez destinos duplicados o ya existentes se
dejaron como estaban; el manifiesto principal registra esas exclusiones.

El registro de cada movimiento está en `E:/retrou/rom-rename.log`. Los
manifiestos con origen, destino, CRC y contenido anterior de Favoritos están
en `docs/rom-rename-20261120.json` y `docs/rom-rename-extra-20261120.json`,
con copias en `E:/retrou/rom-rename-manifest-20261120.json` y
`E:/retrou/rom-rename-manifest-extra-20261120.json`. Los logs de la auditoría
CRC están en `native/build/logs/` y no se incluyen en Git.

Para revertir los nombres en la SD, desde la raíz del proyecto y con la SD
montada como `E:`, ejecutar **en este orden**:

```powershell
python tools/rename_roms_libretro.py rollback --manifest docs/rom-rename-extra-20261120.json
python tools/rename_roms_libretro.py rollback --manifest docs/rom-rename-20261120.json
```

La herramienta comprueba que los destinos y Favoritos conservan el estado
registrado antes de mover archivos. El log inverso se guarda en
`E:/retrou/rom-rename-rollback.log`. Revertir los commits de Git por sí solo
no devuelve los nombres en la SD.

Pendiente: abrir las cuatro bibliotecas en Wii U y comprobar portadas,
Favoritos y guardados de juegos renombrados (ROM-1).
