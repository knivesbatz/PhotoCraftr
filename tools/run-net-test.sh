#!/bin/sh
# Native integration test of the updater's HTTPS client (src/updater/net.c)
# against the real GitHub.  Needs network access; not part of `make test`.
#
#   tools/run-net-test.sh [extra-anchor.pem]
#
# If the network path intercepts TLS (corporate proxy, sandbox), pass that
# proxy's CA certificate as extra-anchor.pem: it is compiled into THIS TEST
# BINARY ONLY (a temporary trust_anchors.c), never into PCUpdate.exe.
# Needs brssl (BRSSL=/path/to/brssl or see tools/trust/regen-trust-anchors.sh).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/pc-nettest
mkdir -p "$out"
"$root/tools/trust/regen-trust-anchors.sh" "$out/test_anchors.c" "$@" >/dev/null
cc -std=gnu99 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers \
   -fsanitize=address,undefined -fno-sanitize=alignment -DPC_VERSION=\"test\" \
   -I"$root/src" -I"$root/src/rt" -I"$root/src/updater" -I"$root/third_party" \
   -I"$root/third_party/bearssl/inc" -I"$root/third_party/bearssl/src" \
   "$root/tests/integration/nettest.c" "$root/src/updater/net.c" "$root/src/updater/httpparse.c" \
   "$root/src/updater/manifest.c" "$root/src/rt/pcfmt.c" "$out/test_anchors.c" \
   $(find "$root/third_party/bearssl/src" -name '*.c') \
   "$root/third_party/monocypher/monocypher.c" "$root/third_party/monocypher/optional/monocypher-ed25519.c" \
   -o "$out/nettest" 2>&1 | grep -v "third_party/bearssl" || true
T="$out/nettest"
fails=0
run() { echo "== $*"; "$T" "$@" || fails=$((fails + 1)); }
# 302 github.com -> raw.githubusercontent.com, then 200
run https://github.com/knivesbatz/PhotoCraftr/raw/main/README.md PhotoCraft
run https://api.github.com/repos/knivesbatz/PhotoCraftr '"full_name"'
# a missing release asset: 404 (-10)
run --expect-error -10 https://github.com/knivesbatz/PhotoCraftr/releases/download/v0.0.0-nonexistent/update-manifest.txt
# plain http is refused before any connection (-3)
run --expect-error -3 http://github.com/
# wrong host name for the certificate (-7) - only meaningful without interception
[ $# -eq 0 ] && run --expect-error -7 https://wrong.host.badssl.com/
echo "nettest: $fails failure(s)"
[ $fails -eq 0 ]
