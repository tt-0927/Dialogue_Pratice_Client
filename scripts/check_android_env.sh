#!/usr/bin/env bash
# Android 打包环境自检。
#
# 用法：./scripts/check_android_env.sh
#
# 逐项检查交叉编译 Android APK 所需的环境，缺什么就打印对应的安装命令。
# 只读，不改动任何东西。

set -uo pipefail

ok_count=0
miss_count=0

pass() { printf '  [OK]   %s\n' "$1"; ok_count=$((ok_count + 1)); }
miss() { printf '  [缺失] %s\n         → %s\n' "$1" "$2"; miss_count=$((miss_count + 1)); }

echo "== Android 打包环境自检 =="
echo

# ---- 1. JDK ----
if command -v java >/dev/null 2>&1; then
    ver="$(java -version 2>&1 | head -1)"
    pass "JDK：$ver"
elif [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/java" ]]; then
    pass "JDK：$("$JAVA_HOME/bin/java" -version 2>&1 | head -1)"
else
    miss "JDK 未安装（Gradle 需要 11 或 17）" \
         "见 docs/android-build.md 第 1 步"
fi

# ---- 2. Android SDK ----
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
if [[ -d "$SDK/platform-tools" ]]; then
    pass "Android SDK：$SDK"
else
    miss "Android SDK 未安装（或 ANDROID_SDK_ROOT 未设置）" \
         "见 docs/android-build.md 第 2 步"
fi

# ---- 3. Android NDK ----
NDK=""
if [[ -n "${ANDROID_NDK_ROOT:-}" && -d "$ANDROID_NDK_ROOT" ]]; then
    NDK="$ANDROID_NDK_ROOT"
elif [[ -d "$SDK/ndk" ]]; then
    NDK="$(find "$SDK/ndk" -maxdepth 1 -mindepth 1 -type d | sort -V | head -1)"
fi

if [[ -n "$NDK" && -d "$NDK/toolchains/llvm" ]]; then
    pass "Android NDK：$(basename "$NDK")"
    case "$(basename "$NDK")" in
        23.*|24.*) : ;;
        *) printf '         ⚠ Qt 6.2.x 官方支持 NDK 23，更高版本可能报错\n' ;;
    esac
else
    miss "Android NDK 未安装" "见 docs/android-build.md 第 2 步"
fi

# ---- 4. Qt for Android ----
QT_ANDROID=""
for candidate in "$HOME"/Qt/6.*/android_arm64_v8a "$HOME"/Qt/6.*/android_armv7; do
    [[ -d "$candidate" ]] && QT_ANDROID="$candidate" && break
done

if [[ -n "$QT_ANDROID" ]]; then
    pass "Qt for Android：$QT_ANDROID"
elif [[ -n "${QT_HOST_PATH:-}" ]]; then
    pass "Qt for Android：由 QT_HOST_PATH 指定"
else
    miss "Qt for Android 未安装（apt 的 Qt6 不含 Android 支持）" \
         "见 docs/android-build.md 第 3 步（需图形界面 + Qt 账号）"
fi

# ---- 5. opus 静态库 ----
if [[ -f "$HOME/Android/deps/arm64/lib/libopus.a" ]]; then
    pass "opus（arm64 静态库）"
else
    miss "opus 未为 arm64 交叉编译" "见 docs/android-build.md 第 4 步"
fi

# ---- 6. 磁盘 ----
avail_kb="$(df -Pk "$HOME" | awk 'NR==2 {print $4}')"
avail_gb=$((avail_kb / 1024 / 1024))
if (( avail_gb >= 10 )); then
    pass "磁盘可用：${avail_gb} GB"
else
    miss "磁盘只剩 ${avail_gb} GB（建议 ≥ 10 GB）" "清理 ~/Qt 或 build 目录"
fi

# ---- 7. 内存 ----
mem_kb="$(awk '/MemAvailable/ {print $2}' /proc/meminfo)"
mem_gb=$((mem_kb / 1024 / 1024))
if (( mem_gb >= 4 )); then
    pass "可用内存：${mem_gb} GB"
else
    miss "可用内存 ${mem_gb} GB（Gradle 建议 ≥ 4 GB）" \
         "关闭其他程序，或调低 gradle.properties 的 -Xmx"
fi

echo
if (( miss_count == 0 )); then
    echo "== 全部就绪，可以构建 =="
    echo "   ./scripts/build_android.sh"
else
    echo "== 还差 ${miss_count} 项（已就绪 ${ok_count} 项）=="
    echo "   完整步骤见 docs/android-build.md"
fi

exit 0
