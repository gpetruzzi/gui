# gui — primera versión del código

Variante de Microsoft Edit para Debian, Ubuntu y Fedora. Nombre del editor: gui.
Conserva la licencia MIT y los avisos del proyecto original.

## Cambios

- Fondo azul #0000AA y texto blanco #FFFFFF, independiente del tema del terminal.
- Menús y barra de estado en tonos azules con texto blanco.
- Archivos abiertos inicialmente como texto sin resaltado de sintaxis para mantener las letras blancas. Se puede activar un lenguaje desde los menús del editor.
- Identificación propia en el diálogo Acerca de.

## Controles heredados de Edit

Shift + flechas selecciona; Ctrl+C o Ctrl+Insert copia;
Ctrl+V o Shift+Insert pega; Ctrl+X corta; Ctrl+S guarda.
El terminal debe transmitir esos atajos a la aplicación. Algunos terminales capturan Shift+Insert para pegar desde el portapapeles del sistema.
Edit también mantiene un portapapeles interno; su sincronización con el del sistema depende del soporte OSC 52 del terminal.

## Compilar en Linux

Se necesita Rust 1.93 o posterior con Cargo, un compilador de C y acceso a las dependencias de Cargo.

Debian / Ubuntu: `sudo apt install build-essential pkg-config libicu-dev`

Fedora: `sudo dnf install gcc pkgconf-pkg-config libicu-devel rpm-build`

Con Rust instalado:

```sh
sh build-linux.sh
./dist/gui archivo.txt
```

Instalación personal: `sh build-linux.sh --install`.

Paquete Debian/Ubuntu: `sh build-linux.sh --deb` (requiere dpkg-deb).

Paquete Fedora: `sh build-linux.sh --rpm` (requiere rpmbuild).

Los paquetes se generan para la arquitectura y las bibliotecas del sistema donde se compilan. Compilar por separado en cada distribución de destino; no se ha comprobado compatibilidad entre versiones.

## Estado de validación

Cambios revisados estáticamente. Esta máquina Windows no dispone de Cargo ni de un entorno Linux operativo: aún no se han compilado, ejecutado ni probado los paquetes.
En Linux ejecutar `cargo test --locked -p edit` y verificar visualmente colores, selección, guardado y pegado antes de usar con archivos importantes.


Proyecto original: https://github.com/microsoft/edit. Documentación original: [README-UPSTREAM.md](README-UPSTREAM.md).

