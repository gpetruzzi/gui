#!/bin/sh
set -eu
cd "$(dirname "$0")"
command -v cargo >/dev/null || { echo 'Se necesita Rust 1.93 o posterior (cargo).'; exit 1; }
cargo build --release --locked -p edit --bin edit
mkdir -p dist
cp target/release/edit dist/gui
case "${1:-}" in
  --install)
    mkdir -p "$HOME/.local/bin"
    install -m 755 dist/gui "$HOME/.local/bin/gui"
    echo 'Instalado en ~/.local/bin/gui. Esa carpeta debe estar en PATH.'
    ;;
  --deb)
    command -v dpkg-deb >/dev/null
    arch=$(dpkg --print-architecture)
    root=$(mktemp -d)
    trap 'rm -rf "$root"' EXIT HUP INT TERM
    mkdir -p "$root/DEBIAN" "$root/usr/bin" "$root/usr/share/doc/gui"
    install -m 755 dist/gui "$root/usr/bin/gui"
    cp LICENSE "$root/usr/share/doc/gui/copyright"
    printf 'Package: gui\nVersion: 0.1.0\nArchitecture: %s\nMaintainer: gui contributors\nDepends: libc6\nSection: editors\nPriority: optional\nDescription: Editor de terminal con fondo azul basado en Microsoft Edit\n' "$arch" > "$root/DEBIAN/control"
    dpkg-deb --build --root-owner-group "$root" "dist/gui_0.1.0_${arch}.deb"
    ;;
  --rpm)
    command -v rpmbuild >/dev/null
    top=$(mktemp -d)
    trap 'rm -rf "$top"' EXIT HUP INT TERM
    mkdir -p "$top/SOURCES" "$top/SPECS" "$top/BUILD" "$top/BUILDROOT" "$top/RPMS" "$top/SRPMS"
    cp dist/gui LICENSE "$top/SOURCES/"
    cat > "$top/SPECS/gui.spec" <<'SPEC'
Name: gui
Version: 0.1.0
Release: 1
Summary: Editor de terminal con fondo azul
License: MIT
Source0: gui
Source1: LICENSE
%description
Editor de terminal basado en Microsoft Edit con tema azul y blanco.
%prep
%build
%install
mkdir -p %{buildroot}%{_bindir} %{buildroot}%{_licensedir}/gui
install -m 755 %{SOURCE0} %{buildroot}%{_bindir}/gui
install -m 644 %{SOURCE1} %{buildroot}%{_licensedir}/gui/LICENSE
%files
%{_bindir}/gui
%license %{_licensedir}/gui/LICENSE
SPEC
    rpmbuild --define "_topdir $top" -bb "$top/SPECS/gui.spec"
    find "$top/RPMS" -name '*.rpm' -exec cp {} dist/ \;
    ;;
  '') ;;
  *) echo 'Uso: sh build-linux.sh [--install|--deb|--rpm]' >&2; exit 1 ;;
esac
echo 'Resultado en dist/.'
