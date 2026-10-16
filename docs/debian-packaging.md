# Debian packaging candidate

Maintainer: **Guillermo Petruzzi <gpetruzzi@gmail.com>**.

These files prepare a local Debian package of gui 1.0. They do not mean
that Debian has accepted gui. The current candidate is `1.0-1~local3`, targeting `unstable` for clean-environment
review. This target only records the intended distribution; it does not upload
the package or imply archive acceptance.

## Install build tools on Debian 13

Run as root, or prefix the commands with sudo:

```sh
apt update
apt install build-essential debhelper libncurses-dev devscripts dpkg-dev \
    fakeroot lintian man-db groff-base git xz-utils
```

Package builds can be run as a normal user; root is needed only for installing
dependencies and the resulting package. Avoid setting `DEB_BUILD_OPTIONS=nocheck`
when validating this candidate, so its core tests run during the build.

## Build the binary package

From the checkout:

```sh
git pull --ff-only
sh scripts/build-debian.sh
```

The script exports **committed HEAD** to a temporary directory. It does not
include untracked files such as prueba.txt, local edits, or an old executable.
Commit packaging changes before using it to test those changes.
The checkout and its files are not cleaned or overwritten.

The package is built with debhelper, dpkg build flags and extra hardening.
Build failures, including core test failures, stop the script.
The temporary workspace is retained for inspection, including on failure.
On success, artifacts are copied to `dist/debian/`.

On Debian amd64, expect a main package named:

```text
dist/debian/gui_1.0-1~local3_amd64.deb
```

The architecture suffix differs on other machines. Debug packages, build
metadata and the upstream source archive may also be produced. Review package
contents and the corresponding changes file before installing:

```sh
dpkg-deb --info dist/debian/gui_1.0-1~local3_amd64.deb
dpkg-deb --contents dist/debian/gui_1.0-1~local3_amd64.deb
lintian --pedantic dist/debian/gui_1.0-1~local3_amd64.changes
```

Review Lintian output rather than assuming that a completed build is sufficient
for Debian acceptance. No build, Lintian, clean-environment or visual-test
results are claimed by the packaging files themselves.

## Install and test

Run the installation as root:

```sh
apt install ./dist/debian/gui_1.0-1~local3_amd64.deb
```

Then test the packaged executable and manual:

```sh
/usr/bin/gui --version
man gui
/usr/bin/gui prueba.txt
```

The package installs into `/usr`, not `/usr/local`. An older hand-installed
`/usr/local/bin/gui` may still take precedence when running just `gui`.
Use `/usr/bin/gui` explicitly until you decide how to manage that older copy;
the package never removes it. Package removal also leaves your documents and
per-user preferences intact.

Confirm keyboard selection, Find/Replace, file browsing, both editing panels,
color preferences, mouse selection, save failures and canceled exits with
unsaved changes. Repeat terminal tests in the KiTTY configuration you use.

The package contains an autopkgtest CLI smoke test. After installing the
candidate, it can also be run directly from the source checkout:

```sh
sh debian/tests/cli
```

The smoke test checks noninteractive help/version and argument validation;
it does not replace interactive terminal testing or the core build-time tests.

## Prepare a source package

```sh
sh scripts/build-debian.sh source
```

This produces the `.dsc`, `.orig.tar.xz`, `.debian.tar.xz` and associated metadata
in `dist/debian/`. The upstream tarball excludes `debian/`. Outputs are unsigned
local candidates, not ready-to-upload signed submissions.

Before sponsorship, build and test the source package in a clean Debian
environment, preferably unstable, using sbuild or an equivalent tool. Also
run autopkgtest and an installation/removal check such as piuparts. Keep the
logs, including failures, for the review.

## Before asking Debian to include gui

1. Check the package archive and WNPP for name or executable conflicts and
   existing packaging work. The name `gui` is a candidate, not a reservation.
2. Review code, licensing, dependency notices, installation and test results.
   Update Standards-Version if a newer applicable policy requires changes.
3. Publish a stable, tested upstream source release, with a matching version
   tag such as `v1.0`. The included `debian/watch` expects version tags; no tag
   or release is created by the build script.
4. File an Intent To Package report against `wnpp` if you will maintain the
   package, or find a Debian maintainer willing to do so. Do not invent an ITP
   number in the changelog.
5. Replace the local candidate version with the reviewed Debian revision,
   typically `1.0-1`, confirm the target distribution, and add the real
   ITP closure when available. Rebuild and sign with the maintainer's key.
6. Publish the source candidate on mentors.debian.net and request sponsorship.
   Address reviewer feedback; acceptance into Debian's NEW queue is separate.

No report, sponsorship request or email is sent automatically. Maintenance
includes future bug reports, security fixes and updated upstream releases.
An unsent draft is provided in [debian-itp-draft.txt](debian-itp-draft.txt).
Review its ownership statement and remove draft notes before any submission.

## References

- [Guide for Debian Maintainers](https://www.debian.org/doc/manuals/debmake-doc/)
- [Debian Policy](https://www.debian.org/doc/debian-policy/)
- [Debian Mentors introduction](https://mentors.debian.net/intro-maintainers/)
- [WNPP](https://www.debian.org/devel/wnpp/)
- [sbuild](https://wiki.debian.org/sbuild)
