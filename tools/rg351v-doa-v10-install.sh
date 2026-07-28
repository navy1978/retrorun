#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PRODUCTION=/roms/retrorun-production
BACKUP=/roms/retrorun-production-backup-20260728
AUTOSTART=/storage/.config/autostart.sh
TARGET=/usr/bin/retrorun.sh
VAULT=/roms/retrorun-test/doa-adaptive-v2/v10-vblank-vault-20260728

for required in \
    "$VAULT/retrorun-rg351v-v28" \
    "$VAULT/flycast-rg351v-adaptive-v10.so" \
    "$VAULT/doa-adaptive-v23-fast-aica.cfg" \
    "$SOURCE_DIR/rg351v-doa-v10-dispatch.sh" \
    "$SOURCE_DIR/rg351v-doa-v10-mount.sh" \
    "$SOURCE_DIR/rg351v-doa-v10-rollback.sh"; do
    if [ ! -e "$required" ]; then
        echo "DOA2 v10 installer: missing $required" >&2
        exit 1
    fi
done

mkdir -p "$PRODUCTION" "$BACKUP"

[ -f "$BACKUP/usr-bin-retrorun.sh" ] ||
    cp "$TARGET" "$BACKUP/usr-bin-retrorun.sh"
[ -f "$BACKUP/usr-bin-retrorun" ] ||
    cp /usr/bin/retrorun "$BACKUP/usr-bin-retrorun"
[ -f "$BACKUP/usr-lib-libretro-flycast2021_libretro.so" ] ||
    cp /usr/lib/libretro/flycast2021_libretro.so \
        "$BACKUP/usr-lib-libretro-flycast2021_libretro.so"

cp "$SOURCE_DIR/rg351v-doa-v10-dispatch.sh" \
    "$PRODUCTION/doa-v10-from-es.sh"
cp "$SOURCE_DIR/rg351v-doa-v10-mount.sh" \
    "$PRODUCTION/mount-doa-v10.sh"
cp "$SOURCE_DIR/rg351v-doa-v10-rollback.sh" \
    "$PRODUCTION/rollback-doa-v10.sh"
chmod 755 "$PRODUCTION"/*.sh

if grep -q " $TARGET " /proc/mounts; then
    umount "$TARGET"
fi

awk '
    { print }
    /^RRCONF=/ {
        print ""
        print "# DOA2_V10_DISPATCH_BEGIN"
        print "if [ \"$PLATFORM\" = \"dreamcast\" ]; then"
        print "    case \"$ROM\" in"
        print "        \"Dead or Alive 2 (Europe)[RDC].cdi\"|\"Dead or Alive 2 (USA)[RDC].cdi\")"
        print "            exec /roms/retrorun-production/doa-v10-from-es.sh \"$2\""
        print "            ;;"
        print "    esac"
        print "fi"
        print "# DOA2_V10_DISPATCH_END"
    }
' "$BACKUP/usr-bin-retrorun.sh" > "$PRODUCTION/retrorun.sh.patched"
chmod 755 "$PRODUCTION/retrorun.sh.patched"
printf 'enabled\n' > "$PRODUCTION/DOA_V10_STATE"

if [ ! -f "$AUTOSTART" ]; then
    printf '#!/bin/sh\n' > "$AUTOSTART"
fi
if ! grep -q 'DOA2_V10_AUTOSTART_BEGIN' "$AUTOSTART"; then
    cat >> "$AUTOSTART" <<'EOF'

# DOA2_V10_AUTOSTART_BEGIN
/roms/retrorun-production/mount-doa-v10.sh &
# DOA2_V10_AUTOSTART_END
EOF
fi
chmod 755 "$AUTOSTART"

"$PRODUCTION/mount-doa-v10.sh"

echo "DOA2 v10 routing installed for Europe and USA ROMs"
