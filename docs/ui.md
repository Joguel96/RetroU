# Interfaz nativa RetroU

La interfaz se ejecuta directamente en Wii U con C++17, WUT y GX2. El frontend
compartido está en `native/src/gx2/gx2_core_test_main.cpp`; imágenes, texto y
biblioteca se implementan en `native/src/gx2/`. Las cuatro apps incorporan la
misma UI y los ajustes específicos de sus motores.

## Navegación

| Control | Acción |
|---|---|
| D-pad / stick izquierdo | mover la selección |
| Izquierda/derecha en Lista | avanzar o retroceder cinco juegos conservando la fila del selector |
| Arriba/abajo en Lista | mover un juego |
| A | activar la opción o ejecutar la ROM seleccionada |
| B | volver al nivel anterior |
| X en biblioteca | marcar o desmarcar favorito |
| Y en biblioteca | cambiar entre Cards, Lista y Carrusel |
| L/R en Lista o Carrusel | cambiar el filtro All/Favoritos/#/A–Z y volver al primer juego |
| Plus en biblioteca | abrir Ajustes |
| Minus en Cards | abrir filtros |
| Minus en Lista o Carrusel | abrir el buscador |
| HOME durante una ROM | pausar el motor y abrir el menú rápido RetroU |
| HOME en biblioteca o Ajustes | abrir el menú HOME de Wii U |

GamePad y mandos externos comparten la navegación. En el buscador, B borra un
carácter y sale cuando la consulta está vacía; X acepta y cierra.

## Touch

Tocar otra ROM la selecciona. Un nuevo tap sobre la ROM ya seleccionada la
ejecuta. Deslizar verticalmente navega Cards/Lista y horizontalmente el Carrusel
sin ejecutar juegos. Los huecos entre filas o tarjetas no activan ROMs.

## Vistas y ajustes

Cards permite entre tres y seis columnas. Lista muestra cinco filas y la
portada del juego seleccionado; N64 conserva su proporción horizontal completa.
Carrusel presenta la portada central y sus vecinas. Lista y Carrusel incluyen
filtro alfabético y Favoritos. El fondo dinámico y su espera se guardan por app
en `sd:/retrou/config/<consola>/view.conf`.

Ajustes usa dos paneles: secciones a la izquierda y opciones a la derecha.
Controles presenta cuatro puertos N64, dos NES/SNES y uno GBA. Las acciones
se aplican al mando seleccionado, con asignación, orden y mapeo por dispositivo.

El menú rápido permite reanudar, gestionar estados con miniatura, editar
controles de la ROM, salir del juego a la biblioteca o cerrar la aplicación.
Los botones usados en el menú no se envían al juego.

La arquitectura, rutas, compilación y opciones se detallan en
[App nativa](../native/README.md); el estado de pruebas está en
[PENDIENTES.md](../PENDIENTES.md).
