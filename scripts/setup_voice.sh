#!/usr/bin/env bash
# 准备语音输入依赖：whisper.cpp 源码 + base.en 模型（约 142 MB）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WHISPER_DIR="$ROOT/third_party/whisper.cpp"
MODEL_DIR="$ROOT/models"
MODEL="$MODEL_DIR/ggml-base.en.bin"

if [[ ! -f "$WHISPER_DIR/CMakeLists.txt" ]]; then
    echo "== 克隆 whisper.cpp =="
    mkdir -p "$ROOT/third_party"
    git clone --depth 1 https://github.com/ggerganov/whisper.cpp.git "$WHISPER_DIR"
else
    echo "== whisper.cpp 已存在，跳过 =="
fi

if [[ ! -f "$MODEL" ]]; then
    echo "== 下载模型 ggml-base.en.bin（142 MB）=="
    mkdir -p "$MODEL_DIR"
    # hf-mirror.com 是 HuggingFace 的国内镜像；网络通畅时也可换成 huggingface.co
    curl -L --retry 3 --progress-bar \
        -o "$MODEL" \
        https://hf-mirror.com/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
else
    echo "== 模型已存在，跳过 =="
fi

ls -lh "$MODEL"
echo "语音依赖就绪，可以重新构建客户端"
