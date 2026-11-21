# ROMs duplicadas retiradas de la SD (2026-11-21)

Se retiraron 29 ROMs de `E:/retrou/roms`: 13 versiones europeas con la misma
obra disponible en USA y 16 copias de contenido idéntico (2 GBA y 14 N64).
En N64 el hash se calculó después de normalizar el orden de bytes de los
formatos `.n64`, `.v64` y `.z64`. Las variantes beta, revisiones y ediciones
con contenido distinto se conservaron. SNES no tenía coincidencias bajo estos
criterios. Los archivos de guardado y assets no se borraron.

Las 29 copias verificadas están en
`native/build/rom-dedupe-20261121/<consola>/`, fuera de la SD. El manifiesto
`native/build/rom-dedupe-20261121/manifest.json` enumera el archivo retirado,
la ROM conservada y el motivo. El respaldo ocupa 275 906 768 bytes.

Para restaurar las 29 ROMs con la SD montada como `E:`:

```powershell
python tools/dedupe_roms.py --restore
```

La herramienta comprueba que las rutas originales estén libres y verifica
los hashes de cada copia. Falta revisar las bibliotecas en Wii U (ROM-2).
