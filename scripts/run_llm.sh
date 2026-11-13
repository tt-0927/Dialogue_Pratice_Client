#!/usr/bin/env bash
# 【可选】启动本地 LLM 服务（OpenAI 兼容，默认 127.0.0.1:8080）
# 项目默认用云端 LLM；只有把 config/llm.json 的 baseUrl 指向 127.0.0.1:8080 时才需要本脚本。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/third_party/llama.cpp/build/bin/llama-server"
MODEL="$ROOT/models/qwen2.5-1.5b-instruct-q4_k_m.gguf"

if [[ ! -x "$BIN" || ! -f "$MODEL" ]]; then
    echo "缺少 llama-server 或模型，请先运行 scripts/setup_llm.sh" >&2
    exit 1
fi

# -c 2048 控制内存占用；-t 留两个核给 whisper 和界面
exec "$BIN" -m "$MODEL" --host 127.0.0.1 --port 8080 -c 2048 -t "$(( $(nproc) > 2 ? $(nproc) - 2 : 1 ))"
