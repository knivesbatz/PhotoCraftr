#!/bin/sh
# Build the Windows Installer package with wixl (msitools) and check that it
# stays compatible with Windows Installer 1.1 (Windows 95/98/Me/NT4).
#
#   installer/build-msi.sh <version> [output.msi]
#
# Inputs (override with environment variables, e.g. for tests):
#   APP_EXE  build/win/PhotoCraftClassic.exe
#   UPD_EXE  build/win/PCUpdate.exe
# Output: build/msi/PhotoCraftClassic-Setup.msi (or the given path)
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
version=${1:?usage: build-msi.sh <version> [output.msi]}
out=${2:-$root/build/msi/PhotoCraftClassic-Setup.msi}
APP_EXE=${APP_EXE:-$root/build/win/PhotoCraftClassic.exe}
UPD_EXE=${UPD_EXE:-$root/build/win/PCUpdate.exe}
stage=$root/build/msi/stage
WIXL=${WIXL:-wixl}

echo "$version" | grep -Eq '^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,5}$' || {
    echo "build-msi.sh: MSI versions must be MAJOR.MINOR.PATCH (major/minor < 256, patch < 65536): '$version'" >&2
    exit 1
}
for f in "$APP_EXE" "$UPD_EXE" "$root/LICENSE-MIT" "$root/LICENSE-APACHE"; do
    [ -f "$f" ] || { echo "build-msi.sh: missing $f" >&2; exit 1; }
done

rm -rf "$stage"
mkdir -p "$stage" "$(dirname "$out")"

# Text files get CRLF line endings so Notepad on Windows 95 shows them.
crlf() { sed -e 's/\r$//' -e 's/$/\r/' "$1" > "$2"; }

cp "$APP_EXE" "$stage/PhotoCraftClassic.exe"
cp "$UPD_EXE" "$stage/PCUpdate.exe"
crlf "$root/LICENSE-MIT" "$stage/LICENSE-MIT.txt"
crlf "$root/LICENSE-APACHE" "$stage/LICENSE-APACHE.txt"
have_notice=0
if [ -f "$root/NOTICE" ]; then crlf "$root/NOTICE" "$stage/NOTICE.txt"; have_notice=1; fi

# README.txt: README.md if it has real content, else a short generated one.
if [ -f "$root/README.md" ] && [ "$(wc -l < "$root/README.md")" -gt 5 ]; then
    crlf "$root/README.md" "$stage/README.txt"
else
    cat > "$stage/README.tmp" <<EOF
PhotoCraft Classic $version
===========================

PhotoCraft Classic is a free image editor for Windows 95, 98, Me, NT 4.0,
2000, XP, Vista, 7, 8 and 10.

Start it from Start > Programs > PhotoCraft Classic.

Updates: Start > Programs > PhotoCraft Classic > Check for Updates, or
Help > Check for Updates in the program.  Updates are downloaded over HTTPS
and verified with a digital signature before they are installed.  Without
Internet access, download update-manifest.txt and
PhotoCraftClassic-Setup.msi from the releases page on another computer and
run "PCUpdate.exe --from-file".

Releases and source code: https://github.com/knivesbatz/PhotoCraftr

PhotoCraft Classic is licensed under the MIT License or the Apache License
2.0, at your option (LICENSE-MIT.txt, LICENSE-APACHE.txt).
EOF
    crlf "$stage/README.tmp" "$stage/README.txt"
    rm -f "$stage/README.tmp"
fi

# RELEASE-NOTES.txt: docs/release-notes.md, CHANGELOG.md or a stub.
notes=""
for n in "$root/docs/release-notes.md" "$root/CHANGELOG.md" "$root/CHANGELOG"; do
    [ -f "$n" ] && { notes=$n; break; }
done
if [ -n "$notes" ]; then
    crlf "$notes" "$stage/RELEASE-NOTES.txt"
else
    printf 'PhotoCraft Classic %s\n\nSee https://github.com/knivesbatz/PhotoCraftr/releases for the release notes.\n' "$version" > "$stage/RELEASE-NOTES.tmp"
    crlf "$stage/RELEASE-NOTES.tmp" "$stage/RELEASE-NOTES.txt"
    rm -f "$stage/RELEASE-NOTES.tmp"
fi

tmp=$out.tmp.msi
rm -f "$tmp"
"$WIXL" -a x86 -D Version="$version" -D StageDir="$stage" -D HaveNotice="$have_notice" \
    -o "$tmp" "$root/installer/PhotoCraftClassic.wxs"

# ---- post-process for Windows Installer 1.1 (things wixl cannot express) ----
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
q() { msibuild "$tmp" -q "$1"; }

tables=$(msiinfo tables "$tmp")
has_table() { echo "$tables" | grep -qx "$1"; }

# 8.3 short names ("SHORT|Long name"): Windows 95/NT4 and FAT volumes.
q "UPDATE \`Directory\` SET \`DefaultDir\` = 'PHOTOCRA|PhotoCraft Classic' WHERE \`Directory\` = 'INSTALLDIR'"
q "UPDATE \`Directory\` SET \`DefaultDir\` = 'PHOTOCRA|PhotoCraft Classic' WHERE \`Directory\` = 'AppMenuDir'"
q "UPDATE \`File\` SET \`FileName\` = 'PHOTOCRA.EXE|PhotoCraftClassic.exe' WHERE \`File\` = 'PhotoCraftClassic.exe'"
q "UPDATE \`File\` SET \`FileName\` = 'RELNOTES.TXT|RELEASE-NOTES.txt' WHERE \`File\` = 'RELEASENOTES.txt'"
q "UPDATE \`File\` SET \`FileName\` = 'LICMIT.TXT|LICENSE-MIT.txt' WHERE \`File\` = 'LICENSEMIT.txt'"
q "UPDATE \`File\` SET \`FileName\` = 'LICAPACH.TXT|LICENSE-APACHE.txt' WHERE \`File\` = 'LICENSEAPACHE.txt'"
q "UPDATE \`Shortcut\` SET \`Name\` = 'PHOTOCRA|PhotoCraft Classic' WHERE \`Shortcut\` = 'AppShortcut'"
q "UPDATE \`Shortcut\` SET \`Name\` = 'PHOTOCRA|PhotoCraft Classic' WHERE \`Shortcut\` = 'DesktopShortcut'"
q "UPDATE \`Shortcut\` SET \`Name\` = 'UPDATES|Check for Updates' WHERE \`Shortcut\` = 'UpdateShortcut'"
q "UPDATE \`Shortcut\` SET \`Name\` = 'RELNOTES|Release Notes' WHERE \`Shortcut\` = 'NotesShortcut'"

# Desktop shortcut only when DESKTOPSHORTCUT=1.
q "UPDATE \`Component\` SET \`Condition\` = 'DESKTOPSHORTCUT = \"1\"' WHERE \`Component\` = 'Desktop'"

# Remove the old version INSIDE the transaction (after InstallInitialize),
# so a failed upgrade rolls back to the old version intact.
q "UPDATE \`InstallExecuteSequence\` SET \`Sequence\` = 1501 WHERE \`Action\` = 'RemoveExistingProducts'"

# MsiFileHash (Installer 2.0) only speeds up repairs: drop it.  Also drop
# empty tables wixl always creates but this package does not use.
has_table MsiFileHash && q "DROP TABLE \`MsiFileHash\`"
for t in ServiceInstall ServiceControl IniFile RemoveIniFile; do
    if has_table "$t" && [ "$(msiinfo export "$tmp" "$t" | wc -l)" -le 3 ]; then q "DROP TABLE \`$t\`"; fi
done

# Columns added after Installer 1.1 (Shortcut: 4.0 resource columns,
# CustomAction: 5.0 ExtendedType): rebuild those tables with the 1.1 schema.
strip_cols() {   # table, number of leading columns to keep
    msiinfo export "$tmp" "$1" > "$work/$1.full.idt"
    awk -F'\t' -v OFS='\t' -v n="$2" 'NR == 3 { print; next } { l = $1; for (i = 2; i <= n; i++) l = l OFS $i; print l }' \
        "$work/$1.full.idt" > "$work/$1.idt"
    q "DROP TABLE \`$1\`"
    msibuild "$tmp" -i "$work/$1.idt"
}
has_table Shortcut && strip_cols Shortcut 12
has_table CustomAction && strip_cols CustomAction 4

# Standard actions that do not exist in Installer 1.1 (1.5+): remove them
# from every sequence table (they would be "unknown action" errors).
for seq in InstallExecuteSequence AdminExecuteSequence AdvtExecuteSequence InstallUISequence AdminUISequence; do
    has_table "$seq" || continue
    for act in MsiPublishAssemblies MsiUnpublishAssemblies MsiConfigureServices; do
        q "DELETE FROM \`$seq\` WHERE \`Action\` = '$act'"
    done
done
# Summary information (written by wixl from the .wxs): schema 110 =
# "page count" property, x86 template, a package code GUID.
msiinfo suminfo "$tmp" > "$work/suminfo.txt"

# ---- checks ----
fail=0
# msiinfo prints PID_PAGECOUNT (the minimum installer version) as "Version".
grep -q "^Version: 110 " "$work/suminfo.txt" || { echo "build-msi.sh: summary page count (installer version) is not 110:" >&2; cat "$work/suminfo.txt" >&2; fail=1; }
grep -Eq "^Revision number \(UUID\): \{[0-9A-F-]{36}\}" "$work/suminfo.txt" || { echo "build-msi.sh: package code missing" >&2; fail=1; }
grep -q "Template: Intel;1033" "$work/suminfo.txt" || { echo "build-msi.sh: template is not Intel;1033" >&2; fail=1; }
# Tables defined by Windows Installer 1.0/1.1 (plus the always-present
# system tables).  Anything else is newer and might break old installers.
allowed=" _Columns _Tables _Storages _Streams _Validation _SummaryInformation _ForceCodepage ActionText AdminExecuteSequence
 AdminUISequence AdvtExecuteSequence AdvtUISequence AppId AppSearch BBControl Billboard Binary BindImage CCPSearch
 CheckBox Class ComboBox CompLocator Complus Component Condition Control ControlCondition ControlEvent CreateFolder
 CustomAction Dialog Directory DrLocator DuplicateFile Environment Error EventMapping Extension Feature
 FeatureComponents File Font Icon IniFile IniLocator InstallExecuteSequence InstallUISequence IsolatedComponent
 LaunchCondition ListBox ListView LockPermissions Media MIME MoveFile ODBCAttribute ODBCDataSource ODBCDriver
 ODBCSourceAttribute ODBCTranslator Patch PatchPackage ProgId Property PublishComponent RadioButton Registry
 RegLocator RemoveFile RemoveIniFile RemoveRegistry ReserveCost SelfReg ServiceControl ServiceInstall Shortcut
 Signature TextStyle TypeLib UIText Upgrade Verb "
allowed=" $(echo $allowed) "
for t in $(msiinfo tables "$tmp"); do
    case "$allowed" in *" $t "*) ;; *) echo "build-msi.sh: table $t is newer than Windows Installer 1.1" >&2; fail=1 ;; esac
done
newer_actions="MsiPublishAssemblies MsiUnpublishAssemblies MsiConfigureServices MsiUnpublishAssemblies"
for seq in InstallExecuteSequence InstallUISequence AdminExecuteSequence AdvtExecuteSequence; do
    msiinfo export "$tmp" "$seq" > "$work/$seq.idt" 2>/dev/null || continue
    for a in $newer_actions; do
        if grep -q "^$a	" "$work/$seq.idt"; then echo "build-msi.sh: $seq still has $a" >&2; fail=1; fi
    done
done
# RemoveExistingProducts must run inside the transaction (after InstallInitialize).
msiinfo export "$tmp" InstallExecuteSequence > "$work/ies.idt"
rep=$(awk -F'\t' '$1=="RemoveExistingProducts"{print $3}' "$work/ies.idt")
ini=$(awk -F'\t' '$1=="InstallInitialize"{print $3}' "$work/ies.idt")
if [ -z "$rep" ] || [ -z "$ini" ] || [ "$rep" -le "$ini" ]; then
    echo "build-msi.sh: RemoveExistingProducts ($rep) must come after InstallInitialize ($ini)" >&2
    fail=1
fi
# Every file needs a short name (FAT 8.3) for Windows 95 / NT4.
msiinfo export "$tmp" File | awk -F'\t' 'NR>3 && $3 !~ /\|/ && length($3) > 12 {print "build-msi.sh: file without short name: " $3; bad=1} END{exit bad}' >&2 || fail=1

if [ "$fail" -ne 0 ]; then
    rm -f "$tmp"
    exit 1
fi
mv "$tmp" "$out"
echo "build-msi.sh: wrote $out ($(wc -c < "$out" | tr -d ' ') bytes, version $version)"
