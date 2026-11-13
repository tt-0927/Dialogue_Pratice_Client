#!/usr/bin/env bash
# 构建 Android APK（arm64-v8a）。
#
# 前置：先跑 ./scripts/check_android_env.sh，全部通过再执行本脚本。
#
# 用法：
#   ./scripts/build_android.sh              # 默认 arm64-v8a / Release
#   ABI=armv7a ./scripts/build_android.sh   # 32 位（老设备）
#
# 说明：
#   - 语音识别（whisper.cpp）与语音合成（Piper）在 Android 上需要额外的
#     交叉编译，本脚本默认**关闭**这两项，只保留「录音发送 + 语音播放」。
#     需要时加 DP_WITH_WHISPER=ON 并自行准备 arm64 版 whisper。
#   - opus 必须提前交叉编译到 $ANDROID_DEPS，见 docs/android-build.md 第 4 步。

set -euo pipefail

# Qt 6.12 要求 CMake >= 3.25，系统自带的 3.22.1 不够。
# 非交互 shell 不会读 ~/.bashrc，这里显式补上，否则 qt-cmake 会失败。
if [[ -d "$HOME/opt/cmake-3.31.12-linux-x86_64/bin" ]]; then
    export PATH="$HOME/opt/cmake-3.31.12-linux-x86_64/bin:$PATH"
fi
if ! cmake --version | head -1 | grep -qE '3\.(2[5-9]|[3-9][0-9])'; then
    echo "警告：当前 cmake 是 $(cmake --version | head -1)，Qt 6.12 要求 >= 3.25" >&2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-android"
ABI="${ABI:-arm64-v8a}"
ANDROID_DEPS="${ANDROID_DEPS:-$HOME/Android/deps/arm64}"

# ---- 定位 Qt for Android ----
QT_ANDROID="${QT_ANDROID:-}"
if [[ -z "$QT_ANDROID" ]]; then
    for candidate in "$HOME"/Qt/6.*/android_arm64_v8a; do
        [[ -d "$candidate" ]] && QT_ANDROID="$candidate" && break
    done
fi
if [[ -z "$QT_ANDROID" || ! -x "$QT_ANDROID/bin/qt-cmake" ]]; then
    echo "找不到 Qt for Android（qt-cmake）。先跑 ./scripts/check_android_env.sh" >&2
    exit 1
fi
echo "Qt for Android: $QT_ANDROID"

# ---- 定位 SDK / NDK ----
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
NDK="${ANDROID_NDK_ROOT:-}"
if [[ -z "$NDK" && -d "$SDK/ndk" ]]; then
    # 取最新版本：Qt 6.12 配 NDK r27c（27.2.12479018），旧版会在 androiddeployqt 阶段炸
    NDK="$(find "$SDK/ndk" -maxdepth 1 -mindepth 1 -type d | sort -V | tail -1)"
fi
if [[ -z "$NDK" || ! -d "$NDK" ]]; then
    echo "找不到 Android NDK。先跑 ./scripts/check_android_env.sh" >&2
    exit 1
fi
echo "SDK: $SDK"
echo "NDK: $NDK"

# ---- JDK（Qt 6.12 要求 21，非交互 shell 不会读 ~/.bashrc）----
if [[ -z "${JAVA_HOME:-}" || ! -x "${JAVA_HOME:-}/bin/java" ]]; then
    for cand in "$HOME"/opt/jdk-21* /usr/lib/jvm/java-21-openjdk-amd64; do
        if [[ -x "$cand/bin/java" ]]; then
            export JAVA_HOME="$cand"
            break
        fi
    done
fi
if [[ -z "${JAVA_HOME:-}" || ! -x "$JAVA_HOME/bin/java" ]]; then
    echo "找不到 JDK 21。见 docs/android-build.md 第 1 节" >&2
    exit 1
fi
export PATH="$JAVA_HOME/bin:$PATH"
echo "JDK: $("$JAVA_HOME/bin/java" -version 2>&1 | head -1)"

# ---- build-tools（androiddeployqt 必需，最容易漏装）----
if [[ ! -d "$SDK/build-tools" ]] || [[ -z "$(ls -A "$SDK/build-tools" 2>/dev/null)" ]]; then
    echo "缺少 Android build-tools：$SDK/build-tools 不存在或为空" >&2
    echo "  sdkmanager \"build-tools;36.0.0\"" >&2
    exit 1
fi
BT_DIR="$(ls -d "$SDK"/build-tools/* 2>/dev/null | sort -V | tail -1)"
echo "build-tools: $BT_DIR"

if [[ ! -f "$ANDROID_DEPS/lib/libopus.a" ]]; then
    echo "缺少 arm64 版 opus：$ANDROID_DEPS/lib/libopus.a" >&2
    echo "见 docs/android-build.md 第 4 步" >&2
    exit 1
fi

# ---- 配置 ----
# 注意：SHARED_ROOT 指向服务端工程，提供 common/Protocol.h。
# Android 构建不需要服务端可执行文件，只要头文件和 Protocol.cpp。
echo
echo "== 配置 =="
"$QT_ANDROID/bin/qt-cmake" -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI="$ABI" \
    -DANDROID_SDK_ROOT="$SDK" \
    -DANDROID_NDK_ROOT="$NDK" \
    -DANDROID_PLATFORM=android-28 \
    -DCMAKE_PREFIX_PATH="$ANDROID_DEPS" \
    -DDP_WITH_WHISPER=OFF \
    -DDP_WITH_TTS=OFF

# ---- 构建 ----
echo
echo "== 构建 APK =="
cmake --build "$BUILD" --target apk -j"$(nproc)"

# ---- 签名 ----
# androiddeployqt 产出的是 unsigned APK，必须先签名才能 adb install。
echo
echo "== 签名 =="
UNSIGNED="$(find "$BUILD" -name '*-unsigned.apk' -type f 2>/dev/null | head -1)"
if [[ -z "$UNSIGNED" ]]; then
    echo "没有生成 APK，检查上面的构建输出" >&2
    exit 1
fi

SIGNED="${UNSIGNED%-unsigned.apk}-signed.apk"
KS="${ANDROID_DEBUG_KEYSTORE:-$HOME/.android/debug.keystore}"
mkdir -p "$(dirname "$KS")"
if [[ ! -f "$KS" ]]; then
    echo ">> 生成 debug keystore: $KS"
    keytool -genkeypair -v -keystore "$KS" -alias androiddebugkey \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -storepass android -keypass android \
        -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
fi

# -p 开启 16 KB 页对齐（Google Play 2025-11 起的硬要求）
"$BT_DIR/zipalign" -p -f 4 "$UNSIGNED" "$BUILD/.aligned.apk"
"$BT_DIR/apksigner" sign --ks "$KS" \
    --ks-pass pass:android --key-pass pass:android \
    --out "$SIGNED" "$BUILD/.aligned.apk"
rm -f "$BUILD/.aligned.apk"
"$BT_DIR/apksigner" verify "$SIGNED" && echo "验签通过"

# ---- 输出 ----
echo
echo "== 产物 =="
SIZE_MB="$(awk -v b="$(stat -c%s "$SIGNED")" 'BEGIN{printf "%.1f", b/1048576}')"
printf '  %s  (%s MB)\n' "$SIGNED" "$SIZE_MB"

echo
echo "安装到手机（USB 或 adb over WiFi）："
echo "  adb install -r \"$SIGNED\""
echo
echo "注意：debug 签名的 APK 只适合自测，上架需换成自己的 keystore。"
