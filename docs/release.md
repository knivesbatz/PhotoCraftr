# Releasing PhotoCraft Classic

This page covers how to cut a release, how CI builds and publishes it, and
how to create, store and rotate the update signing key. For the updater's
design see [updater.md](updater.md).

## What a release contains

`make dist` writes these files to `dist/`. CI uploads all of them to the
GitHub Release.

| Asset | Purpose |
|---|---|
| `PhotoCraftClassic-Setup.msi` | Windows Installer package (Installer 1.1+, major upgrade) |
| `Setup.exe` | Wizard and bootstrapper: installs Windows Installer if needed, then runs the MSI. `Setup.exe /S` is silent. |
| `PhotoCraftClassic.exe` | Standalone program; runs without installing |
| `PCUpdate.exe` | Updater, for reference. It is also inside the MSI. |
| `RELEASE-NOTES.txt` | Release notes (CRLF) |
| `SHA256SUMS.txt` | SHA-256 of every other asset |
| `update-manifest.txt` | **Signed** update manifest |

The asset names are part of the update protocol, so do not rename them:

- PCUpdate.exe downloads
  `https://github.com/knivesbatz/PhotoCraftr/releases/latest/download/update-manifest.txt`;
- the manifest points at
  `https://github.com/knivesbatz/PhotoCraftr/releases/download/v<version>/PhotoCraftClassic-Setup.msi`.

`releases/latest` resolves to the newest **non-prerelease** release, so do
not publish a broken build as a normal release.

Windows 95/98/Me/NT4 users without Windows Installer also need Microsoft's
redistributables next to `Setup.exe`: `InstMsiA.exe` for 9x and
`InstMsiW.exe` for NT4. Microsoft no longer distributes them, so they are
not release assets. Setup.exe explains where to put them.

## Cutting a release

1. Update `VERSION`. The format is `MAJOR.MINOR.PATCH`: major and minor are
   below 256 and patch is below 65536 (the MSI limits).
2. Write the release notes in `docs/release-notes.md`. If it exists, the
   build uses it; otherwise `CHANGELOG.md`. Keep the newest release at the
   top. The manifest carries at most about 60 lines or 4 KB of it.
3. Check that `src/updater/pubkey.h` pins the **release** public key and not
   the test key. CI refuses to release while `PC_UPDATE_PUBKEY_IS_TEST_KEY`
   is 1.
4. If the trust anchors may be stale, check that the roots in `tools/trust/`
   still cover github.com and *.githubusercontent.com (see
   [Trust anchors](#trust-anchors)).
5. Build locally, either unsigned:

   ```
   make test && make check && make msi && make dist
   ```

   or signed with the release key from an offline machine:

   ```
   PC_UPDATE_SIGNING_KEY=$(cat /secure/pc-update-key.txt) make dist
   ```

6. Commit, then tag and push:

   ```
   git tag -a v0.2.0 -m "PhotoCraft Classic 0.2.0"
   git push origin v0.2.0
   ```

7. CI (`.github/workflows/build.yml`) then:
   - checks that the tag equals `v$(cat VERSION)`;
   - runs `make test`;
   - runs `make dist` with the secret `PC_UPDATE_SIGNING_KEY`;
   - runs `gh release create v0.2.0` with all the assets above.

   `make dist` refuses to sign if the key's public half is not pinned in
   `pubkey.h`.
8. Smoke test: on an older install, run Start > PhotoCraft Classic > Check
   for Updates.

### Sequence numbers

Every manifest has a `sequence`. The updater rejects manifests whose
sequence is lower than one it has already seen. `tools/make-manifest.sh`
uses the build time (`date -u +%s`), which only increases. To re-issue a
manifest for the same release (for example to fix the notes), just build it
again: the sequence goes up. Do not set `PC_UPDATE_SEQUENCE` by hand unless
the new value is higher than every published one.

### Re-signing an existing release by hand

```
make build/native/pcsign
tools/make-manifest.sh dist/PhotoCraftClassic-Setup.msi 0.2.0 docs/release-notes.md > m.txt
build/native/pcsign sign /secure/pc-update-key.txt m.txt > update-manifest.txt
build/native/pcsign verify <public-key-hex> update-manifest.txt
gh release upload v0.2.0 update-manifest.txt --clobber
```

## Signing key

The key is Ed25519. The secret is a 32-byte seed, stored as 64 hex digits.
The public key (32 bytes) is compiled into PCUpdate.exe through
`src/updater/pubkey.h`.

### Generate (once)

Do this on an offline or trusted machine:

```
make build/native/pcsign
build/native/pcsign keygen pc-update-key.txt pc-update-key.pub
```

`keygen` refuses to overwrite an existing file and creates the secret with
mode 0600. It prints the public key in hex and as a C array.

1. Put the C array into `src/updater/pubkey.h`, replacing the test key, and
   set `PC_UPDATE_PUBKEY_IS_TEST_KEY` to 0.
2. Update the vector in `tests/test_updater.c` if you want the
   pcsign-compatibility test to keep running with the real key. Otherwise it
   is skipped automatically.
3. Store `pc-update-key.txt` offline, for example on two encrypted USB
   sticks in different places. Anyone who has this file can push code to
   every user.
4. `pc-update-key.pub` is public; commit or publish it.

`build/native/pcsign pubkey <secret>` prints the public key of an existing
secret.

### Set the CI secret

The release job reads the secret `PC_UPDATE_SIGNING_KEY`: the 64 hex digits
of the seed, with no newline. A GitHub
[environment](https://docs.github.com/actions/deployment/targeting-different-environments)
named `release` is recommended, restricted to `v*` tags and with required
reviewers:

```
gh secret set PC_UPDATE_SIGNING_KEY --env release < pc-update-key.txt
# or, as a plain repository secret:
gh secret set PC_UPDATE_SIGNING_KEY < pc-update-key.txt
```

(GitHub UI: Settings > Environments > release > Add secret, or
Settings > Secrets and variables > Actions.)

If you prefer never to give CI the key, leave the secret unset. CI then
fails at the signing step. Build `dist/` locally, sign offline, and upload
with `gh release create`/`gh release upload`.

### Rotate

Several public keys can be pinned at once; a manifest is accepted if any of
them verifies it.

1. Generate a new key pair.
2. Add the new public key to `pc_update_pubkeys[]` in `src/updater/pubkey.h`,
   **keeping the old one**. Release this version, signed with the **old**
   key, so every updater can still verify it.
3. Wait until most users run the new version, then switch
   `PC_UPDATE_SIGNING_KEY` to the new key.
4. In a later release, remove the old key from `pubkey.h`.

If the old key leaked, do steps 1 to 4 immediately, and treat any manifest
signed with the old key after the leak as hostile. Users on versions that
only trust the leaked key cannot be protected remotely; announce the
incident and ask them to reinstall from the releases page.

## Trust anchors

The updater uses only the roots compiled in from `tools/trust/*.pem`. To
change them:

```
# needs BearSSL's brssl: BRSSL=/path/to/brssl, or the script clones and builds it
tools/trust/regen-trust-anchors.sh            # rewrites src/updater/trust_anchors.c
```

Edit the PEM list inside the script when adding or removing a file. To find
out which roots GitHub uses, look at its current certificates in
Certificate Transparency (for example crt.sh for github.com and
%.githubusercontent.com). Then follow the issuers to the roots.
`openssl s_client -showcerts -connect github.com:443` also works from a
network without TLS interception.

## Windows Installer package notes

- **UpgradeCode** is `{499A4A81-3992-4273-A28A-4C80B21994C2}`. It is fixed
  forever; changing it breaks upgrades.
- ProductCode is new for every build. Every build therefore upgrades older
  and same-version builds and refuses to downgrade ("A newer version ... is
  already installed").
- Per-machine install (`ALLUSERS=1`): Program Files and HKLM.
  - Windows 9x without profiles: there is only one "machine" anyway.
  - NT: needs an administrator. Vista and later show a UAC prompt.

  Per-user installs are not offered, because Windows Installer 1.1 on 9x
  has no meaningful per-user install location.
- Public properties you can pass:
  - `INSTALLDIR=<folder>`;
  - `DESKTOPSHORTCUT=1|0` (default 1);
  - `EXPLICITOPTIONS=1`: use the two values above even on an upgrade, where
    the previous choices are kept otherwise.
- `installer/build-msi.sh` post-processes the wixl output and fails the build
  unless:
  - the page count is 110;
  - there is no MsiFileHash and no other post-1.1 table or action;
  - every File, Directory and Shortcut has an 8.3 short name;
  - the Shortcut and CustomAction tables have the 1.1 column set;
  - RemoveExistingProducts runs after InstallInitialize.
- File associations add "Open with" entries only: the
  `PhotoCraftClassic.Image` ProgID, `OpenWithProgids`, `OpenWithList` and
  `Applications\PhotoCraftClassic.exe\SupportedTypes` for .psd .bmp .gif
  .jpg .jpeg .png .tif .tiff .tga. The default program of an extension is
  never changed.

## What CI does on every push

`make test` (native, ASan/UBSan), `make check` (cross build plus Windows 95
import audit), `make msi`, and `make dist` with an unsigned manifest. The
`dist/` folder is uploaded as a workflow artifact.
