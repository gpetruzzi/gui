#!/bin/sh
# Author: Guillermo Petruzzi
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a local package from committed files, leaving the checkout untouched.
set -eu

if [ "$#" -gt 1 ]; then
    echo 'Usage: sh scripts/build-debian.sh [binary|source]' >&2
    exit 2
fi
mode=${1:-binary}
case "$mode" in
    binary|source) ;;
    *) echo 'Usage: sh scripts/build-debian.sh [binary|source]' >&2; exit 2 ;;
esac
for tool in git tar mktemp dpkg-buildpackage dpkg-parsechangelog; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing build tool: $tool. See docs/debian-packaging.md." >&2
        exit 1
    fi
done

repo=$(git rev-parse --show-toplevel)
cd "$repo"
build_root=$(mktemp -d "${TMPDIR:-/tmp}/gui-debian.XXXXXX")
echo "Build workspace: $build_root"
echo 'Building committed HEAD only; untracked files and local edits are excluded.'
git archive --format=tar --output="$build_root/committed.tar" HEAD
snapshot="$build_root/snapshot"
mkdir "$snapshot"
tar -xf "$build_root/committed.tar" -C "$snapshot"
version=$(dpkg-parsechangelog -l"$snapshot/debian/changelog" -SVersion)
package=$(dpkg-parsechangelog -l"$snapshot/debian/changelog" -SSource)
upstream=${version%-*}
upstream=${upstream#*:}
name="$package-$upstream"
source_dir="$build_root/$name"
# Freeze upstream sources at the published tag; take only debian/ from HEAD.
upstream_ref="refs/tags/v$upstream"
if ! git rev-parse --verify "$upstream_ref^{commit}" >/dev/null 2>&1; then
    echo "Missing release tag v$upstream. Run git fetch origin --tags." >&2
    exit 1
fi
mkdir "$source_dir"
git archive --format=tar --output="$build_root/upstream.tar" "$upstream_ref"
tar -xf "$build_root/upstream.tar" --exclude=debian -C "$source_dir"
cp -R "$snapshot/debian" "$source_dir/debian"
chmod 755 "$source_dir"
commit_epoch=$(git show -s --format=%ct "$upstream_ref")
tar --sort=name --mtime="@$commit_epoch" --owner=0 --group=0 --numeric-owner \
    --exclude="$name/debian" -cJf "$build_root/${package}_${upstream}.orig.tar.xz" -C "$build_root" "$name"

cd "$source_dir"
if [ "$mode" = source ]; then
    dpkg-buildpackage -S -sa -us -uc
else
    dpkg-buildpackage -b -us -uc
fi

output_dir="$repo/dist/debian"
mkdir -p "$output_dir"
for artifact in "$build_root"/*.deb "$build_root"/*.changes "$build_root"/*.buildinfo \
                "$build_root"/*.dsc "$build_root"/*.tar.xz "$build_root"/*.tar.gz; do
    if [ -f "$artifact" ]; then cp "$artifact" "$output_dir/"; fi
done
echo "Artifacts: $output_dir"
echo "Build workspace retained for inspection: $build_root"
