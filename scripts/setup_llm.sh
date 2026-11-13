#!/usr/bin/env bash
# 【可选】准备本地 LLM：llama.cpp（编译 llama-server）+ Qwen2.5-1.5B-Instruct Q4_K_M
# 项目默认用云端 LLM（config/llm.json），只有想在断网环境下跑翻译才需要执行本脚本。
# 会占用约 1.4 GB（third_party/llama.cpp 332 MB + 模型 1.1 GB）。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LLAMA_DIR="$ROOT/third_party/llama.cpp"
MODEL_DIR="$ROOT/models"
MODEL="$MODEL_DIR/qwen2.5-1.5b-instruct-q4_k_m.gguf"

if [[ ! -f "$LLAMA_DIR/CMakeLists.txt" ]]; then
    echo "== 克隆 llama.cpp =="
    mkdir -p "$ROOT/third_party"
    git clone --depth 1 https://github.com/ggerganov/llama.cpp.git "$LLAMA_DIR"
else
    echo "== llama.cpp 已存在，跳过克隆 =="
fi

if [[ ! -x "$LLAMA_DIR/build/bin/llama-server" ]]; then
    echo "== 编译 llama-server（约 2-4 分钟）=="
    cmake -S "$LLAMA_DIR" -B "$LLAMA_DIR/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLAMA_CURL=OFF \
        -DLLAMA_BUILD_TESTS=OFF \
        -DLLAMA_BUILD_EXAMPLES=OFF \
        -DLLAMA_BUILD_SERVER=ON > /tmp/llama_cmake.log 2>&1
    cmake --build "$LLAMA_DIR/build" -j"$(nproc)" --target llama-server > /tmp/llama_build.log 2>&1
else
    echo "== llama-server 已编译，跳过 =="
fi

if [[ ! -f "$MODEL" ]]; then
    echo "== 下载 Qwen2.5-1.5B-Instruct Q4_K_M（约 1.1 GB）=="
    mkdir -p "$MODEL_DIR"
    curl -L --retry 3 --progress-bar \
        -o "$MODEL" \
        https://hf-mirror.com/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf
else
    echo "== 模型已存在，跳过下载 =="
fi

ls -lh "$LLAMA_DIR/build/bin/llama-server" "$MODEL"
echo "本地 LLM 依赖就绪"
