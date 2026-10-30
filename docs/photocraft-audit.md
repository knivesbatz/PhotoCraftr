# Audit of upstream PhotoCraft (storytold/photocraft)

> **Audited tree:** `/home/user/ref/photocraft`, commit `b1cfff9d30298eea4f446a8f6a5e4285cb43c6e7`
> (2026-10-30, "Merge pull request #10204"), workspace version **0.17.0**.
> **Licence:** MIT OR Apache-2.0, Copyright (c) 2026 ArtCraft Team and the PhotoCraft
> contributors (`LICENSE-MIT`, `LICENSE-APACHE`, `NOTICE`).
> **Audited for:** PhotoCraft Classic (C/Win32, Windows 95–10). Date: 2026-10-30.

## 1. What upstream is

| Aspect | Evidence |
|---|---|
| Language and toolchain | `Cargo.toml`: `edition = "2024"`, `rust-version = "1.95"`, `resolver = "3"`. `AGENTS.md`: "written in **Rust only**". `unsafe_code = "forbid"` workspace-wide, except `crates/tablet`. |
| Size | `docs/architecture.md` (reviewed 2026-10-10): **25 library crates, 4 apps, xtask, ~539,000 lines of Rust in ~1,310 files, ~8,040 tests, ~700 engine commands, 628 Photoshop menu items mapped**. The current tree has 26 crates (`ipp` added). Measured lines of `.rs` per crate: ui-egui 277,557; engine 161,029; algo 68,123; io 53,512; codecs 29,739; compose 22,402; psd 20,663; text 17,622; raw 16,818; gpu 13,687; paint 10,128 (the rest are under 10k). |
| Dependencies | `Cargo.lock`: **879 packages** (827 from crates.io, 24 git sources, the rest workspace paths). |
| Notable dependencies (locked versions) | eframe 0.37.0, egui 0.37.0, egui-wgpu 0.37.0, egui-winit 0.37.0 (all patched to the `storytold/egui` fork rev `6d518fc`); **wgpu / wgpu-hal / wgpu-core / naga 30.0.1**; **winit 0.30.13** (patched to the `storytold/winit` fork rev `a517b38`); glow 0.17.0; ash 0.38 (Vulkan); accesskit 0.25.1; **rfd 0.17.2**; **rayon 1.12.0**; **windows-sys 0.52.0 / 0.59.0 / 0.61.2**; windows 0.62.2; getrandom 0.2.17 / 0.3.4 / 0.4.3; parking_lot 0.12.5; tokio 1.54.1; rmcp 3.6.0; wasmi 2.0.0; rustls 0.23.46; ring 0.17.14; ureq 3.5.0; reqwest 0.13.5; image 0.25.10; zune-jpeg 0.4/0.5; flate2 1.1.10 + zlib-rs 0.6.8; kurbo 0.11/0.13; vello_cpu 0.2.0; parley 0.11.1; skrifa 0.44.0; harfrust 0.12.0; usvg 0.45.1; chrono 0.4.45; arboard 3.7.0. Shared libraries from `storytold/craft-libs` (craft-ui, craft-print, craft-jpeg, craft-update, craft-docpath) are pinned by git revision. |
| Layering | `AGENTS.md` §2 and `docs/architecture.md` §3: L0 geom/cms/color/raster plus the standalone psd/codecs/raw. L1 doc. L2 ops/paint/algo/text/vector. L3 compose/gpu/format. L4 io/plugins. L5 engine. L6 ui-egui/automation. Apps on top. Enforced by `cargo xtask layers`. |
| Apps | `apps/photocraft` (eframe+wgpu desktop, TCP control server; it also links `photocraft-automation`), `apps/photocraft-cli`, `apps/photocraft-web` and `apps/photocraft-web-boot` (wasm32). |
| UI design (`docs/ui-design.md`) | Themes: Pro (Spectrum-dark `#323232`, accent `#378ef0`), Pro Medium, Studio, **Classic ("Windows-2000 bevels, square corners, navy selection")**, Adwaita, Solarized, Breeze, Soft. Lucide SVG icons (ISC). Inter and JetBrains Mono fonts (OFL). The Essentials dock is Color\|Swatches, Properties\|Adjustments, Layers\|Channels\|Paths (Photoshop CC layout, not CS6). Tabs read `name @ 12.5% (RGB/8)`. |
| Clean-room policy | `AGENTS.md` §3 rule 3 and `docs/architecture.md` §1 principle 8: behaviour studied from Photoshop. "Never copy their code, shaders, profiles or assets". Specs come from the Adobe PSD spec, ISO 32000 and papers. Some PSD details "follow MIT-licensed psd-tools or ag-psd behavior" (`crates/psd/src/lib.rs` header). Blend, levels and color-balance constants were fitted to psd-tools oracle files (`compose/src/psblend.rs`, `adjust/tone.rs`). |

### 1.1 Windows packaging

- `packaging/windows/package.ps1` builds `cargo build --release --locked -p photocraft
  -p photocraft-cli --features heif --target <x86_64|i686|aarch64>-pc-windows-msvc` with a static
  CRT (`-C target-feature=+crt-static`). It then runs **WiX v5** (`wix build`, "WiX v5: dotnet
  tool install --global wix --version 5.0.2").
- `packaging/windows/photocraft.wxs` uses `<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">`
  and `<Package … Scope="perUserOrMachine" …>`, plus the `WixUI_InstallDir` UI, `MajorUpgrade` and
  an `install-policy.wxi` with `VersionNT64` conditions.
- There is no Inno Setup and no NSIS.
- CI (`.github/workflows/ci.yml`, `release.yml`, `windows-arm64.yml`) runs on `windows-latest` /
  `ubuntu-latest` / `macos-15` through `storytold/craft-common-actions`.

## 2. Why the existing stack cannot run on Windows 95–XP

Each item below is a hard blocker on its own.

### 2.1 The Rust toolchain targets Windows 10+

- The Rust blog "Updated baseline standards for Windows targets" (2024-02-26) says the minimum
  for `*-pc-windows-*` targets is **Windows 10** from **Rust 1.78** (2024-05-02): "These
  requirements apply both to the Rust toolchain itself and to binaries produced by Rust". This
  covers x86_64/i686 msvc, gnu and gnullvm.
- The rustc platform-support page (fetched 2026-10-30) lists `i686-pc-windows-msvc` as
  "32-bit MSVC (**Windows 10+, Windows Server 2016+, Pentium 4**)" and the same for
  `i686-pc-windows-gnu`.
  - "Pentium 4" means an SSE2 baseline. Windows 95 never saves XMM state, and the 486/Pentium
    machines it ran on have no SSE at all.
- `i686-win7-windows-msvc` and `x86_64-win7-windows-msvc` are **Tier 3**: "Rust does not ship
  pre-compiled artifacts for this target", so building them needs `build-std`. Even they stop at
  Windows 7.
- Upstream requires **rust-version 1.95** and **edition 2024**, so no pre-1.78 toolchain can build
  it.

### 2.2 `std` imports post-XP APIs at load time

From `library/std/src/sys/pal/windows/c/bindings.txt` and `c.rs` (rust-lang/rust master, fetched
2026-10-30):

| API | Where std uses it | First Windows |
|---|---|---|
| `ProcessPrng` (bcryptprimitives.dll, `raw-dylib`) | `sys/random/windows.rs` (non-win7 targets) | Windows 10 |
| `WaitOnAddress`, `WakeByAddressSingle/All` | futex-based `Mutex`/`Condvar`/`Once`/thread parking | Windows 8 |
| `GetSystemTimePreciseAsFileTime` | `SystemTime::now` (win7 targets fall back to `GetSystemTimeAsFileTime`) | Windows 8 |
| `AcquireSRWLockExclusive/Shared`, `TryAcquireSRWLock*`, `SleepConditionVariableSRW` | sync primitives, `RwLock` | Vista (TryAcquire: 7) |
| `InitOnceBeginInitialize/Complete` | one-time init | Vista |
| `CreateWaitableTimerExW` | high-resolution `thread::sleep` | Vista |
| `GetFinalPathNameByHandleW`, `CreateSymbolicLinkW` | `fs::canonicalize`, `fs::soft_link` | Vista |
| `SetThreadDescription` | thread names (looked up at run time) | Win10 1607 |

- Nearly everything else is `…W` (Unicode). Windows 95/98/ME implement almost no wide-character
  APIs without MSLU (`unicows.dll`).
- A missing import makes the Win9x/NT4/2000/XP loader refuse the EXE before `main`.
- Rust linkers also write PE subsystem version **6.0** by default. XP accepts at most 5.1/5.2, and
  9x/NT4 expect 4.0.

### 2.3 The MSVC static CRT

- `+crt-static` links the Universal CRT statically from current Visual Studio. The last toolset
  that targeted XP was `v141_xp` (VS2017). Current UCRT objects import Vista+/Win7+ APIs.

### 2.4 wgpu 30 has no rendering path on 1998–2001 hardware

- The wgpu README backend table: **Vulkan**, **DX12**, **OpenGL 3.3+ via WGL** (or ANGLE, GLES 3).
  There is no Direct3D 9/11 backend.
- DX12 needs Windows 10. Vulkan drivers exist from Windows 7 on. OpenGL 3.3 needs 2010-era
  GPUs and drivers.
- Win95-era systems have, at best, OpenGL 1.1 (the `opengl32.dll` software ICD) or DirectDraw.
- `apps/photocraft/Cargo.toml` enables eframe with `features = ["wgpu", …]`, and `crates/gpu` is
  a wgpu compositor.

### 2.5 egui/eframe 0.37

- egui is an immediate-mode GUI that tessellates to triangles and needs a GPU backend:
  `egui-wgpu`, or `egui_glow` (OpenGL 2+/GLES2 through glutin and WGL extensions).
- Its fonts are TTF rasterized to a texture atlas. That gives antialiased text, which is neither
  period-appropriate nor available without a GPU.
- Neither backend can run on the targets (§2.4).

### 2.6 winit 0.30.13 (Windows backend, v0.30.x branch)

- **Static imports**:
  - `src/platform_impl/windows/window.rs` line 14: `DwmEnableBlurBehindWindow`,
    `DwmSetWindowAttribute` from dwmapi.dll, which first shipped with Vista.
  - Line 32: `RegisterTouchWindow` (Windows 7).
  - `TrackMouseEvent` in `event_loop.rs` (Windows 98/NT4; **not** Windows 95, where only
    comctl32's `_TrackMouseEvent` exists).
  - COM `CoInitializeEx`, OLE drag and drop.
- **Dynamic look-ups that degrade gracefully** (`util.rs` lines 257–280): `GetDpiForWindow`,
  `AdjustWindowRectExForDpi`, `GetDpiForMonitor`, `EnableNonClientDpiScaling`,
  `SetProcessDpiAwarenessContext`, `SetProcessDpiAwareness`, `SetProcessDPIAware`,
  `GetPointerFrameInfoHistory`, `SkipPointerFrameMessages`, `GetPointerDeviceRects`,
  `GetPointerTouchInfo`, `GetPointerPenInfo`. All are Vista/8/10 APIs.
- **Wide-character window classes** throughout. Upstream additionally patches winit to its own
  fork.

### 2.7 rfd 0.17

- The Windows backend uses the COM `IFileOpenDialog`/`IFileSaveDialog` interfaces (Vista+). This
  is from my knowledge of rfd's `win_cid` backend; it was not re-verified in this session, because
  github.com pages were not reachable here.

### 2.8 `crates/tablet`

- `src/windows.rs` declares `extern "system" fn GetPointerPenInfo(...)` (line 73) and hooks
  `WM_POINTER*` messages (lines 77–78). These are Windows 8 APIs, imported statically.

### 2.9 Networking and async

- `photocraft-automation` brings in tokio 1.54 and rmcp. On Windows, mio uses AFD/IOCP with
  `GetQueuedCompletionStatusEx` and `SetFileCompletionNotificationModes` (Vista+).
- getrandom 0.3/0.4 on Windows calls `ProcessPrng`, as std does. ring/rustls use the same RNG
  path.

### 2.10 Installer

- WiX v4/v5 packages default to `InstallerVersion = 500`, and `Scope="perUserOrMachine"` is a
  Windows Installer 5.0 feature (Windows 7+).
- Windows 95/98/ME/NT4 can have at most Windows Installer **2.0** (InstMsiA/InstMsiW). Windows
  2000 has up to 3.1, and XP 3.1 (4.5 by redistributable).

### 2.11 Resources

- The build is 539k lines, with GPU texture residency, 256² `Arc` COW tiles, rayon thread pools,
  8/16/32-bit pipelines and ICC transforms.
- Typical 1998 machines had a 486/Pentium at 33–200 MHz, 8–64 MB of RAM and 1–4 MB of VRAM.

### 2.12 rust9x

rust9x (https://github.com/rust9x/rust, docs at https://github.com/rust9x/docs) is a **third-party
fork of Rust's standard library** for 9x, ME and NT 3.1 SP2+.

- **Targets**: `i486-`, `i586-`, `i686-` and `x86_64-rust9x-windows-msvc`. Precompiled
  toolchains are offered from `rust9x-1.98.0-beta-v4` (README, fetched 2026-10-30).
- **Feasibility for us**: technically possible for a small program, as its sample app shows on
  Windows 95 RTM with WinSock2, on 95 B and on NT 3.51/4.0. But its own docs say:
  - "**Only the standard library is ported. Crates that directly use Windows APIs will need to be
    patched**". That rules out winit, wgpu, egui, rfd, tokio/mio, getrandom, windows-sys users,
    arboard and the tablet crate, which is most of upstream's dependency graph.
  - "this is not stable software at all".
  - 9x/ME need `unicows.dll`.
  - The 9x system allocator "fails spectacularly" under fragmentation, so `dlmalloc` is needed.
  - Floating point on non-SSE2 (`i586`) targets "is generally unsound" (rust-lang/rust#114479).
    That is critical for an image editor on 486/Pentium.
  - C99 math intrinsics need "hacky workarounds" (`limitations.md`).
  - A 2026 release note (seen only in a search snippet, not verified) said the build "currently
    needs Windows 98/NT4 or higher". That conflicts with the README's claim of 9x support.
- **Why we did not choose it**:
  1. The user chose a legacy-first C implementation.
  2. Porting the GUI, GPU and async stack would be a rewrite anyway, so keeping Rust buys little.
  3. We need exact control of the import table, the PE header and x87 code generation. That is
     simpler with mingw-w64 C and our own C runtime (`src/rt/pcrt.c`, `Makefile`:
     `-march=i486 -mno-sse -mfpmath=387`, `--subsystem windows:4.0`, `-nostdlib`).
  4. It would depend on a one-maintainer fork tracking nightly Rust.

## 3. What we port (reimplement in C, with attribution) and what we replace

Licensing applies to every row marked "port":

- Upstream is MIT OR Apache-2.0, and PhotoCraft Classic is dual-licensed under the same terms
  (`LICENSE-MIT`, `LICENSE-APACHE` in this repo; the Apache text is identical to upstream's).
- A reimplementation that follows upstream's code closely is a derivative work. Each C file
  carries the header
  `/* Portions derived from PhotoCraft <upstream path>, Copyright (c) 2026 ArtCraft Team and the PhotoCraft contributors, MIT OR Apache-2.0. Modified for PhotoCraft Classic. */`.
  That satisfies MIT (keep the notice) and Apache-2.0 §4(b) (state the changes) and §4(c) (keep
  notices).
- Our `NOTICE` reproduces the relevant upstream NOTICE line (§4(d)). Upstream NOTICE items for
  assets we do not ship (Inter, JetBrains Mono, Lucide, SCOWL, libfacedetection, emoji-icon-font)
  are excluded, as §4(d) permits for notices "that do not pertain to any part of the Derivative
  Works".
- Do not use the ArtCraft names or logos (`docs/brand/LICENSE-brand.txt`; they are not open
  source).

The module names under "Our C module" are **proposed**.

| Area | Upstream path(s) | What it does | Our C module | Notes |
|---|---|---|---|---|
| Blend-mode math | `crates/color/src/blend.rs` (`BlendMode`, `blend_channel`, `lum`/`set_lum`/`set_sat`, `composite`), `crates/compose/src/psblend.rs` (Photoshop-exact Vivid Light, Hard Mix, Dissolve, `composite`) | Separable and non-separable modes (ISO 32000 / W3C formulas plus Photoshop variants), source-over with opacity | `src/core/blend.c` | **Port** the PS5 modes only: Normal, Dissolve, Behind, Clear, Multiply, Screen, Overlay, Soft Light, Hard Light, Color Dodge, Color Burn, Darken, Lighten, Difference, Exclusion, Hue, Saturation, Color, Luminosity. Integer 8-bit with 0–255 fixed point; keep the float reference in tests. Drop the CS6-only modes (Linear Burn/Dodge, Vivid/Linear/Pin Light, Hard Mix, Darker/Lighter Color, Subtract, Divide) or put them behind a flag. |
| Levels | `crates/compose/src/adjust/tone.rs` (`levels_q`, `levels_gamma` with the fitted toe `LEVELS_TOE_POWER = 2.2`, `tone_luts`) | Input/output range and gamma, whole-level rounding that matches Photoshop | `src/core/adjust.c` (`pc_levels_lut`) | **Port** as 256-entry LUTs per channel. Keep the documented rounding. |
| Curves | `tone.rs` (`curve_lut`, `curve_map_lut`, `curve_lut_mode` for pencil/arbitrary) | Spline through points → LUT | `adjust.c` (`pc_curves_lut`) | **Port**. Up to 16 points (PS5). Arbitrary map = pencil. |
| Hue/Saturation | `crates/compose/src/adjust/color.rs` (`hue_saturation`, `hue_range_tables`, `colorize_saturation`, `rgb_to_hsl`/`hsl_to_rgb`) | Master and six ranges with fall-off; Colorize | `adjust.c` | **Port**, 8-bit only (drop the HDR branch). |
| Color Balance | `color.rs` (`color_balance_channel`, `BalanceCurve`) | Shadows/midtones/highlights with Preserve Luminosity | `adjust.c` | **Port**. |
| Brightness/Contrast | `crates/compose/src/adjust.rs` (`Adjustment::BrightnessContrast { legacy: true }` → `adjust/channels.rs legacy_brightness_contrast`), `tone.rs` (`modern_*` for the CS3+ mode) | Legacy linear (PS5) and modern curves | `adjust.c` | **Port the legacy path only** (PS5 behaviour). |
| Invert, Posterize, Threshold, Desaturate, Equalize, Channel Mixer | `tone.rs` (`posterize`), `adjust.rs`, `algo/src/histogram.rs` | Per-pixel and histogram ops | `adjust.c`, `src/core/histogram.c` | **Port**. Simple. |
| Gaussian blur | `crates/algo/src/blur.rs` (`gaussian_kernel`: σ = radius, 3σ taps; `gaussian_blur_kernel` with a variance fix below r=1; `conv_sep`; `boxes_for_gauss` / `box_blur` for large radii), `algo/src/separable.rs` | Separable convolution, premultiplied alpha, box-blur approximation for large σ | `src/core/filter_blur.c` | **Port**: integer-kernel separable convolution for r ≤ ~8, three-pass box blur beyond. Fixed point (no SSE). |
| Unsharp Mask | `crates/algo/src/sharpen.rs` (`unsharp` → `sharpen/exact.rs`; `with_blur`), `docs/unsharp-mask-performance.md` | Amount/radius/threshold on top of the Gaussian | `src/core/filter_sharpen.c` | **Port**. Skip the FFT path (`sharpen/fft.rs`). |
| Median, Despeckle, Dust & Scratches, Add Noise | `crates/algo/src/noise.rs` (`median`, `median_ranked` histogram method, `median_direct`, `disc_rows`, `hash01`, `add`) | Disc-shaped median with an optional threshold; deterministic noise | `src/core/filter_noise.c` | **Port** the histogram-sliding median (Huang) and hash-based noise. Drop `denoise.rs` (guided filter; that is Reduce Noise, CS2+). |
| Other PS5 filters (Find Edges, Emboss, Diffuse, Wind, Mosaic, Spherize, Twirl, Pinch, High Pass, Max/Min, Offset, Clouds) | `algo/src/stylize.rs`, `stylize2/*.rs`, `distort.rs`, `pixelate*.rs`, `render.rs`, `sharpen.rs high_pass`, `other.rs` | Classic filters | `src/core/filter_*.c` | **Port selectively**, after the core set. |
| Resampling | `crates/algo/src/resample.rs` (`Resample::{Nearest, Bilinear, Bicubic, …}`, Keys cubic, 1/128-pixel phases, box averaging when reducing below half) | Image Size and transforms | `src/core/resample.c` | **Port** Nearest, Bilinear and Bicubic (Keys a=−0.5; Photoshop uses −0.75 in places; follow upstream's constants). Drop Lanczos, Preserve Details and the Smoother/Sharper variants (CS). |
| Free Transform | `crates/algo/src/transform.rs` (`Homography`, `quad_to_rect`, `warp_surface` with inverse mapping and premultiplied sampling, pre-reduction) | Scale, rotate, skew, distort, perspective | `src/core/transform.c` | **Port**: an 8-bit, tiled inverse warp. |
| Selections | `crates/algo/src/selection.rs` (`magic_wand`, `wand_region`, `combine`, `SelectionMode`, `color_range`), `selection/marquee.rs` (`ellipse_surface`), `selection/lasso.rs` (`polygon_surface`), `selection/morphology.rs`, `selection_blur.rs` (feather) | Coverage masks; wand tolerance as a per-channel difference ≤ tol levels (alpha included); 4-connected contiguous fill; expand, contract, border, smooth, feather | `src/core/select.c` | **Port**. The mask is 8-bit 0–255 per pixel. Contiguous = 4-connected scanline fill (upstream uses a stack; we use a scanline span fill to bound memory). |
| Magnetic Lasso | `crates/algo/src/magnetic.rs` (live wire, Mortensen & Barrett 1995) | Edge snapping | `src/core/magnetic.c` | **Port later** (v1.1). |
| Paint Bucket and Gradient | `crates/algo/src/paint.rs` (`bucket_fill`, `bucket_fill_src` for patterns, gradient `t` for linear/radial/angle/reflected/diamond) | Fill a tolerance region; gradient tool | `src/core/fill.c` | **Port**. Same tolerance semantics as the wand. |
| PSD reader/writer | `crates/psd/src/` — `header.rs` (signature `8BPS`, v1/v2, channels, depth, mode), `resources.rs` (image resource blocks, `8BIM`), `layer.rs` (layer records: rect, channel info, blend key, opacity, clipping, flags, mask data, blending ranges, Pascal name padded to 4; additional tagged blocks such as `luni`, `lsct`), `tagged.rs`, `compression.rs` (Raw / **RLE PackBits** with a per-row byte-count table / ZIP / ZIP-with-prediction), `rows.rs`, `image_data.rs` (merged image), `builder.rs`, `lazy.rs`, `io.rs` | A lossless PSD/PSB parser and writer that keeps unknown blocks raw | `src/io/psd_read.c`, `src/io/psd_write.c`, `src/io/packbits.c` | **Port the structure**: v1 only (no PSB), 8-bit, RGB/Gray/Indexed/Bitmap, Raw and RLE. Read ZIP only if zlib is added; v1 skips ZIP (Photoshop writes ZIP only for 16-bit layers). Write the merged composite (Maximize Compatibility). Keep unknown tagged blocks verbatim when re-saving. Keep upstream's limits (dimensions ≤ 30000 for PSD; bounded allocations) and its never-panic discipline as **no-crash checks in C**. Effects (`lrFX`, PS5) → `io/effects_map.rs` is a reference. |
| Doc ⇄ PSD mapping | `crates/io/src/psd_import*.rs`, `psd_export*.rs`, `effects_map*`, `channel_map.rs`, `guides_map.rs` | Document model ↔ PSD | `src/io/psd_doc.c` | **Port selectively** (layers, masks, alpha channels, guides, resolution, effects lrFX). |
| Brush dab model | `crates/paint/src/brush.rs` (`BrushSettings`, `TipShape`, spacing default 0.25, hardness), `dynamics.rs` (`PathWalker` spacing walker, `DabBuilder`), `render.rs` (dab coverage profile `TipProfile`, "build-up up to stroke opacity" accumulation: Opacity vs Flow), `render/reference.rs` | Computed round tips with hardness, roundness and angle; spacing along the path; stroke buffer composited once at stroke opacity | `src/core/brush.c` | **Port** the computed-tip model (diameter, hardness, spacing, angle, roundness, the PS5 Brush Options), the path walker and the stroke-buffer semantics. PS5 *Fade: N steps* maps to upstream `Control::Fade`. Drop dynamics (Shape/Scatter/Texture/Dual/Color/Transfer/Pose), mixer, symmetry and procedural tips (PS7+). Airbrush = build-up over time. Wet Edges: port if simple (PS5 had it). |
| History | `crates/ops/src/lib.rs` (`History`, `HistoryState`, `record`, `undo`/`redo`, `max_states`, byte budget, `set_brush_source`, `remove`, `purge_last`), `crates/raster` (COW 256² tiles) | One whole-document snapshot per step, made cheap by `Arc` tile sharing; History-brush source | `src/core/history.c`, `src/core/tiles.c` | **Port the model**: a snapshot per state over reference-counted **64×64** tiles (smaller tiles suit 16 MB machines), a state limit of 20 (PS5 default), a memory budget, non-linear history and snapshots. |
| Layer compositing | `crates/compose/src/composite.rs`, `masks.rs`, `effects/*.rs` (drop shadow, glow, bevel) | CPU compositor (the oracle) | `src/core/compose.c`, `src/core/effects.c` | **Port the 8-bit CPU path**: tiles, masks, clipping groups, Blend If ranges, and PS5's five effects. Drop the GPU path, CMYK and Lab. |
| Color-picker math | `docs/color-picker.md`, `crates/color/src/convert.rs` | HSB, RGB, Lab conversions | `src/core/color.c` | **Port** the sRGB ↔ HSB/Lab formulas (fixed D65, no ICC). |
| File codecs (flat) | `crates/codecs` (image, zune-jpeg, own codecs) | Many formats at many depths | `third_party/stb` (`stb_image.h` v2.30 decode; `stb_image_write.h` v1.16 encode) plus `src/io/bmp.c` (RLE BMP and the OS/2 variant), `src/io/gif.c` (writer, LZW) | **Replace.** |
| Text | `crates/text` (parley, skrifa, harfrust shaping, craft-fonts) | Unicode shaping and layout | `src/core/type.c` using GDI `GetGlyphOutline` (8-bit gray outlines) and `ExtTextOut` metrics | **Replace** with the PS5 Type dialog model (one style per layer, as in PS5). |
| Vector paths | `crates/vector` (kurbo, vello_cpu), `algo/src/trace.rs` | Bézier paths → coverage | `src/core/path.c` | **Replace** with our own: Bézier flattening plus a scanline (non-zero/even-odd) rasterizer with 4×4 supersampled AA. Implement from the textbook algorithm; upstream is used only as behaviour reference. |
| UI | `crates/ui-egui` (277k lines), `menu_catalog.rs`, `docs/ui-design.md`, `docs/ui-dialogs.md`, `docs/context-menu-inventory.md` | egui shell | `src/ui/*.c` (GDI, custom-drawn) | **Replace.** Use `menu_catalog.rs` and the context-menu inventory **only as a checklist** of Photoshop command names. |
| Engine and commands | `crates/engine` (`CommandSpec`, registry, ~700 commands) | Command dispatch | `src/core/cmd.c` (a static command table: id, label, menu path, accelerator, enabled-fn, run-fn) | **Re-design** the idea at a smaller scale. |
| Update check | `craft-update` (git dependency) | Release checks over HTTPS (rustls) | `src/updater/` (BearSSL TLS 1.2, Monocypher Ed25519-signed manifest, WinSock 1.1 `wsock32`) | **Replace.** |

## 4. What we drop, and why

The yardstick is the feature set of 1998 plus the constraints of Windows 95: a 486/Pentium,
8–64 MB of RAM, no GPU API beyond GDI and DirectDraw, no Unicode, and Windows Installer ≤ 2.0.

| Dropped | Upstream location | Why |
|---|---|---|
| GPU compositor and display conversion | `crates/gpu` (wgpu, naga/WGSL), `docs/gpu-composite-reuse.md` | No GPU compute or shaders on the targets. GDI `StretchDIBits`/`SetDIBitsToDevice` from 8-bit tiles is enough. |
| Web build | `apps/photocraft-web`, `apps/photocraft-web-boot`, `packaging/web`, `[profile.wasm-release]` | Not a target. |
| MCP and automation server, TCP control channel | `crates/automation` (rmcp, tokio), `docs/mcp.md`, `docs/control-protocol.md`, the `--control` server in `apps/photocraft` | 2020s agent tooling. tokio/mio need Vista+. It would also enlarge the attack surface on unpatched OSes. A future headless CLI could reuse the command table. |
| Plug-ins | `crates/plugins` (wasmi WebAssembly sandbox) | Not period-correct (PS5 used native 8BF plug-ins, which we don't host either). wasmi needs std features unavailable on Win9x. |
| CMYK, Lab, Duotone, Multichannel, 16 and 32 bit | `crates/cms` (ICC engine), `compose/src/cmyk*.rs`, `native_lab.rs`, `raster` `PixelFormat` depths | Memory and simplicity; no ICM on Win95. See D11. |
| Camera RAW, HEIF, Affinity, KRA/ORA/PDN/XCF, PDF/SVG import | `crates/raw`, `crates/heif`, `crates/affinity`, `io/src/{kra,ora,pdn,xcf,pdf,svg}` | Post-1998 formats (RAW/DNG 2004, HEIF 2015); large decoders. |
| Native `.pcraft` format | `crates/format` (zstd tiles) | PSD is our native format. |
| Smart Objects and Smart Filters, linked layers, layer comps | `io/src/smart_map*`, `doc`, `docs/smart-filters.md` | CS2+/CS3+ features. |
| 3D | none upstream (Photoshop's 3D menu) | See D6. |
| AI and content-aware features | `algo/src/content_aware.rs`, `inpaint*`, `remove.rs`, `segment/*` (GrabCut, SLIC, sky, subject, max-flow), `matting*`, `faces*` (libfacedetection CNN weights, BSD-3: not carried), `seam.rs` (content-aware scale), `quick-selection` docs | Not in the 1998 feature set. Compute and memory are far beyond the targets. The user asked for no AI. |
| Warps and CS-era filters | `liquify.rs`, `puppet.rs`, `vanishing.rs`, `panorama*`, `hdr.rs`, `camera_raw*`, `lens.rs`, `wideangle.rs`, `relight.rs`, `oil.rs`, `blur2.rs` (iris/tilt-shift), `denoise.rs` | CS-era. Heavy. |
| Brush dynamics, mixer brush, symmetry, ABR v6+ | `paint/src/{dynamics,mixer,symmetry,procedural}.rs` | PS7+ features. We keep PS5 tips and options only. |
| Printing via IPP/CUPS, craft-print | `crates/ipp`, `craft-print` | Windows printing goes through GDI `StartDoc`/`StretchDIBits`. |
| Tablet crate | `crates/tablet` | Win8 pointer API. Wintab support could be added later via a dynamically loaded `wintab32.dll`, which is period-correct. |
| Localization (13 TSV languages), accessibility via AccessKit | `ui-egui/src/i18n`, accesskit | v1 is English-only. Strings will be in resources so they can be localized later with `LoadStringA` (ANSI code pages on 9x). |

## 5. Summary

| Component | Reuse strategy | Status (2026-10-30) |
|---|---|---|
| Blend modes (`color/blend.rs`, `compose/psblend.rs`) | Port the PS5 subset to 8-bit fixed point; float reference in tests | Planned |
| Levels / Curves / Hue-Sat / Color Balance / Brightness-Contrast (legacy) / Invert / Posterize / Threshold / Desaturate / Equalize / Channel Mixer (`compose/adjust/*`) | Port as LUT and per-pixel code | Planned |
| Gaussian blur, Unsharp Mask, Median, Add Noise, Dust & Scratches (`algo/blur.rs`, `sharpen.rs`, `noise.rs`) | Port (separable integer kernels, box-blur approximation, Huang median) | Planned |
| Other PS5 filters (`algo/stylize*`, `distort*`, `pixelate*`, `render*`, `other*`) | Port selectively | Later |
| Resampling (`algo/resample.rs`) | Port Nearest, Bilinear, Bicubic | Planned |
| Free Transform (`algo/transform.rs`) | Port the 8-bit inverse warp | Planned |
| Selections, wand, bucket, gradient (`algo/selection*`, `algo/paint.rs`) | Port | Planned |
| Magnetic Lasso / Magnetic Pen (`algo/magnetic.rs`) | Port | Later (v1.1) |
| PSD (`crates/psd`, `io/psd_*`) | Port the structure (v1, 8-bit, Raw/RLE, merged image, lrFX) | Planned |
| Brush engine (`paint/brush.rs`, `dynamics.rs PathWalker`, `render.rs`) | Port the computed-tip and spacing model plus stroke-buffer semantics; drop dynamics | Planned |
| History (`ops`), COW tiles (`raster`) | Port the model (snapshots over refcounted 64×64 tiles) | Planned |
| Compositor and layer effects (`compose`) | Port the 8-bit CPU path and the five PS5 effects | Planned |
| Flat codecs (`codecs`) | Replace with stb_image 2.30 / stb_image_write 1.16 plus our own BMP/GIF | Vendored (`third_party/stb`) |
| Text (`text`) | Replace with GDI glyph outlines | Planned |
| Vector (`vector`) | Replace with our own flattening and scanline rasterizer | Planned |
| UI (`ui-egui`) | Replace with custom GDI (see `historical-ui-reference.md`) | Spec written |
| Engine and command registry (`engine`) | Re-design as a small static command table | Planned |
| Update check (`craft-update`) | Replace with BearSSL plus Monocypher | Vendored (`third_party/bearssl`, `third_party/monocypher`) |
| Installer (WiX v5 MSI, Windows Installer 5) | Replace with an MSI built for Windows Installer 2.0 (`wixl`/msitools) plus a `Setup.exe` bootstrapper that runs InstMsiA/InstMsiW | Makefile targets present |
| GPU, web, MCP and automation, plug-ins, CMYK/Lab/16/32, RAW/HEIF/Affinity, smart objects, AI/content-aware, IPP, tablet, i18n | Drop | — |
