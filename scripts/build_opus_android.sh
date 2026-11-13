#!/usr/bin/env bash
# 交叉编译 opus 静态库（Android arm64，配套 Qt 6.12 / NDK r27c）
#
# 产物：~/Android/deps/arm64/lib/libopus.a
#
# 用法：./scripts/build_opus_android.sh
#   API=28 指定最低 API 等级（必须与 build_android.sh 的 ANDROID_PLATFORM 一致）
#   OPUS_VER=1.3.1 指定 opus 版本
set -euo pipefail

SDK="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
NDK="${ANDROID_NDK_ROOT:-$SDK/ndk/27.2.12479018}"
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
API="${API:-28}"
PREFIX="${PREFIX:-$HOME/Android/deps/arm64}"
OPUS_VER="${OPUS_VER:-1.3.1}"

CC_BIN="$TOOLCHAIN/bin/aarch64-linux-android${API}-clang"
if [ ! -x "$CC_BIN" ]; then
    echo "找不到 $CC_BIN" >&2
    echo "检查 NDK 路径：$NDK" >&2
    exit 1
fi

echo "NDK    = $NDK"
echo "API    = $API"
echo "PREFIX = $PREFIX"

mkdir -p "$HOME/build" "$PREFIX"
cd "$HOME/build"

if [ ! -f "opus-$OPUS_VER.tar.gz" ]; then
    echo ">> 下载 opus $OPUS_VER"
    curl -LO "https://downloads.xiph.org/releases/opus/opus-$OPUS_VER.tar.gz"
fi

rm -rf "opus-$OPUS_VER"
tar xzf "opus-$OPUS_VER.tar.gz"
cd "opus-$OPUS_VER"

echo ">> configure"
./configure \
    --host=aarch64-linux-android \
    --with-sysroot="$TOOLCHAIN/sysroot" \
    CC="$CC_BIN" \
    CXX="$TOOLCHAIN/bin/aarch64-linux-android${API}-clang++" \
    --prefix="$PREFIX" \
    --disable-shared --enable-static

echo ">> make"
make -j"$(nproc)"
make install

echo
echo "=== 产物 ==="
ls -lh "$PREFIX/lib/libopus.a"
echo
echo "静态库里的 .o 是 REL 类型，没有 LOAD 段，16 KB 对齐要到最终链接才生效。"
echo "NDK r27 的 clang 默认带 -z max-page-size=16384，APK 构建完成后这样验证："
echo "  \$TOOLCHAIN/bin/llvm-readelf -l <libdialogue_client_arm64-v8a.so> | grep LOAD"
echo "最后一列 Align 应为 0x4000（16 KB），而不是 0x1000（4 KB）。"
