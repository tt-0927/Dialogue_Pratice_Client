#!/usr/bin/env bash
# 拉取 Piper TTS（神经网络、离线）+ 英文音色
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIPER_DIR="$ROOT/third_party/piper"
MODEL_DIR="$ROOT/models"
VOICE="en_US-lessac-medium"
BASE_URL="https://hf-mirror.com/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium"

if [[ -x "$PIPER_DIR/piper" ]]; then
    echo "== piper 已存在，跳过 =="
else
    echo "== 下载 piper 二进制（约 20 MB）=="
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
    curl -L --retry 3 --progress-bar \
        -o "$TMP/piper.tar.gz" \
        https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_linux_x86_64.tar.gz
    mkdir -p "$PIPER_DIR"
    tar -xzf "$TMP/piper.tar.gz" -C "$PIPER_DIR" --strip-components=1
fi

mkdir -p "$MODEL_DIR"
for ext in onnx onnx.json; do
    target="$MODEL_DIR/$VOICE.$ext"
    if [[ -f "$target" ]]; then
        echo "== $VOICE.$ext 已存在 =="
    else
        echo "== 下载 $VOICE.$ext =="
        curl -L --retry 3 --progress-bar -o "$target" "$BASE_URL/$VOICE.$ext"
    fi
done

echo "== 自检：合成一句 =="
echo "Hello, this is a text to speech test." | \
    "$PIPER_DIR/piper" -m "$MODEL_DIR/$VOICE.onnx" -f /tmp/tts_probe.wav >/dev/null 2>&1
ls -lh /tmp/tts_probe.wav
echo "Piper 就绪"
