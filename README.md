# gui — versión nueva en C

Editor de terminal para Linux, escrito desde cero en C11 con ncursesw.
Esta rama `c-rewrite` contiene código nuevo: no es una traducción del código Rust de Microsoft Edit.
La versión anterior de Rust permanece en `main`.

**Estado: primer prototipo sin compilar ni probar en ejecución.** No reemplazar todavía la versión instalada. La revisión realizada es de código, no una validación de funcionamiento.

## Aspecto y controles

- Fondo azul, texto blanco y barra superior blanca con letras negras.
- Sin números de línea y sin resaltar la línea donde está el cursor.
- Solo la selección explícita cambia de color.
- Diálogos clásicos grises con título claro, campos delimitados, casillas y botones con sombra de media celda.
- Menús: Alt+A Archivo, Alt+E Edición, Alt+V Ver, Alt+Y Ayuda. F10 abre los menús.
- Ctrl+B Buscar; Ctrl+R Reemplazar; F3 busca la siguiente coincidencia.
- Shift+flechas selecciona; Ctrl+A selecciona todo.
- Ctrl+C / Ctrl+Insert copia; Ctrl+X corta; Ctrl+V / Shift+Insert pega.
- Ctrl+S guarda; Ctrl+O abre; Ctrl+N crea un documento; Ctrl+Q sale.
- Ctrl+Z deshace y Ctrl+Y rehace, con hasta 64 estados por documento.
- Mouse para colocar el cursor, seleccionar y usar menús. El diálogo de búsqueda también permite activar controles con el mouse.

El portapapeles de esta primera versión es interno. Para pegar texto de otra aplicación se utiliza el pegado del terminal (habitualmente Ctrl+Shift+V). El editor recibe también el pegado delimitado del terminal. Algunos terminales capturan Ctrl+Insert o Shift+Insert antes de enviarlos a la aplicación.

## Preparar Debian o Ubuntu

```sh
apt update
apt install build-essential libncurses-dev git
git clone --branch c-rewrite --single-branch https://github.com/gpetruzzi/gui.git gui-c
cd gui-c
```

En Fedora, los requisitos son `gcc make ncurses-devel git`.

Cuando quieras compilar y probar, de forma separada de la versión instalada:

```sh
make
./gui prueba.txt
```

No hace falta Rust. Para una futura instalación, después de verificar el prototipo: `make install` (por defecto en `/usr/local/bin`).

## Comprobaciones pendientes en Linux

Se incluyen pruebas del núcleo (selección, historial, búsqueda, reemplazo y guardado UTF-8/CRLF). Se ejecutan con `make test`; **todavía no se compilaron ni se ejecutaron**.

1. Abrir un archivo UTF-8 con tildes, editarlo y guardar una copia.
2. Comprobar selección con las cuatro flechas y copiar/pegar con ambos juegos de atajos.
3. Verificar visualmente la línea sin resaltado y las sombras de botones.
4. Buscar, distinguir mayúsculas, palabras completas y reemplazar una o todas las coincidencias.
5. Deshacer y rehacer inserción, borrado, pegado y reemplazo de varias coincidencias.
6. Cancelar la apertura/salida con cambios sin guardar y comprobar que no se pierden.
7. Comprobar archivos con saltos LF y CRLF, guardado fallido y cambio de tamaño de terminal.

## Alcance de este prototipo

Un documento abierto a la vez, búsqueda literal sin expresiones regulares y archivos UTF-8. No se convierten archivos binarios ni codificaciones antiguas. Se preserva el BOM UTF-8 y el formato LF/CRLF uniforme; los archivos con saltos mezclados se normalizan a CRLF al guardar si contienen CRLF. Los enlaces simbólicos no se sobrescriben.

El almacenamiento y el historial usan copias en memoria: esta primera implementación está orientada a textos pequeños y medianos. La edición se hace por puntos de código Unicode; todavía no se agrupan emojis compuestos ni todos los caracteres combinados al mover o borrar. La presentación exacta depende de la fuente y de las capacidades de la terminal.

Licencia MIT. La interfaz usa ncursesw, una biblioteca independiente. El cuadro Acerca de identifica gui y su propia versión, 0.1.0.

