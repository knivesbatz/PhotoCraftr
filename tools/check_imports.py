#!/usr/bin/env python3
"""Fail if a Windows executable imports anything not on the Windows 95 allowlist.

    tools/check_imports.py --objdump i686-w64-mingw32-objdump \\
        --allowlist tools/win95-api-allowlist.txt build/win/*.exe

Imports are read from `objdump -p` (the PE import directory).  Matching is
case-insensitive on the DLL name and exact on the function name.  Imports by
ordinal are always reported (we never need them).  Functions reached at run
time through LoadLibrary/GetProcAddress are, by design, not visible here.
"""
import argparse
import re
import subprocess
import sys


def load_allowlist(path):
    allowed = set()
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if "!" not in line:
                sys.exit(f"{path}:{lineno}: expected DLL!Function, got {line!r}")
            dll, func = line.split("!", 1)
            allowed.add((dll.strip().lower(), func.strip()))
    return allowed


DLL_RE = re.compile(r"^\s*DLL Name:\s*(\S+)")
# "	4047c	 1569  RegCloseKey"  (hint/name table row: rva, hint, name)
FUNC_RE = re.compile(r"^\s*([0-9a-fA-F]+)\s+(\d+)\s+(\S+)\s*$")
ORD_RE = re.compile(r"^\s*([0-9a-fA-F]+)\s+<none>\s+([0-9a-fA-F]+)\s*$")


def imports_of(objdump, exe):
    out = subprocess.run([objdump, "-p", exe], check=True, capture_output=True, text=True).stdout
    result = []
    dll = None
    in_table = False
    for line in out.splitlines():
        m = DLL_RE.match(line)
        if m:
            dll = m.group(1)
            in_table = False
            continue
        if dll is None:
            continue
        if "Hint/Ord" in line and "Name" in line:
            in_table = True
            continue
        if in_table:
            if not line.strip():
                in_table = False
                dll = None
                continue
            m = FUNC_RE.match(line)
            if m and m.group(3) != "<none>":
                result.append((dll, m.group(3)))
                continue
            m = ORD_RE.match(line) or re.match(r"^\s*[0-9a-fA-F]+\s+(\d+)\s*$", line)
            if m:
                result.append((dll, "#ordinal " + m.groups()[-1]))
                continue
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--objdump", default="objdump")
    ap.add_argument("--allowlist", required=True)
    ap.add_argument("--verbose", "-v", action="store_true", help="list every import")
    ap.add_argument("exes", nargs="+")
    args = ap.parse_args()

    allowed = load_allowlist(args.allowlist)
    bad = 0
    for exe in args.exes:
        try:
            imps = imports_of(args.objdump, exe)
        except FileNotFoundError as e:
            print(f"check_imports: {e}", file=sys.stderr)
            return 2
        except subprocess.CalledProcessError as e:
            print(f"check_imports: objdump failed on {exe}: {e.stderr.strip()}", file=sys.stderr)
            return 2
        if not imps:
            print(f"check_imports: {exe}: no imports found (not a PE file?)", file=sys.stderr)
            bad += 1
            continue
        missing = sorted({(d, f) for d, f in imps if (d.lower(), f) not in allowed})
        dlls = sorted({d.upper() for d, _ in imps})
        if args.verbose:
            for d, f in sorted(set(imps)):
                print(f"  {exe}: {d}!{f}")
        if missing:
            bad += len(missing)
            print(f"{exe}: {len(missing)} import(s) not on the Windows 95 allowlist:")
            for d, f in missing:
                print(f"    {d}!{f}")
        else:
            print(f"{exe}: OK ({len(set(imps))} imports from {', '.join(dlls)})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
