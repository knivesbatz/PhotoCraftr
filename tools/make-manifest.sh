#!/bin/sh
# Build an UNSIGNED PhotoCraft Classic update manifest on stdout.
#
#   tools/make-manifest.sh <msi-file> <version> [notes-file] > update-manifest.unsigned.txt
#   build/native/pcsign sign - update-manifest.unsigned.txt > update-manifest.txt
#
# Environment overrides:
#   PC_UPDATE_CHANNEL   channel name                  (default: stable)
#   PC_UPDATE_SEQUENCE  anti-rollback sequence number (default: current UTC
#                       Unix time, i.e. monotonic across releases)
#   PC_UPDATE_RELEASED  release date YYYY-MM-DD       (default: today, UTC)
#   PC_UPDATE_URL       full MSI download URL         (default: GitHub release
#                       asset for tag v<version>)
#   PC_UPDATE_REPO      owner/repo for the default URL (default: knivesbatz/PhotoCraftr)
#
# Format: docs/updater.md, "Manifest format".
set -eu

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    echo "usage: $0 <msi-file> <version> [notes-file]" >&2
    exit 64
fi
msi=$1
version=$2
notes=${3:-}

[ -f "$msi" ] || { echo "$0: $msi: no such file" >&2; exit 1; }
case "$version" in
    *[!0-9A-Za-z.-]*|"") echo "$0: bad version '$version'" >&2; exit 1 ;;
esac
echo "$version" | grep -Eq '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$' ||
    { echo "$0: version must be MAJOR.MINOR.PATCH[-PRE]" >&2; exit 1; }

channel=${PC_UPDATE_CHANNEL:-stable}
sequence=${PC_UPDATE_SEQUENCE:-$(date -u +%s)}
released=${PC_UPDATE_RELEASED:-$(date -u +%Y-%m-%d)}
repo=${PC_UPDATE_REPO:-knivesbatz/PhotoCraftr}
url=${PC_UPDATE_URL:-https://github.com/$repo/releases/download/v$version/$(basename "$msi")}
size=$(wc -c < "$msi" | tr -d ' ')
sha=$(sha256sum "$msi" | cut -d' ' -f1)

case "$url" in https://*) ;; *) echo "$0: URL must be https://" >&2; exit 1 ;; esac

printf 'PCUPDATE 1\n'
printf 'product PhotoCraftClassic\n'
printf 'channel %s\n' "$channel"
printf 'version %s\n' "$version"
printf 'sequence %s\n' "$sequence"
printf 'released %s\n' "$released"
printf 'msi_url %s\n' "$url"
printf 'msi_size %s\n' "$size"
printf 'msi_sha256 %s\n' "$sha"
if [ -n "$notes" ]; then
    [ -f "$notes" ] || { echo "$0: $notes: no such file" >&2; exit 1; }
    # One "notes" line per text line: strip CR, control characters and
    # markdown heading marks, drop leading/trailing blank lines, cap at
    # 60 lines of 900 bytes (the updater accepts 64 lines / 4 KB total).
    tr -d '\r' < "$notes" | tr '\t' ' ' | LC_ALL=C tr -d '\000-\010\013-\037\177' |
        sed -e 's/^#\{1,6\} *//' |
        awk 'NF { started = 1 } started { buf[++n] = $0 }
             END { while (n > 0 && buf[n] ~ /^[ \t]*$/) n--;
                   if (n > 60) n = 60;
                   total = 0;
                   for (i = 1; i <= n; i++) {
                       line = substr(buf[i], 1, 900); sub(/[ \t]+$/, "", line);
                       total += length(line) + 2;
                       if (total > 4000) break;
                       if (line == "") print "notes"; else print "notes " line } }'
fi
