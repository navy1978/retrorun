# RG351V DOA2 v10 EmulationStation integration

## Purpose

This integration launches the finalized RG351V build only for:

- `Dead or Alive 2 (Europe)[RDC].cdi`
- `Dead or Alive 2 (USA)[RDC].cdi`

Every other game continues through the firmware RetroRun wrapper and its
configured core.

## Final components

- RetroRun commit: `c0754a7a6f00e197a36a1a1b8538bed9755718f5`
- RetroRun tag: `retrorun-rg351v-v28`
- RetroRun binary SHA-256:
  `9746911c60250ea57bd34cbb6d7af36e76187987cdac876856ee65ef955639cc`
- Flycast commit: `b026813d`
- Flycast tag: `flycast-rg351v-adaptive-v10`
- Flycast core SHA-256:
  `37891bf5159037326631c716a71d0247b2f64906a1c7ee4b46f73eef2f7551a8`
- Runtime configuration: `doa-adaptive-v23-fast-aica.cfg`

The protected device vault is:

```text
/roms/retrorun-test/doa-adaptive-v2/v10-vblank-vault-20260728
```

## Why a bind mount is required

The RG351V firmware mounts `/dev/loop0` as a read-only SquashFS root.
Consequently, `/usr/bin/retrorun.sh` cannot be replaced directly.

The installer creates a patched copy under persistent `/roms`, then bind
mounts it over `/usr/bin/retrorun.sh`. The original firmware file remains
unchanged beneath the mount. The autostart hook restores the bind mount after
boot.

The patch performs an exact platform and ROM basename match before the
firmware wrapper mutates the global RetroRun configuration. Matching DOA2
launches the v28 frontend, v10 core and validated audio environment directly
from the vault.

## Deployment

Copy these files to the same writable directory on the device:

```text
tools/rg351v-doa-v10-install.sh
tools/rg351v-doa-v10-dispatch.sh
tools/rg351v-doa-v10-mount.sh
tools/rg351v-doa-v10-rollback.sh
```

Run the installer on the device:

```sh
chmod 755 rg351v-doa-v10-*.sh
./rg351v-doa-v10-install.sh
```

Persistent production files and firmware backups are stored in:

```text
/roms/retrorun-production
/roms/retrorun-production-backup-20260728
```

## Dry-run check

The dispatcher can be verified without starting the emulator:

```sh
RETRORUN_DOA_V10_DRY_RUN=1 /usr/bin/retrorun.sh \
  flycast2021 \
  "/storage/roms/dreamcast/Dead or Alive 2 (USA)[RDC].cdi" \
  dreamcast
```

Expected output includes `DOA2_V10_SELECTED` and the v28/v10 vault paths.

## Rollback

Run:

```sh
/roms/retrorun-production/rollback-doa-v10.sh
```

Rollback writes the persistent state as `disabled` and unmounts the patched
wrapper. The original firmware wrapper becomes active immediately. The
installer sets the state back to `enabled`.

## Validation status

On 2026-07-28, DOA2 USA launched successfully from EmulationStation and the
user confirmed the result. Reboot persistence testing was deliberately
skipped. Detailed v28 behavior and game observations remain documented in
`docs/RG351V_V28_VALIDATION.md`.
