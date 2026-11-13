#!/bin/bash
# 一键安装 Android 构建工具链（配套 Qt 6.12 LTS）
#
# 前置：JDK 21 已解压到 ~/opt/jdk-21*，cmdline-tools 的 zip 已下到 ~/Downloads/cmdtools.zip
# 装什么：platform-tools + platforms;android-36 + NDK r27c(27.2.12479018)
#
# 用法：bash scripts/setup_android_env.sh
# 幂等：已装过的组件 sdkmanager 会跳过
set -e

SDK="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
JDK_DIR="$(ls -d "$HOME"/opt/jdk-21* 2>/dev/null | head -1)"

if [ -z "$JDK_DIR" ]; then
    echo "!! 找不到 JDK 21（~/opt/jdk-21*），先按 docs/android-build.md 第 1 节装" >&2
    exit 1
fi

export JAVA_HOME="$JDK_DIR"
export ANDROID_SDK_ROOT="$SDK"
export ANDROID_HOME="$SDK"
export PATH="$JAVA_HOME/bin:$SDK/cmdline-tools/latest/bin:$SDK/platform-tools:$PATH"

echo "JAVA_HOME = $JAVA_HOME"
echo "SDK       = $SDK"
java -version 2>&1 | head -1

# --- 1. cmdline-tools（目录名必须是 latest，否则 sdkmanager 找不到自己）---
if [ ! -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]; then
    echo ">> 解压 cmdline-tools"
    mkdir -p "$SDK/cmdline-tools"
    cd "$SDK/cmdline-tools"
    rm -rf cmdline-tools latest
    unzip -q "$HOME/Downloads/cmdtools.zip"
    mv cmdline-tools latest
fi

# --- 2. 接受许可 ---
yes | sdkmanager --licenses >/dev/null 2>&1 || true

# --- 3. 装组件 ---
# NDK 必须 r27c：Qt 6.12 的官方二进制就是用它编的，版本错会在 androiddeployqt 阶段炸
# build-tools 必须单独确认：几个包写在一起时，sdkmanager 可能静默跳过其中一个
# （实测踩过：build-tools 没装上，直到 androiddeployqt 报 "Could not locate build tools" 才发现）
echo ">> 安装 SDK 组件（NDK 约 1GB，耐心等）"
for pkg in "platform-tools" \
           "platforms;android-36" \
           "build-tools;36.0.0" \
           "ndk;27.2.12479018"; do
    echo "   -> $pkg"
    sdkmanager "$pkg" || echo "   !! $pkg 安装失败" >&2
done

echo
echo "=== 装完检查 ==="
for pkg in platform-tools platforms build-tools ndk; do
    if [ -d "$SDK/$pkg" ] && [ -n "$(ls -A "$SDK/$pkg" 2>/dev/null)" ]; then
        echo "  OK       $pkg: $(ls "$SDK/$pkg" | tr '\n' ' ')"
    else
        echo "  MISSING  $pkg   <-- 必须补装，否则 androiddeployqt 会失败" >&2
    fi
done
"$SDK/platform-tools/adb" version 2>/dev/null | head -1
echo
echo "把下面两行写进 ~/.bashrc："
echo "export JAVA_HOME=\"$JAVA_HOME\""
echo "export ANDROID_SDK_ROOT=\"$SDK\""
echo "export PATH=\"\$JAVA_HOME/bin:\$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:\$ANDROID_SDK_ROOT/platform-tools:\$PATH\""
