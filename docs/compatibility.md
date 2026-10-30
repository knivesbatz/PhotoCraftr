# Compatibility

> **Status (0.1.0, 2026-10-30):** no row is **Verified** yet. Nothing has been run on a real
> Windows 95-10 machine or VM. Every row is **Expected**: the static checks below pass for all three
> executables, and the program was smoke-tested under Wine 9 (32-bit) with Wine set to report
> Windows 95, Windows 98 and Windows 10. Wine is not Windows, so it cannot prove compatibility.
> A row becomes **Verified** only after the real-OS procedure below has been run, with the date,
> build, machine or VM and tester recorded here.

## Matrix

Status values:

- **Verified**: the full real-OS procedure (below) passed on this version.
- **Expected**: the static checks pass and nothing is known to block, but the procedure has not
  been run on the real OS.
- **Untested**: no evidence yet.

| Windows version | Arch | Status | Method | Notes |
|---|---|---|---|---|
| Windows 95 (retail / RTM, 4.00.950) | x86 | Expected | Static checks; Wine smoke test | Needs Winsock with TCP/IP installed for the updater. Windows Installer 2.0 via InstMsiA.exe. No `TrackMouseEvent` (use `_TrackMouseEvent` or our own capture logic). No mouse-wheel message; the app handles `WM_MOUSEWHEEL` where the OS sends it and otherwise works without a wheel. |
| Windows 95 OSR2 / OSR2.1 / OSR2.5 (4.00.950B/C) | x86 | Expected | Static checks; Wine smoke test | As retail 95. FAT32 on OSR2. |
| Windows 98 (4.10.1998) | x86 | Expected | Static checks; Wine smoke test | Windows Installer 2.0 via InstMsiA.exe. Gradient captions available. |
| Windows 98 SE (4.10.2222) | x86 | Expected | Static checks; Wine smoke test | As 98. |
| Windows ME (4.90.3000) | x86 | Expected | Static checks; Wine smoke test | Ships Windows Installer 1.2; upgrade to 2.0 via InstMsiA.exe. |
| Windows NT 4.0 SP6 / SP6a | x86 | Expected | Static checks; Wine smoke test | Windows Installer 2.0 via InstMsiW.exe (needs SP6). No gradient captions (`SPI_GETGRADIENTCAPTIONS` unsupported). |
| Windows 2000 (SP4) | x86 | Expected | Static checks; Wine smoke test | Windows Installer 2.0 is built in (SP3+ has 2.0; 3.1 is available). |
| Windows XP (SP2/SP3) | x86 | Expected | Static checks; Wine smoke test | Windows Installer 3.1 built in (SP2+). Test with Luna themes on and with the classic theme. |
| Windows XP Professional x64 | x64 (WOW64) | Expected | Static checks; Wine smoke test | 32-bit binary under WOW64. |
| Windows Vista (SP2) | x86 / x64 | Expected | Static checks; Wine smoke test | UAC: install per-machine with elevation. The updater must not write to Program Files without elevation. Check VirtualStore redirection. |
| Windows 7 (SP1) | x86 / x64 | Expected | Static checks; Wine smoke test | DWM composition; our custom-drawn frame must stay correct with Aero on and off. |
| Windows 8 | x86 / x64 | Expected | Static checks; Wine smoke test | |
| Windows 8.1 | x86 / x64 | Expected | Static checks; Wine smoke test | The app calls `SetProcessDPIAware` (looked up at run time) so pixel art stays sharp; at high DPI the interface is small. |
| Windows 10 (22H2) | x86 / x64 | Expected | Static checks; Wine smoke test | TLS to the update server must work (BearSSL, not Schannel). Check SmartScreen on unsigned builds. |

Windows 11 is not in scope for this matrix, but it is expected to behave like Windows 10 x64.

## Known prerequisites per OS

| Prerequisite | Applies to | Details |
|---|---|---|
| **Winsock with TCP/IP installed** | Windows 95 (all releases) | The updater (`PCUpdate.exe`) loads `wsock32.dll` (Winsock 1.1) at run time, so it starts without it and "Install update from file" still works; only online checks need it. Windows 95 has Winsock 1.1 only if the TCP/IP protocol is installed (Control Panel > Network > Add > Protocol > Microsoft > TCP/IP). Without it the main program still runs; the updater reports "network not available". Winsock 2 is **not** required. |
| **Windows Installer 1.1 or later** | 95, 98, 98 SE: `InstMsiA.exe`. NT 4.0 SP6: `InstMsiW.exe`. ME and 2000+ already qualify. | The MSI is authored for Windows Installer 1.1 (summary page count 110; no post-1.1 tables, checked by `installer/build-msi.sh`). `Setup.exe` reads the `msi.dll` file version via `version.dll` and, below 1.1, runs `InstMsiA.exe` (9x) or `InstMsiW.exe` (NT 4) from its own folder, handles the reboot request, then runs the MSI. The redistributables are Microsoft's and are not bundled in this repository (see docs/release.md). |
| **Correct system clock** | All (updater) | HTTPS certificate validation (BearSSL X.509) fails if the date is wrong. Many retro installs boot with a 1998/2000 clock. The updater shows "Check the date and time in Control Panel" when certificates are "not yet valid" or "expired". |
| **Display** | All | 640×480 minimum, 800×600 recommended. **256 colors or more recommended**; 16-color (VGA) mode is supported, with images dithered (the UI uses only the 16 VGA colors). |
| **CPU / FPU** | All | i486 or later with an x87 FPU. 486SX needs the 487 coprocessor; the build uses x87 instructions and no SSE or MMX. |
| **RAM** | All | Guidance (to be measured): 16 MB minimum, 32 MB or more recommended. |
| **Disk** | All | Scratch/tile swap in `%TEMP%` (9x: `C:\WINDOWS\TEMP`). |

## Verification methods used in this repository

### Static checks (run on every build: `make check`, `make test`)

1. **Import-table audit against a Windows 95 API allowlist.**
   - `tools/check_imports.py` runs `objdump -p` on each EXE (`PhotoCraftClassic.exe`,
     `PCUpdate.exe`, `Setup.exe`) and fails if any imported DLL or function is not in
     `tools/win95-api-allowlist.txt`.
   - The allowlist contains only functions exported by retail Windows 95 `KERNEL32`, `USER32`,
     `GDI32`, `ADVAPI32`, `SHELL32` (`DragQueryFileA`/`DragFinish`), `WSOCK32` and `VERSION`,
     ANSI (`…A`) variants only. It was curated from Win95-era SDK documentation; it has not been
     machine-checked against a real Windows 95 `KERNEL32.DLL` export table.
   - Any newer API must be loaded dynamically with `GetProcAddress` and guarded.
2. **PE header check.** The Optional Header must have `MajorSubsystemVersion.Minor = 4.0` and
   `MajorOperatingSystemVersion = 4.0`, set by the Makefile with `--subsystem windows:4.0` and the
   `--major/minor-*-version` flags.
   - `DllCharacteristics` must have no `DYNAMIC_BASE` or `NX_COMPAT` (Win9x ignores them, but they
     are disabled for determinism).
   - There must be no TLS directory. The Win9x loader does not run TLS callbacks for statically
     loaded images.
   - There must be no relocation-stripping problems: `--disable-reloc-section` gives a fixed image
     base of 0x400000.
3. **No C-runtime dependency.** The import list must not contain `msvcrt.dll`, `msvcr*.dll`,
   `ucrtbase.dll` or `api-ms-win-crt-*`. The binaries are linked `-nostdlib`, with `src/rt/pcrt.c`
   providing the libc subset on KERNEL32. Retail Windows 95 ships no `msvcrt.dll`.
4. **No SSE or MMX.** Compiled with `-march=i486 -mno-sse -mno-mmx -mfpmath=387`. The check
   disassembles each binary (`objdump -d`) and fails on any XMM/MMX register or SSE/SSE2/MMX
   mnemonic (`movaps`, `cvtsi2ss`, `xmm`, `mm0`…) and on `cmov`, `fcomi` and other P6-only
   instructions, if the i486 baseline is to be enforced strictly.
5. **Native unit tests** (`make test`): the portable engine, codecs and updater manifest logic,
   built for the host with AddressSanitizer and UBSan.

### Wine smoke tests

**Wine is not Windows.** These tests catch crashes, missing imports and obvious rendering
breakage quickly. They prove nothing about real Windows 95/98/XP. Wine implements far more API
than Win95, and its version mode only changes reported version numbers and a few behaviours. A
pass under Wine never upgrades a matrix row beyond "Expected".

- Prefixes are created with `winecfg` set to Windows 95, Windows 98, Windows XP and Windows 10
  (`WINEPREFIX=… winecfg -v win95|win98|winxp|win10`).
- Under `xvfb-run`, each test:
  - launches the app with `--selftest` (if implemented) or a scripted open/save of each format;
  - takes a screenshot with `import -window root` and compares the main window against reference
    PNGs at 800×600 and 1024×768;
  - runs the installer with `msiexec /i … /qn` and then uninstalls.
- Record the Wine version with each run.

## Real-machine / VM testing procedure

### Emulators and virtualizers

| Tool | Good for | Notes |
|---|---|---|
| **86Box** or **PCem** | Windows 95 retail/OSR2, 98, 98 SE, ME, NT 4 on period-accurate hardware (486/Pentium, S3/Cirrus/Voodoo video, NE2000/PCnet network) | The most faithful option. Slow is fine. Use for the 95/98 rows. |
| **VirtualBox** | 98 SE (with caveats), ME, NT 4, 2000, XP, Vista, 7, 8, 8.1, 10 | Win9x needs a CPU-speed patch (e.g. the PATCHER9X fix) on fast hosts; no guest additions for 9x. |
| **QEMU** (with or without KVM) | 2000…10; 9x with `-cpu pentium`, `-vga cirrus`, `-device ne2k_pci` or `pcnet` | Scriptable. Good for CI-like batches. |

Use only **licensed Windows installation media and product keys**. Do not commit or redistribute
OS images.

### Per-version test script (record results in the matrix)

1. **Clean install** of the Windows version, at the stated service pack, with default settings at
   800×600 and 256 colors, then a second pass at 1024×768 and 16-bit color. For Windows 95, also
   test 640×480 with 16 colors.
2. **Install via `Setup.exe`**:
   - On 95/98/ME/NT4, confirm that Setup detects the missing or old Windows Installer, runs
     **InstMsiA.exe/InstMsiW.exe**, handles the reboot and resumes, then installs the MSI.
   - On 2000+, confirm that it goes straight to the MSI.
   - Check the Start menu shortcut, the file associations (.psd if chosen) and the uninstall entry
     in Add/Remove Programs.
3. **Launch** from the Start menu.
   - The main window appears maximized, with no missing-import or "not a valid Win32 application"
     dialog.
   - Check the caption gradient (98+) or solid caption (95/NT4) and the menus.
   - Check that the 800×600 layout matches `historical-ui-reference.md` §3.
4. **Open and save each format**: PSD (layered, from the test corpus), BMP (1/4/8/24-bit, RLE),
   JPEG, PNG, GIF, TGA.
   - Re-open each saved file. Compare checksums of pixel data against reference outputs where
     defined.
   - Open a layered PSD saved by PhotoCraft Classic in another editor if available.
5. **Core editing**: new document, brush stroke, marquee plus fill, Levels, Gaussian Blur, undo
   ×20, History palette, Image Size, Canvas Size.
6. **Memory**: open a 2000×2000 image on a 16 MB and a 32 MB configuration (86Box/PCem). There
   must be no crash, and a clear "not enough memory" message if it fails.
7. **Update flow**:
   - With networking (TCP/IP installed) and a **correct clock**, run Help > Check for Updates.
     Verify the TLS connection, the manifest signature check and the download plus install of a
     test update against the staging server.
   - Repeat with a wrong clock and confirm the date/time message.
   - On Windows 95 without TCP/IP, confirm the "network not available" message.
8. **Uninstall** via Add/Remove Programs. All program files and shortcuts must be removed, and the
   per-user settings handled as documented.
9. **Record** the date, build version, emulator/VM and its configuration (CPU, RAM, video, color
   depth), the OS build, the result and any notes in the matrix above. Set the status to
   **Verified** only when steps 2–8 pass.


## Results recorded so far

| Date | Build | Environment | What was exercised | Result |
|---|---|---|---|---|
| 2026-10-30 | 0.1.0 (this branch) | Ubuntu 24.04 + Wine 9.0 (32-bit), Xvfb 1024x768x24, Wine version "Windows 10" | Launch; open PNG and PSD; menus and submenus; Gaussian Blur with preview; Paintbrush strokes; New Layer; Type tool; Free Transform; Levels, Curves, Hue/Saturation, Color Balance, Image Size, New, Preferences, Color Picker dialogs; layered Save As PSD and reopen; marquee selection; "1998 Palettes" workspace | Pass (bugs found during the run were fixed: a Save that could write PSD data into a .png name, dialog group boxes, live stroke redraw) |
| 2026-10-30 | 0.1.0 | Wine 9.0, Xvfb 800x600x16, Wine version "Windows 98" | Launch with a PSD at 800x600 16-bit colour | Pass: Tools panel switches to two columns, Navigator starts collapsed, image fits at 66.7% |
| 2026-10-30 | 0.1.0 | Wine 9.0, Wine version "Windows 95" | Launch; File > New | Pass |
| 2026-10-30 | 0.1.0 | Wine 9.0 | MSI install, upgrade 0.1.0 -> 0.1.1, downgrade refusal, uninstall; Setup.exe wizard and `/S`; updater check, download, signature/hash/replay failures, apply and relaunch (see docs/updater.md) | Pass, except that Wine's msiexec did not roll back a deliberately failed upgrade. Real Windows Installer should; unverified. |

Wine reports a version but implements the modern API set, so these runs cannot catch a Windows 95
API that is missing; the import audit is what guards that.
