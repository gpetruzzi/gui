# Debian 1.0 submission

ITP: https://bugs.debian.org/1151078
Release: https://github.com/gpetruzzi/gui/releases/tag/v1.0

The initial upload candidate is 1.0-1 for unstable. Its changelog closes the
ITP upon acceptance. Local trial revisions were never uploaded to Debian
and are omitted from the initial upload changelog.

The build helper uses upstream files from the published v1.0 tag and only
debian/ from committed HEAD. Its deterministic orig tarball excludes
debian/ and is repacked as xz, so differs from GitHub's gzip archive.
Preserve this orig tarball for later Debian revisions. Do not move the tag.

```sh
cd ~/gui-c
git switch main
git pull --ff-only
git fetch origin --tags
sh scripts/build-debian.sh source
lintian --pedantic dist/debian/gui_1.0-1_source.changes
```

Repeat the clean unstable build, autopkgtest and piuparts for this exact
source candidate. Earlier results apply to 1.0-1~local3. Preserve the logs.

Next, create a Mentors account and register your public OpenPGP key.
Keep its private key and passphrase private. Sign the source changes using
debsign and upload using dput over HTTPS, following the current guide:
https://mentors.debian.net/intro-maintainers/

After upload, mark the package as needing a sponsor and use its package
page's request-for-sponsorship template. No sponsorship request has been
sent yet. A sponsor must review and upload the package, followed by Debian
NEW queue review for a new package.
