#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
if [ -z "${WEBOS_NDK:-}" ]; then WEBOS_NDK="$ROOT/.build/toolchain/arm-webos-linux-gnueabi_sdk-buildroot"; fi
SYSROOT="$WEBOS_NDK/arm-webos-linux-gnueabi/sysroot"
CC="$WEBOS_NDK/bin/arm-webos-linux-gnueabi-gcc"
OUT="$ROOT/.build/hardware-wallpaper"
mkdir -p "$OUT"
# Clocked SMP + ACB is the personal C9 background backend. NDL v1 stays
# available for rollback; NDL v2 is an explicit newer-TV build.
case "${LUMA_NDL_API:-smp-c9}" in
  smp-c9)
    for SOURCE in luma_smp_c9 vendor/smp_player vendor/smp_payload vendor/strbuf vendor/pacer; do
      OBJECT=$(basename "$SOURCE")
      "$CC" --sysroot="$SYSROOT" -O2 -fPIC \
        -I"$SYSROOT/usr/include/starfish-media-pipeline" \
        -I"$SYSROOT/usr/include/appswitching-control-block" -I"$ROOT/native/smp/vendor" \
        -c "$ROOT/native/smp/$SOURCE.c" -o "$OUT/$OBJECT.o"
    done
    "$WEBOS_NDK/bin/arm-webos-linux-gnueabi-g++" --sysroot="$SYSROOT" -O2 -fPIC \
      -I"$SYSROOT/usr/include/starfish-media-pipeline" \
      -I"$SYSROOT/usr/include/appswitching-control-block" -I"$ROOT/native/smp/vendor" \
      -c "$ROOT/native/smp/vendor/smp_api.cpp" -o "$OUT/smp_api.o"
    "$WEBOS_NDK/bin/arm-webos-linux-gnueabi-g++" --sysroot="$SYSROOT" -shared \
      -Wl,-soname,libluma_hwplayer.so -o "$OUT/libluma_hwplayer.so" \
      "$OUT/luma_smp_c9.o" "$OUT/smp_player.o" "$OUT/smp_payload.o" \
      "$OUT/strbuf.o" "$OUT/pacer.o" "$OUT/smp_api.o" \
      -L"$SYSROOT/usr/lib" -lplayerAPIs -lAcbAPI -lpthread
    ;;
  1|2)
    if [ "$LUMA_NDL_API" = 1 ]; then PLAYER_SOURCE="$ROOT/native/smp/luma_ndl_c9.c"; else PLAYER_SOURCE="$ROOT/native/smp/luma_ndlplayer.c"; fi
    "$CC" --sysroot="$SYSROOT" -O2 -fPIC -D__WEBOS__ \
      -I"$SYSROOT/usr/include/libndl-media" -shared -Wl,-soname,libluma_hwplayer.so \
      -o "$OUT/libluma_hwplayer.so" "$PLAYER_SOURCE" \
      -L"$SYSROOT/usr/lib" -lNDL_directmedia -lpthread -ldl
    ;;
  *) echo "LUMA_NDL_API must be smp-c9, 1 or 2" >&2; exit 1 ;;
esac
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
GST_CFLAGS=$(pkg-config --cflags gstreamer-app-1.0)
GST_LIBS=$(pkg-config --libs gstreamer-app-1.0)
"$CC" --sysroot="$SYSROOT" -O2 $GST_CFLAGS "$ROOT/native/smp/luma_demux.c" -o "$OUT/luma-demux" $GST_LIBS
file "$OUT/libluma_hwplayer.so" "$OUT/luma-demux"
