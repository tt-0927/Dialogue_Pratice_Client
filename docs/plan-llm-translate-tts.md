# 翻译 + 智能回复 + 语音跟读：技术方案

> 目标：把「英语对话练习」从「看得懂但说不出口」补成「听懂 → 学着说 → 说出来」的闭环。
> 状态：**M1~M4 全部完成并验证**（翻译 / TTS / 中文输入译文卡 / 智能建议）。

---

## 0. 结论先行

1. **三个模块全部在客户端完成，服务端零改动。** 翻译、TTS、建议回复都是本地能力，协议不用动一行。
2. **LLM 统一走云端**，接口是 OpenAI 兼容的 HTTP。地址、密钥、模型全写在 `config/llm.json`，
   改完保存即生效（热重载），不用重新编译或重启。
3. **TTS 用 Piper**（神经网络、离线、单句 100–300 ms）。`espeak-ng` 只作兜底，音质太机械，不适合学口语。
4. **所有结果本地缓存**：同一句话只请求一次 LLM、只合成一次音频，重进房间/重连不重复花钱花时间。

---

## 1. 需求拆解与现状差距

| 需求 | 现状 | 差距 |
| --- | --- | --- |
| 收到他人英文消息，自动翻译成中文显示在消息下方 | 只有原文气泡 | 新增译文行 + 翻译服务 |
| 英文「语音消息」也翻译 | **不存在这种消息类型** | 见下方澄清 |
| 输入框输入中文，上方显示英文译文 | 输入框直发 | 新增译文卡 + 防抖翻译 |
| 英文译文转成口语化语音，可反复播放 | 无 | 新增 TTS + 播放器 |
| 学会后自己用语音输入说出来 | ✅ 已实现（whisper 本地识别） | 无需改动 |
| 智能提示回复 | 无 | 新增建议服务 + 建议条 |

### 关于「英文语音消息」的澄清

现有语音输入是：**麦克风 → whisper 本地识别 → 文本 → 发送 `Chat`**。
服务端和对方看到的始终是**文本**，音频不出客户端。

所以「语音消息的翻译」已经被第一条覆盖——它到了对方那里就是普通文本消息。

**已确认要做真正的语音消息**（发音频、对方点播）。设计见第 7 章。
有了它，「英文语音消息翻译」才真正成立：收到语音 → 本地 whisper 转写 → 翻译成中文 → 显示在语音条下方。

---

## 2. 环境实测约束（决定选型）

| 项 | 实测值 | 影响 |
| --- | --- | --- |
| 内存 | 总 7.7 GB / **可用 5.3 GB** | 本地 LLM 只能上 1.5B（~1.1 GB）或 3B（~2.0 GB），7B 会触发 swap |
| GPU | 无（VM 未直通 RTX 4070，且 Workstation 不支持 PCIe 直通） | 纯 CPU 推理，详见 2.1 / 2.2 |
| CPU | i5-12600KF，16 线程，AVX2 + AVX-VNNI（无 AVX512） | llama.cpp 走 AVX2 内核，约为同频 AVX512 的 70% |
| 磁盘 | 61 GB 可用 | 模型 + 语音资源约 1.3 GB，无压力 |
| 网络 | hf-mirror.com 可达，实测下载 10 MB/s | 142 MB 模型 15 秒下完，1 GB 模型约 2 分钟 |

### 2.1 本地部署的硬件要求（分档）

前提：**VMware Workstation/Player 不支持 PCIe 直通**，虚拟显卡是 `VMware SVGA II`，
没有 CUDA/ROCm 能力。所以在虚拟机里跑本地模型只能**纯 CPU 推理**，宿主有独立显卡也用不上
（想用显卡得走 2.2 的「宿主跑、VM 连」）。

内存是真正的瓶颈，不是 CPU：

| 模型 | Q4_K_M 体积 | 运行内存（含 2K 上下文 KV cache） | 加上系统 + whisper + Piper + 客户端 | 建议 VM 内存 |
| --- | --- | --- | --- | --- |
| 1.5B | 1.1 GB | ~1.8 GB | ~3.8 GB | **8 GB**（最低 4 GB） |
| 3B | 2.0 GB | ~2.8 GB | ~4.8 GB | **8 GB** |
| 7B | 4.4 GB | ~5.5 GB | ~7.5 GB | **16 GB** |
| 14B | 9 GB | ~10.5 GB | ~12.5 GB | **32 GB** |

按实测 CPU（i5-12600KF / 16 线程 / AVX2）推算的生成速度：

| 模型 | 生成速度 | 一句翻译（约 13 token） | 3 条建议（约 250 token） |
| --- | --- | --- | --- |
| 1.5B | 36 tok/s（实测） | **0.36 s** | ~7 s |
| 3B | 18–22 tok/s | 0.8–1.2 s | ~12 s |
| 7B | 8–10 tok/s | 2–3 s | ~28 s |
| 14B | 4–5 tok/s | 4–6 s | ~55 s（体验已不可接受） |

其他硬件条件：

| 项 | 要求 | 说明 |
| --- | --- | --- |
| CPU | **≥ 4 核**，8 核以上收益递减；必须 AVX2 | 2013 年后的 CPU 基本都有。有 AVX512 快 20–30%，但 12–14 代酷睿消费级没有 |
| vCPU 数 | **≤ 物理线程数** | 超了反而变慢（VMware 调度开销）。当前 16 vCPU = 宿主 16 线程，正好 |
| 磁盘 | **SSD（NVMe）**，空闲 ≥ 30 GB | 模型用 mmap 加载，机械盘首次加载要几十秒，且推理时会反复缺页 |
| 内存分配 | **≤ 宿主物理内存的 70%** | 超了宿主会 swap，整个系统卡死。宿主 32 GB → 给 VM 16 GB 是安全上限 |
| 3D 加速 | 不需要 | 那个是图形加速，跟 LLM 计算无关 |

**这台机器的结论**：7.7 GB 内存 / 16 vCPU / 60 GB 空闲，**1.5B 和 3B 都能舒适跑**，
7B 会吃紧（可用 5.3 GB vs 需要 5.5 GB，容易触发 swap）。

### 2.2 想用上 RTX 4070：宿主跑模型，VM 连 HTTP

VMware 里既然拿不到 GPU，最优解是把推理放到宿主：

```
Windows 宿主：llama-server（CUDA）监听 0.0.0.0:8080
                    ↑ 局域网/NAT
Ubuntu VM：dialogue_client → config/llm.json 的 baseUrl 指向宿主 IP
```

好处是**虚拟机内存压力归零**，速度提升 10 倍以上（4070 12 GB 跑 7B Q4 约 40–60 tok/s，
14B 约 25–35 tok/s）。客户端代码一行不用改——`LlmClient` 本来就是 OpenAI 兼容的 HTTP 客户端。

显存预算（RTX 4070 桌面版 12 GB / 笔记本版 8 GB）：

| 模型 | 模型显存 | + 4K 上下文 KV cache | 12 GB 卡 | 8 GB 卡 |
| --- | --- | --- | --- | --- |
| 7B / 8B Q4_K_M | 4.4 GB | ~5.5 GB | ✅ 很宽裕 | ✅ 可以 |
| 14B Q4_K_M | 9 GB | ~10.5 GB | ⚠️ 勉强，上下文压到 2K | ❌ 放不下 |
| 32B Q4_K_M | 20 GB | — | ❌ | ❌ |

对英语对话练习来说 **7B/8B 是甜点**：翻译质量明显好于 1.5B，速度还够快。

宿主要做的：

**① 下载 llama.cpp 的 Windows CUDA 包（两个，必须解压到同一目录）**

到 <https://github.com/ggml-org/llama.cpp/releases> 找最新的 nightly（形如 `b11960`），下这两个：

| 包 | 作用 | 大小 |
| --- | --- | --- |
| `llama-bXXXXX-bin-win-cuda-12.4-x64.zip` | 主程序 `llama-server.exe` | ~267 MB |
| `cudart-llama-bin-win-cuda-12.4-x64.zip` | CUDA 运行时 DLL，**缺了启动会报找不到 dll** | ~373 MB |

> CUDA 12.4 兼容驱动 550+；CUDA 13.4 需要更新的驱动（580+）。不确定就用 12.4。
> 如果 CUDA 版跑不起来，可以退回 `llama-bXXXXX-bin-win-vulkan-x64.zip`（36 MB，只依赖显卡驱动，
> 兼容性最好，速度比 CUDA 慢 10–20%）。

**② 启动（PowerShell，在解压目录里）**

```powershell
.\llama-server.exe -m D:\models\qwen2.5-7b-instruct-q4_k_m.gguf `
    --host 0.0.0.0 --port 8080 -ngl 99 -c 4096
```

- `--host 0.0.0.0` 是关键，默认只听 `127.0.0.1`，虚拟机连不上
- `-ngl 99` = 所有层都放到 GPU
- 模型放 NVMe SSD，路径别带中文和空格

**③ 放行防火墙**

```powershell
New-NetFirewallRule -DisplayName "llama-server 8080" -Direction Inbound `
    -Protocol TCP -LocalPort 8080 -Action Allow
```

**④ VM 里改配置**

```json
{ "llm": { "baseUrl": "http://192.168.x.x:8080", "apiKey": "", "model": "qwen2.5-7b-instruct" } }
```

`apiKey` 留空即可——`AppConfig` 对 `127.0.0.1` / 局域网地址不校验 Key。

**⑤ 验证**

```bash
# 在 VM 里
curl -s http://192.168.x.x:8080/health
curl -s http://192.168.x.x:8080/v1/chat/completions -H 'Content-Type: application/json' \
  -d '{"model":"x","messages":[{"role":"user","content":"say ok"}],"max_tokens":8}'
```

> **更省事的备选：Ollama**。装完 `ollama run qwen2.5:7b` 就有 OpenAI 兼容接口，
> 但要设环境变量 `OLLAMA_HOST=0.0.0.0` 才能被 VM 访问，
> 客户端 `baseUrl` 填 `http://192.168.x.x:11434/v1`。

> VMware 网络模式：NAT 下 VM 访问宿主用 VMnet8 网关地址（通常 `192.168.x.1`）；
> 桥接下用宿主的局域网 IP。桥接更直观，NAT 不受宿主换网络影响。

---

## 3. 选型对比

### 3.1 LLM：统一走云端

| 项 | 结论 |
| --- | --- |
| 接口 | OpenAI 兼容 `/v1/chat/completions` |
| 默认 | DeepSeek `deepseek-chat`（便宜、中文好、协议兼容） |
| 备选 | 通义 `qwen-plus`、Kimi、OpenAI——只改 `baseUrl` + `model` |
| 延迟 | 0.6–2.0 s（看网络；本地 1.5B 实测 0.36 s，云端慢但质量更好） |
| 内存 | **0**（不再需要本地模型，4 GB 内存压力解除） |
| 成本 | 约 0.001 元/千次翻译，可忽略 |
| 离线 | ❌ 断网时翻译不可用；聊天、语音识别、TTS 不受影响 |

**配置全部写在 `config/llm.json`**，改完保存即生效：

```json
{
  "llm": {
    "enabled": true,
    "baseUrl": "https://api.deepseek.com",
    "apiKey": "sk-...",
    "model": "deepseek-chat",
    "temperature": 0.2,
    "timeoutMs": 20000
  },
  "translation": { "maxHistoryMessages": 12, "cacheSize": 500 }
}
```

`baseUrl` 支持三种写法，客户端自动补全路径：

| 写法 | 实际请求 |
| --- | --- |
| `https://api.deepseek.com` | `…/v1/chat/completions` |
| `https://dashscope.aliyuncs.com/compatible-mode/v1` | `…/compatible-mode/v1/chat/completions` |
| 完整的 `…/chat/completions` | 原样使用 |

未填 `apiKey` 且不是本地地址时，翻译自动停用并提示——不会每条消息都去撞 401。

配置查找顺序：`--config <path>` → 环境变量 `DP_LLM_CONFIG` → `config/llm.json` → `~/.config/DialoguePratice/llm.json`。

### 3.1.1 备选模型清单

先分清两类——**这个项目需要的是「通用对话模型」**，因为它同时要干两件事：
把英文翻成中文，以及生成 3 条带中文的英文建议。纯翻译模型只擅长前者。

#### 通用对话模型（翻译 + 建议都能做）

| 模型 | 尺寸档 | 中文 | 许可证 | 备注 |
| --- | --- | --- | --- | --- |
| **Qwen2.5 / Qwen3** | 0.6B–72B | ★★★★★ | Apache 2.0（3B/72B 除外） | 中文最强、JSON 输出稳定，当前默认 |
| **GLM-4-9B** | 9B | ★★★★★ | GLM-4 协议 | 智谱，中文好，9B 略大于 8B 甜点 |
| **Gemma 2 / 3** | 1B–27B | ★★☆ | Gemma 协议（非标准开源，商用有条件） | 小尺寸能力强，中文一般 |
| **Phi-4 / Phi-4-mini** | 3.8B / 14B | ★★ | MIT | 推理强、英文好，中文偏弱 |
| **Llama 3.2 / 3.1** | 1B–405B | ★★☆ | Llama 社区许可 | 生态最广、工具链最全，中文一般 |
| **Ministral 3B / 8B** | 3B / 8B | ★★ | Mistral Research（商用需授权） | 端侧优化，首 token 快 |
| **MiniCPM** | 2.6B / 4B | ★★★★ | Apache 2.0 | 面壁，小尺寸中文好 |
| **DeepSeek-R1 蒸馏** | 1.5B–70B | ★★★★ | MIT | 带思维链，翻译会先"想"一遍，**慢 2–4 倍**，不建议用于实时翻译 |

#### 翻译专用模型（只翻不聊）

| 模型 | 尺寸 | 特点 |
| --- | --- | --- |
| **Hunyuan-MT-7B** | 7B | 腾讯 2025-09 开源，33 语种，**WMT2025 31 个方向里拿了 30 个第一**；基于 Qwen2 微调，有 GGUF 量化版。中英互译质量明显强于同尺寸通用模型 |
| **Tower-Plus** | 2B / 9B | Unbabel，多语翻译专用 |
| **ALMA** | 7B / 13B | 苏黎世大学，翻译专用，中英对表现不错 |
| **SeamlessM4T** | — | Meta，语音+文本翻译一体（对我们价值不大，语音已用 whisper） |

#### 针对这个项目的推荐

| 场景 | 选择 | 理由 |
| --- | --- | --- |
| **云端（当前）** | DeepSeek `deepseek-chat` / 通义 `qwen-plus` / 智谱 `glm-4-plus` | 质量、速度、成本都够，零部署 |
| **宿主 4070（12 GB）** | **Qwen2.5-7B-Instruct** 或 **GLM-4-9B** | 中文强、JSON 稳，5.5–7 GB 显存，40–60 tok/s |
| **VM 内纯 CPU（8 GB）** | **Qwen2.5-3B-Instruct** | 2.8 GB 内存，18–22 tok/s，质量可接受 |
| **VM 内极限小（4 GB）** | Qwen2.5-1.5B / Gemma-3-1B / MiniCPM-2.6B | 1.5B 实测 0.36 s/句 |
| **只想要最好的翻译** | Hunyuan-MT-7B | 翻译质量最高，但**不能生成建议**（见下） |

> **✅ 已实现「双模型」**：`ChatPage` 持有两个独立的 `LlmClient`——
> `TranslationService` 用 `translateLlm_`，`ReplyService` 用 `llm_`。
> 配置里 `translation.llm` 单独写一段即可，不写就沿用主模型（旧配置完全兼容）。
>
> ```json
> {
>   "llm":               { "model": "qwen2.5-7b-instruct", "baseUrl": "...", "apiKey": "..." },
>   "translation": { "llm": { "model": "hunyuan-mt-7b", "baseUrl": "http://192.168.80.1:8081" } }
> }
> ```
>
> 实测（两个假模型服务分别在 18081 / 18082）：翻译请求只打到 18082、建议请求只打到 18081，
> 截图见 `docs/screenshots/19-split-models.png`。
>
> 本地同时跑两个 7B 要 11 GB 显存，12 GB 卡偏紧——建议组合是
> **Qwen2.5-3B（对话，2 GB）+ Hunyuan-MT-7B（翻译，4.4 GB）≈ 6.4 GB**，或干脆都走云端。

**换模型只需要改一行**：`config/llm.json` 的 `model` 字段（本地还要改 `baseUrl`），
保存即生效，不用重新编译。

### 3.2 TTS

| 方案 | 音质 | 离线 | 部署 | 单句延迟 |
| --- | --- | --- | --- | --- |
| **Piper**（推荐） | 自然，接近真人朗读 | ✅ | 下载 20 MB 二进制 + 63 MB 模型 | 100–300 ms |
| espeak-ng | 机械音，学口语会带偏 | ✅ | `apt install` | < 50 ms |
| qt6-speech（speech-dispatcher） | 后端仍是 espeak | ✅ | apt | < 100 ms |
| 云端 TTS | 最好 | ❌ | API Key + 计费 | 300–800 ms |

**推荐 Piper**：`en_US-lessac-medium` 女声，语速可调（`--length-scale 1.1` 放慢，适合跟读）。
调用方式是子进程：`echo "text" | piper -m model.onnx -f out.wav`，不用往客户端里链 onnxruntime。

---

## 4. 总体架构

```
                         ┌──────────────────── 客户端 ────────────────────┐
收到他人消息 ──► ChatPage ──► TranslationService ──► LlmClient ──┐
                   │              │  ▲                           │  HTTP (OpenAI 兼容)
                   │              │  └── 缓存：内存 LRU + 磁盘     ▼
                   ▼              │                     云端 API（OpenAI 兼容）
             MessageBubble        │
             （译文行）            │
                                  │
输入中文 ──► 译文卡 ──► TtsService ──► piper 子进程 ──► WAV ──► QMediaPlayer
                                  ▲
建议回复条 ◄── ReplyService ◄─────┘  （最近 6 条消息做上下文）
```

**线程模型**：LLM 走 `QNetworkAccessManager`（异步，不占线程）；TTS 走 `QProcess`（异步）；
whisper 已在独立线程。UI 线程全程不阻塞。

---

## 5. 交互设计

### 5.1 收到他人的英文消息

```
[头像] bob
       ┌────────────────────────────────┐
       │ Could you say that again,      │
       │ please?                        │
       │ ─────────────────────────────  │  ← 1px 分隔线
       │ 你能再说一遍吗？                 │  ← 译文：字号小 1pt、灰色
       │                        20:15   │
       └────────────────────────────────┘
```

时序：
1. `appendChat` → 判定「非本人 + 含拉丁字母」→ 气泡先渲染原文，译文行显示 `翻译中…`
2. **200 ms 防抖合批**：连续到达的多条（尤其进房补历史）合成一次请求，一次最多 8 条
3. 返回后按 `seq` 回填；失败显示 `翻译失败 · 点击重试`

历史消息也翻译，但只翻最近 30 条（更早的滑动到可见区域再懒翻译，避免一次几十个请求）。

### 5.2 输入框输入中文

```
┌─ 译文 ──────────────────────────────────────┐
│ Could you say that again, please?      ▶ 播放 │
│                              [ 填入输入框 ]  │
└─────────────────────────────────────────────┘
[ 🎤 ]  [ 你能再说一遍吗？            ]  [ ➤ ]
```

- 输入停顿 **500 ms** 且内容含中文 → 请求 `中 → 英`
- 译文卡出现，带 **▶ 播放**（可反复点）和 **填入输入框**
- 点「填入」把英文放进输入框，你可以改，再回车发送
- 输入框清空 → 卡片自动消失

### 5.3 跟读闭环（核心场景）

```
① 听到对方英文 → 看中文译文（5.1）
② 想说中文 → 看英文译文 + 点播放反复听（5.2）
③ 跟着念 → 点麦克风 → whisper 识别 → 文本进输入框
④ 回车发送 —— 这次是你自己说出来的
```

第 ③ 步复用现有语音输入，不需要改动。

### 5.4 智能提示回复

输入框为空、且刚收到他人消息时，自动请求 3 条英文建议：

```
┌─ 建议回复 ────────────────────────────────────┐
│ [ Sure, that sounds great.        ▶ ]        │
│ [ I'm not sure I follow.          ▶ ]        │
│ [ Could you explain that again?   ▶ ]        │
└──────────────────────────────────────────────┘
```

- 点 chip 正文 → 填入输入框
- 点 ▶ → 播放发音（先听后说）
- 手动触发：输入框右侧一个小灯泡按钮

**同一时刻只显示一个辅助面板**：输入框有中文 → 显示译文卡；输入框为空且有新消息 → 显示建议条。
避免输入区上方堆成三明治。

---

## 6. 模块与文件清单

```
Dialogue_Pratice_Client/
├── LlmClient.{h,cpp}          # OpenAI 兼容 /v1/chat/completions，异步 + 超时 + 重试
├── TranslationService.{h,cpp} # 合批、去重、缓存、回填
├── ReplyService.{h,cpp}       # 3 条建议，JSON 输出解析
├── TtsService.{h,cpp}         # piper 子进程 + 缓存 + QMediaPlayer 播放
├── TranslationCard.{h,cpp}    # 输入框上方的译文卡（UI）
├── SuggestionBar.{h,cpp}      # 建议回复条（UI）
├── SettingsDialog.{h,cpp}     # LLM 地址/模型/开关、TTS 音色/语速
└── scripts/
    ├── setup_llm.sh           # 拉 llama.cpp + GGUF（约 1.1 GB）
    └── setup_tts.sh           # 拉 piper + 英文音色（约 83 MB）
```

关键接口草案：

```cpp
class LlmClient : public QObject {
public:
    struct Config { QString baseUrl, apiKey, model; int timeoutMs = 8000; };
    void setConfig(const Config&);
    // 返回 requestId，完成后发 finished(requestId, text) / failed(requestId, reason)
    int  chat(const QString& system, const QString& user, int maxTokens = 256);
signals:
    void finished(int requestId, const QString& text);
    void failed(int requestId, const QString& reason);
};

class TranslationService : public QObject {
public:
    void queueForMessage(qint64 seq, const QString& english);  // 合批
    void translateInput(const QString& chinese);               // 单条，防抖
signals:
    void messageTranslated(qint64 seq, const QString& chinese);
    void inputTranslated(const QString& english);
    void failed(const QString& reason);
};

class TtsService : public QObject {
public:
    void speak(const QString& english);   // 命中缓存直接播，否则先合成
    void stop();
signals:
    void speakingChanged(bool);
    void failed(const QString& reason);
};
```

`ChatItem` 增加两个字段：`translation`、`translationState`（None/Pending/Done/Failed），
`MessageBubble` 增加 `setTranslation()` / `setTranslationPending()`。

---

## 7. 语音消息（新增协议 + 服务端改动）

翻译、TTS、建议回复三项**服务端零改动**。语音消息是唯一要动协议和服务的部分。

### 7.1 编码：Opus，不是 PCM

| 方案 | 10 秒体积 | 依赖 | 结论 |
| --- | --- | --- | --- |
| PCM 16k/16bit | 320 KB | 无 | 太大，必须拆帧 |
| **Opus 16 kbps** | **约 20 KB** | `libopus-dev` | ✅ base64 后 27 KB，**现有 64 KB 单帧直接装得下** |
| AAC/M4A | 约 24 KB | FFmpeg | 依赖太重 |

选 Opus 的关键收益：**帧格式完全不用改**，`kMaxFrameSize` 保持 64 KB，
不需要分片协议、不需要重组缓冲，服务端只当不透明字节流搬运。

### 7.2 协议扩展（3 处）

| 方向 | opcode | payload |
| --- | --- | --- |
| 上行 | `Voice` 0x09 | `{"durationMs":3200,"opus":"<base64>"}` |
| 下行 | `VoicePush` 0x8A | `{"seq":31,"from":7,"name":"bob","room":"1001","durationMs":3200,"opus":"...","ts":...}` |
| 下行 | `HistoryResp` 0x89 | 条目增加 `kind` 字段：`{"kind":"text"}` 或 `{"kind":"voice","audio":"...","durationMs":...}` |

历史条目结构从「只有文本」变成「文本或语音」，`kind` 缺省视为 `text`，向后兼容。

### 7.3 服务端改动

- `HistoryEntry` 增加 `kind`（Text/Voice）、`audio`（std::string）、`durationMs`
- 每房间语音字节上限 **512 KB**（约 25 条 10 秒语音），超出按最旧淘汰——
  复用现有 `appendHistory()` 的条数/字节双上限逻辑，只是把 audio 也算进字节数
- 服务端**不解析音频**，只做透传与缓存，不引入任何编解码依赖

### 7.4 客户端改动

- **录制发送**：复用 `VoiceInput` 的采集链路，但走「发送」模式（不送 whisper），
  libopus 编码后发 `Voice`。按住麦克风说话 → 松手发送（与语音输入区分：长按说话，点击是输入法）
- **语音气泡**：▶ 播放按钮 + 时长 + 简单波形（按帧能量画 40 根柱子）
- **播放**：Opus 解码 → `QAudioSink` 播放，播放中按钮变暂停，可重播
- **收到语音**：后台 whisper 转写 → 得到英文原文 → 走翻译服务 → 气泡下方显示
  「英文原文 + 中文译文」两行。**这才是「英文语音消息翻译」的完整形态**

### 7.5 兼容性

- 新客户端连旧服务端：`Voice` 会收到 `unknown_op` 错误，界面提示「服务端版本过旧」
- 新服务端推 `VoicePush` 给旧客户端：旧客户端 `handleFrame` 的 `default` 分支静默忽略，不崩
- 因此**不需要版本协商**

### 7.6 交互上的一个取舍

麦克风按钮现在有两种语义，容易误触：
- **点击** = 语音输入（转文字进输入框，现有行为）
- **长按** = 录语音消息（松手发送）

用长按区分，并在按住时把按钮变成红色圆环 + 显示「松手发送」。

---

## 8. 性能与资源预算（预估值，实施后补实测）

| 项 | 预估 |
| --- | --- |
| 单条翻译 | 云端 0.6–2.0 s（待填 Key 实测）；本地 1.5B 联调值 0.36 s 作为参考 |
| 批量 8 条翻译 | 2.5–4 s（一次前向，摊薄后更快） |
| TTS 合成一句 | 100–300 ms（命中缓存 0 ms） |
| 建议回复 3 条 | 1.5–3 s（后台，不阻塞） |
| 常驻内存增量 | 0（LLM 在云端）+ 0.2 GB（whisper 已有）+ 0.05 GB（TTS 播放） |
| 磁盘 | +0.08 GB（只剩 TTS 音色） |
| 翻译缓存 | 内存 LRU 500 条；磁盘可选 |
| 语音消息体积 | Opus 16 kbps 单声道：10 秒约 20 KB（base64 后 27 KB，单帧可容纳） |
| 服务端语音存储 | 每房间上限 512 KB（约 25 条 10 秒语音），超出淘汰最旧的 |

---

## 9. 实施计划

| 阶段 | 内容 | 验收标准 |
| --- | --- | --- |
| ~~**M1** LLM 底座~~ ✅ | `config/llm.json` 配置化（云端 OpenAI 兼容）+ 热重载；`LlmClient` + `TranslationService`；收到消息自动翻译 | **已完成**：用本地 OpenAI 兼容服务联调通过（0.36 s/句）；改配置保存后自动重载并补翻；未填 Key 时明确提示且不阻塞 |
| ~~**M2** TTS~~ ✅ | `setup_tts.sh` 拉 piper + 音色；`TtsService` + 气泡播放按钮 | **已完成**：`--say` 诊断通过（93 KB WAV）；点气泡 ▶ 触发合成（缓存 0→1） |
| ~~**M3** 中文输入译文卡~~ ✅ | 防抖翻译 + 译文卡 + 填入输入框 | **已完成**：输入中文出英文、▶ 可听、填入后输入框变英文 |
| ~~**M4** 智能建议~~ ✅ | `ReplyService` + `SuggestionBar`（设置对话框仍未做，改配置用文件） | **已完成**：收到消息后出现 3 条建议；点 chip 填入输入框；与译文卡互斥 |
| **M5** 语音消息 | Opus 编解码 + `Voice`/`VoicePush` + 服务端存储 + 语音气泡 + 收到语音自动转写翻译 | 长按录 3 秒发出去，对方点播能听到；收到的语音下方自动出现英文原文与中文译文 |

每阶段都能独立跑通、独立演示，不会出现「做了一半不能看」的状态。

---

## 10. 风险与降级

| 风险 | 降级策略 |
| --- | --- |
| 内存不足（4 GB 可用） | 默认 1.5B；`llama-server` 加 `--ctx-size 2048` 控内存；再不够切云端 |
| LLM 进程挂了 | 翻译服务自动降级为「关闭」，聊天不受影响；设置页可一键重启 |
| TTS 合成失败 | 回退 espeak-ng；再失败则隐藏播放按钮 |
| 首次加载慢（3–5 s） | 应用启动时后台预热，模型加载完再点亮翻译图标 |
| 隐私 | 本地部署时消息不出机器；切云端时设置页明确提示「消息将发送到 <地址>」 |
| 译文质量差 | 设置页提供「重新翻译」按钮；提示词可换（直译/意译两套） |

---

## 11. 已确认的决策

| 决策 | 结论 |
| --- | --- |
| LLM 部署 | **云端**（OpenAI 兼容），模型/地址/密钥写在 `config/llm.json`，热重载 |
| 语音消息 | **做**（Opus + `Voice`/`VoicePush`，见第 7 章） |
| 译文缓存 | 默认只放内存（LRU 500 条）；落盘留作后续增强 |

### 实施顺序

`M1 翻译底座` → `M2 TTS 播放` → `M3 中文输入译文卡` → `M5 语音消息` → `M4 智能建议`

先做 M1 是因为它是整条链路的地基（译文渲染、缓存、失败态都在这里定型）；
M4 放最后是因为它依赖 M1 的上下文拼接，且纯属锦上添花。
