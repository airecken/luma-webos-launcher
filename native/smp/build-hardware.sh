#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
if [ -z "${WEBOS_NDK:-}" ]; then WEBOS_NDK="$ROOT/.build/toolchain/arm-webos-linux-gnueabi_sdk-buildroot"; fi
SYSROOT="$WEBOS_NDK/arm-webos-linux-gnueabi/sysroot"
CC="$WEBOS_NDK/bin/arm-webos-linux-gnueabi-gcc"
OUT="$ROOT/.build/hardware-wallpaper"
mkdir -p "$OUT"
# Personal C9 profile defaults to the pre-webOS-5 DirectMedia API.
# Set LUMA_NDL_API=2 to explicitly build the upstream C1 backend.
case "${LUMA_NDL_API:-1}" in
  1) PLAYER_SOURCE="$ROOT/native/smp/luma_ndl_c9.c" ;;
  2) PLAYER_SOURCE="$ROOT/native/smp/luma_ndlplayer.c" ;;
  *) echo "LUMA_NDL_API must be 1 or 2" >&2; exit 1 ;;
esac
"$CC" --sysroot="$SYSROOT" -O2 -fPIC -D__WEBOS__ \
    -I"$SYSROOT/usr/include/libndl-media" \
    -shared -Wl,-soname,libluma_hwplayer.so \
    -o "$OUT/libluma_hwplayer.so" "$PLAYER_SOURCE" \
    -L"$SYSROOT/usr/lib" -lNDL_directmedia -lpthread -ldl
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
GST_CFLAGS=$(pkg-config --cflags gstreamer-app-1.0)
GST_LIBS=$(pkg-config --libs gstreamer-app-1.0)
"$CC" --sysroot="$SYSROOT" -O2 $GST_CFLAGS "$ROOT/native/smp/luma_demux.c" -o "$OUT/luma-demux" $GST_LIBS
file "$OUT/libluma_hwplayer.so" "$OUT/luma-demux"
