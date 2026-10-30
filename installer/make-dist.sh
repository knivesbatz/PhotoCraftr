#!/bin/sh
# Assemble a release in dist/ (run by `make dist`, after `make check msi`).
#
#   installer/make-dist.sh <version>
#
# dist/ receives:
#   PhotoCraftClassic-Setup.msi  Windows Installer package
#   Setup.exe                    bootstrapper (wizard, Windows Installer check)
#   PhotoCraftClassic.exe        standalone program (runs without installing)
#   PCUpdate.exe                 updater
#   RELEASE-NOTES.txt
#   update-manifest.txt          signed update manifest, if PC_UPDATE_SIGNING_KEY
#                                is set (hex Ed25519 seed); otherwise
#   update-manifest.unsigned.txt (and a warning)
#   SHA256SUMS.txt
#
# The asset names matter: PCUpdate.exe fetches
#   https://github.com/<repo>/releases/latest/download/update-manifest.txt
# and the manifest points at .../releases/download/v<version>/PhotoCraftClassic-Setup.msi
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
version=${1:?usage: make-dist.sh <version>}
dist=$root/dist
bw=$root/build/win
msi=$root/build/msi/PhotoCraftClassic-Setup.msi
pcsign=$root/build/native/pcsign

for f in "$msi" "$bw/Setup.exe" "$bw/PhotoCraftClassic.exe" "$bw/PCUpdate.exe" "$pcsign"; do
    [ -f "$f" ] || { echo "make-dist.sh: missing $f (run make check msi build/native/pcsign first)" >&2; exit 1; }
done
python3 "$root/tools/gen-license-text.py" --check

rm -rf "$dist"
mkdir -p "$dist"
cp "$msi" "$dist/PhotoCraftClassic-Setup.msi"
cp "$bw/Setup.exe" "$bw/PhotoCraftClassic.exe" "$bw/PCUpdate.exe" "$dist/"
cp "$root/build/msi/stage/RELEASE-NOTES.txt" "$dist/RELEASE-NOTES.txt"

# Release notes for the manifest: the markdown source if there is one.
notes=""
for n in "$root/docs/release-notes.md" "$root/CHANGELOG.md"; do
    [ -f "$n" ] && { notes=$n; break; }
done
[ -n "$notes" ] || notes=$dist/RELEASE-NOTES.txt

"$root/tools/make-manifest.sh" "$dist/PhotoCraftClassic-Setup.msi" "$version" "$notes" > "$dist/update-manifest.unsigned.txt"

if [ -n "${PC_UPDATE_SIGNING_KEY:-}" ]; then
    # Refuse to publish a manifest the shipped updater could not verify.
    pub=$("$pcsign" pubkey - | head -1)
    hexlist=$(tr -d ' \n\t' < "$root/src/updater/pubkey.h" | grep -o '{0x[0-9a-fx,]*}' | tr -d '{}' | sed 's/0x//g; s/,//g')
    if ! echo "$hexlist" | grep -qx "$pub"; then
        echo "make-dist.sh: the signing key's public key $pub is not pinned in src/updater/pubkey.h" >&2
        exit 1
    fi
    if grep -q "PC_UPDATE_PUBKEY_IS_TEST_KEY 1" "$root/src/updater/pubkey.h"; then
        echo "make-dist.sh: WARNING: src/updater/pubkey.h still says it holds the TEST key" >&2
    fi
    "$pcsign" sign - "$dist/update-manifest.unsigned.txt" > "$dist/update-manifest.txt"
    "$pcsign" verify "$pub" "$dist/update-manifest.txt"
    rm -f "$dist/update-manifest.unsigned.txt"
else
    echo "make-dist.sh: WARNING: PC_UPDATE_SIGNING_KEY is not set; wrote dist/update-manifest.unsigned.txt" >&2
    echo "make-dist.sh: WARNING: sign it with: build/native/pcsign sign <key> dist/update-manifest.unsigned.txt > dist/update-manifest.txt" >&2
fi

(cd "$dist" && sha256sum -- * | grep -v ' SHA256SUMS.txt$' > SHA256SUMS.txt)
echo "make-dist.sh: dist/ for $version:"
ls -l "$dist"
