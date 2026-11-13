# 声网 RTM 接入方案（已废弃）

> **⚠️ 本方案已于 2026-10-29 放弃并清理**：项目改用局域网自研服务端，
> `RtmBridge`、声网 SDK（13 MB）、`rtm_smoke`、`setup_agora.sh` 全部删除，
> 二进制不再链接 `libagora_rtm_sdk.so`。
>
> 本文档仅作**设计记录**保留——里面的 SDK 踩坑（`libaosl` 间接依赖、`eventHandler` 必填、
> 同 userId 双登录互踢、seq 冲突）和 RTC 平台支持的结论仍然有效，
> 以后若重新评估云传输方案可以少走弯路。

> 目标：把「英语对话练习」的消息链路换成声网实时消息（RTM 2.x），
> 拿到公网可达、自带重连/弱网优化的传输能力。
> 状态：~~A1（SDK 打通）+ A2（双模式适配）+ A3（本地历史缓存）已完成~~ → **整体废弃**

---

## 1. 先给结论：能做，但要想清楚替代的是什么

**实测结果（已在你的 Ubuntu 22.04 上跑通，不是查文档抄的）：**

| 项 | 结果 |
| --- | --- |
| SDK 版本 | RTM C++ for Linux **v2.3.0**（2026-07-20 发布） |
| 安装包 | 4.8 MB，MD5 `9a8ee5f8deda76e23eea80f5b3c5a453`（与官方一致） |
| 平台要求 | x86-64 / **C++14** / Ubuntu 18.04+ —— 我们的 C++17 + 22.04 满足 |
| 编译链接 | ✅ g++ 11 通过（`-lagora_rtm_sdk -laosl -lpthread`） |
| 运行时 | ✅ 加载成功，`createAgoraRtmClient()` 返回 `-10004`（未设 eventHandler，符合预期） |

**三个必须知道的实测细节：**

1. **`libaosl.so` 是间接依赖**，只在 exe 上设 `RUNPATH` 找不到它。
   必须加 `-Wl,--disable-new-dtags`（强制用可传递的 `DT_RPATH`），否则启动就报
   `libaosl.so: cannot open shared object file`。
2. **`RtmConfig.eventHandler` 必填**，漏了直接返回 `-10004 RTM_ERROR_INVALID_EVENT_HANDLER`，
   且 `client == nullptr`，很容易误判成"网络问题"。
3. **`RtmConfig` 里带 `appId` 和 `userId`**，`login()` 只收 token——
   和我们「登录带昵称」的习惯不一样，昵称要另外映射（见 4.3）。

**已经把 SDK 落进项目并写了自检程序**（`tools/rtm_smoke.cpp`，CMake 目标 `rtm_smoke`）：

```bash
# 推荐：先把 appId / token / userId 填进 config/agora.json，然后直接跑
./build/bin/rtm_smoke

# 或者命令行覆盖
./build/bin/rtm_smoke --appid <APP_ID> --token <TOKEN> --user u1 --channel 3001
```

实测输出（用假 App ID）：

```
[ok] 客户端已创建 (userId=u1, areaCode=CN)
[event] link state 0 -> 1 (Perform login operation)
[event] login      code=-10003        ← App ID 无效，符合预期
[event] link state 1 -> 5 (Unknown)
```

`ldd build/bin/rtm_smoke` 里两个 `.so` 都从 `third_party/agora_rtm/lib` 解析成功，
说明 **CMake 接线和运行时加载都没问题，只差真实的 App ID / Token**。

---

## 2. RTM 能替代什么、不能替代什么

我们现在的服务端做了 6 件事，RTM 的覆盖情况：

| 现有能力 | RTM 2.x 对应 | 差距 |
| --- | --- | --- |
| 登录 / 身份 | `RtmConfig{appId, userId}` + `login(token)` | userId 有字符限制，中文昵称要映射 |
| 进房 / 退房 | `subscribe(channel)` / `unsubscribe` | ✅ 频道无需预建，正好对上「房间号即频道名」 |
| 发言广播 | `publish(channel, json)` | ✅ 单条有大小上限，我们 ≤4KB 无压力 |
| 成员列表 | **Presence**（控制台单独开关） | ✅ 但要按 Presence 事件重建，不能再读 `RoomState.members` |
| 历史消息 | **History / Storage**（单独开关，按量计费） | ⚠️ 免费额度内可用；也可客户端本地缓存 |
| 用户资料（年龄/描述） | **User Metadata** | ⚠️ 需自己定义字段与同步时机 |
| 心跳 / 断线重连 | SDK 内置，`onLinkStateEvent` 上报 | ✅ 可以删掉我们那套 3s×6 重连逻辑 |
| 房间「是否新建」 | ❌ 无此概念 | 用 Presence 人数近似（0 人 = 新建） |
| 局域网 / 断网可用 | ❌ 必须公网 | 这是最大代价 |

**一句话**：RTM 把「传输 + 连接管理」做得很完整，但**房间语义、成员模型、资料存储要我们重新组织**。
它不是「换个 socket」，而是「换一套会话模型」。

---

## 3. 三种接入方式

| | A. RTM 全替换 | **B. 传输层抽象 + 双模式（推荐）** | C. 只接 RTC 做语音 |
| --- | --- | --- | --- |
| 做法 | 删掉自研服务端，客户端只连 RTM | 抽 `ChatTransport` 接口，`ProtocolBridge`（自研）与 `RtmBridge`（声网）两种实现，配置切换 | 消息仍走自研服务端，只把语音通话交给声网 RTC |
| 工作量 | 2 天 | **2.5–3 天** | 1.5 天 |
| 内网可用 | ❌ | ✅ 可切回 | ✅ |
| 现有服务端 | 废弃 | 保留，仍可跑 21 项自检 | 保留 |
| 答辩演示 | 依赖公网 | 断网也能演，双模式是加分项 | 依赖公网做语音 |

**推荐 B**。理由：
- 你已经有一套能跑、有自检、有历史/重连的服务端，直接废弃很浪费；
- 教室/实验室断网时还能退回局域网模式演示；
- `ProtocolBridge` 的信号面（`loginSucceeded` / `roomJoined` / `chatReceived` / `historyReceived` / `systemMessage` / `networkError`）本来就很干净，抽接口成本低。

> 如果你确定以后只用公网、不要内网模式，那 A 更省事，我可以按 A 做。

---

## 4. 具体集成方案（按 B 展开）

### 4.1 控制台准备（一次性）

对照你给的那个控制台页面，按顺序做：

1. **创建项目**：通用项目 → 鉴权机制选 **调试模式**（测试阶段最省事，只需 App ID）
   或 **安全模式**（正规做法，需要 Token）。
2. **开通 RTM**：全部产品 → 实时消息 RTM → 功能配置 → 启用。
3. **按需开通**：
   - **Presence**（成员列表要用，v2.3.0 默认开）
   - **Storage / History**（历史消息要用，按量计费）
4. **拿 App ID**：总览 → 项目信息 → 复制。
5. **拿临时 Token**（安全模式才需要）：总览 → 临时 Token 生成器 → 添加产品选 RTM → 填 userId → 复制（**24 小时过期**）。

### 4.2 SDK 落地与构建

```
Dialogue_Pratice_Client/
└── third_party/agora_rtm/
    ├── include/          # IAgoraRtmClient.h 等 8 个头文件
    └── lib/
        ├── libagora_rtm_sdk.so
        └── libaosl.so
```

```cmake
# CMakeLists.txt
set(AGORA_RTM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/agora_rtm")

target_include_directories(dialogue_client PRIVATE ${AGORA_RTM_DIR}/include)
target_link_directories(dialogue_client PRIVATE ${AGORA_RTM_DIR}/lib)
target_link_libraries(dialogue_client PRIVATE agora_rtm_sdk aosl pthread)

# 关键：RUNPATH 不作用于间接依赖（libaosl），必须退回 DT_RPATH
target_link_options(dialogue_client PRIVATE
    "-Wl,--disable-new-dtags"
    "-Wl,-rpath,$ORIGIN/../third_party/agora_rtm/lib")
```

运行时把两个 `.so` 放到 `build/bin/../third_party/agora_rtm/lib`（或直接拷到 `build/bin`），
`ldd` 里不应出现 `not found`。

### 4.3 身份与配置

新建 `config/agora.json`，复用现有的 `AppConfig` 热重载机制：

```json
{
  "transport": "rtm",
  "rtm": {
    "appId": "",
    "token": "",
    "userIdPrefix": "u",
    "areaCode": "CN",
    "presenceEnabled": true,
    "historyEnabled": false
  },
  "rtc": {
    "token": "",
    "testChannel": "3001"
  }
}
```

`rtm.appId` 两个产品共用（同一个声网项目）；`rtm.token` 与 `rtc.token` 是两个独立 Token。

- `transport`：`"rtm"` 走声网，`"server"` 走自研服务端 —— 这就是 B 方案的开关。
- **userId 映射**：RTM 的 userId 有字符限制且不能含空格，我们的昵称是中文。
  做法：`userId = "u" + 本地生成的短 id`（或昵称的 CRC32），**昵称放在消息体和 User Metadata 里**。
- App ID / Token 都放配置，不硬编码；`config/agora.json` 加入 `.gitignore`。

### 4.4 代码结构

```
ChatTransport.h        # 抽象接口：登录/进房/发言/退出 + 6 个信号
├── ProtocolBridge     # 现有实现，改成继承 ChatTransport（几乎不改内部逻辑）
└── RtmBridge          # 新增：声网 RTM 实现
```

```cpp
class ChatTransport : public QObject {
    Q_OBJECT
public:
    virtual void login(const QString& name) = 0;
    virtual void joinRoom(const QString& room) = 0;
    virtual void leaveRoom() = 0;
    virtual void sendChat(const QString& text) = 0;
signals:
    void loginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    void roomJoined(const QString& room, bool created, const QVector<MemberInfo>& members);
    void chatReceived(qint64 seq, qint64 from, const QString& name,
                      const QString& room, const QString& text, qint64 ts);
    void historyReceived(const QString& room, const QVector<ChatMessage>& messages);
    void systemMessage(...);
    void networkError(const QString& message);
    void disconnected();
};
```

`RtmBridge` 的核心是三件事：

```cpp
// 1) 事件回调 → Qt 主线程（RTM 回调在 SDK 线程里！）
void RtmEventListener::onMessageEvent(const MessageEvent& e) override {
    emit bridge_->rawMessage(QString::fromUtf8(e.publisher),
                             QString::fromUtf8(e.message, int(e.messageLength)));
    // emit 跨线程自动走 QueuedConnection，UI 线程安全
}

// 2) 建客户端（注意 eventHandler 必填）
agora::rtm::RtmConfig cfg{};
cfg.appId = appId_.toUtf8().constData();
cfg.userId = userId_.toUtf8().constData();
cfg.eventHandler = &listener_;
int err = 0;
client_ = agora::rtm::createAgoraRtmClient(cfg, err);

// 3) 登录 → 订阅 → 发言
client_->login(token_.toUtf8().constData(), requestId);
client_->subscribe(room.toUtf8().constData(), agora::rtm::SubscribeOptions(), requestId);
agora::rtm::PublishOptions opt;
opt.messageType = agora::rtm::RTM_MESSAGE_TYPE_STRING;
client_->publish(room.toUtf8().constData(), json.constData(), size_t(json.size()), opt, requestId);
```

### 4.5 消息格式

RTM 只搬字符串，不需要我们那套 `4B 长度 + 1B opcode` 的帧头，直接用 JSON：

```json
{"t":"chat","seq":12,"name":"bob","text":"hi","ts":1793092534667}
{"t":"presence","event":"join","userId":"u7","name":"bob"}
```

`seq` 改为客户端本地递增 + `ts` 排序；`ChatPage` 的按 `seq` 去重逻辑要改成按
`(publisher, ts, text hash)` 去重。

### 4.6 状态映射

| 我们的 UI 状态 | RTM 事件 |
| --- | --- |
| 已连接 / 重连中 | `onLinkStateEvent`（`currentState`） |
| 登录成功 | `onLoginResult` |
| 成员进 / 出 | Presence `onJoin/onLeave` |
| 收到消息 | `onMessageEvent` |
| 发送成功 | `onPublishResult` |
| 进房成功 | `onSubscribeResult` |

我们自研的「3 秒 × 6 次重连」可以整段删掉，交给 SDK。

### 4.7 RTC 实时语音（已确认要做）

目标形态：

```
顶栏：[←] 房间 3001 / 2 位成员          [ 🎙 语音通话 ]  [成员 2]
点「语音通话」→ 加入 RTC 频道（频道名 = 房间号）→ 双方实时说话
通话中：谁在说话头像外圈高亮，可静音自己、可挂断
```

集成要点：

| 项 | 做法 |
| --- | --- |
| 初始化 | `IRtcEngine::initialize(RtcEngineContext{appId, eventHandler})` |
| 入会 | `joinChannel(token, roomName, 0, options)` —— 频道名直接用房间号 |
| 音频参数 | `setAudioProfile(AUDIO_PROFILE_SPEECH_STANDARD)` + `setAudioScenario(AUDIO_SCENARIO_CHATROOM)`，口语练习用 speech 更省带宽 |
| 成员 | `onUserJoined` / `onUserOffline` |
| 说话指示 | `onAudioVolumeIndication`（驱动头像外圈动画） |
| 静音 / 挂断 | `muteLocalAudioStream` / `leaveChannel` |
| 身份 | 与 RTM 共用同一个 App ID、**同一个 userId**；但 Token 要**分别生成两个**（见 7.1） |

#### 架构澄清：RTC 的"服务端"在哪

「RTC 应该放服务器、客户端只做采集和播放」这个直觉是对的——实时音视频确实需要一个中转节点
（NAT 穿透、混流、弱网抗丢包都靠它）。但**声网把这个中转做成了云服务**，不是你自己部署的东西：

```
自己搭：  客户端 ── PCM/Opus ──► 你的服务器（SFU/混流）──► 其他客户端
声网：    客户端 SDK ──► SD-RTN（声网的全球实时网）──► 其他客户端 SDK
                        ↑ 这就是"服务器"，你不用部署，也接不进去
```

所以：

- **媒体流不经过你的服务器**，你的服务器只做控制面：发 Token、管房间、可选录制/推流。
- **客户端 SDK 干的正是"采集 + 播放"**：麦克风采集、3A 处理、编码、发送、接收、解码、播放全包了。
  你说的"客户端只做采集和播放"就是它——只是传输这一段由 SDK 代劳，你不需要自己写。
- **声网确实有服务端 SDK**（「本地服务端录制 SDK」，Linux x86_64，包里就是 `libagora_rtc_sdk.so`），
  但它的用途是"加入频道 → 录制/转码 → 输出文件或推流到 CDN"，**不是把流转发给你自己的客户端**。
  想做"网关"（Linux 客户端 ↔ 声网频道）理论上可行，但要解码再编码，延迟和复杂度都上一个量级。

**对你这个项目反而是好消息**：你要的"服务端中转"，在**语音消息**方案里天然就有——
客户端 Opus 编码 → 发 RTM → 服务端（声网频道）分发 → 对方解码播放。
只是它是"半双工的消息"，不是"实时通话"。

**❌ 结论：RTC 在 Linux 桌面走不通，A5 取消。**

查了官方文档中心的「产品概述 → 平台支持」表，RTC SDK 支持：Android / iOS / macOS / HarmonyOS /
Web / Windows / Unity / Electron / Flutter / React Native / Unreal / React —— **没有 Linux**。

产品页宣传语里确实出现过「…macOS、Electron、Linux…」，但那个 Linux 指的是
**「本地服务端录制 SDK」**（跑在服务器上收流录制，不做麦克风采集与播放），不是桌面客户端 SDK。

因此语音能力走降级方案：

| 能力 | 做法 |
| --- | --- |
| 说英文 | 已有 whisper 本地识别 → 文本 → 发 RTM |
| 听发音 | 计划中的 Piper TTS（M2） |
| 发语音消息 | Opus 编码后作为 RTM 消息发送（不需要 RTC） |

如果将来一定要声网 RTC，客户端得换到 Windows / macOS。

---

## 5. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| **必须公网** | 断网/纯内网演示直接不可用 | B 方案保留 `transport: "server"` 兜底 |
| 体验套餐配额 | 超额后服务被关停 | 以控制台显示为准；演示前查一下余量 |
| 临时 Token 24h 过期 | 第二天突然登录失败 | 调试模式免 Token；或自建 Token 服务（见下） |
| userId 字符限制 | 中文昵称不能直接用 | `u<id>` 映射，昵称走消息体 |
| 历史消息要开 Storage | 不开就没有进房补历史 | 先关掉该功能（`historyEnabled: false`），用本地缓存 |
| 消息顺序 | RTM 不保证全局顺序 | 用 `ts` 排序，UI 已按时间排 |

**正式环境的 Token**：自研服务端加一个 `GET /rtm-token?userId=xxx` 接口，
用 App Certificate + HMAC-SHA256 按官方算法签发（官方有 C++/Python 示例），
客户端登录前先取 Token。测试阶段不必做。

---

## 6. 工作量与阶段

| 阶段 | 内容 | 验收 |
| --- | --- | --- |
| ~~**A1** 打通~~ ✅ | 控制台配置 + SDK 落地 + CMake + `rtm_smoke` | **已完成**：login/subscribe/publish 全部 code=0；u1↔u2 互通 |
| ~~**A2** 适配~~ ✅ | `ChatTransport` 抽象 + `RtmBridge` + `config/agora.json` 的 `transport` 切换 | **已完成**：GUI 内收发正常、成员数随 Presence 变化；自研模式回归通过 |
| ~~**A3** 补齐~~ ✅ | 历史改用**本地 SQLite 缓存**（不依赖计费的 Storage）；Presence 成员已在 A2 完成 | **已完成**：退出房间再进入，历史从本地补回（截图验证） |
| **A4** 回归 | 两种 transport 都跑一遍，确认自研模式没退化 | 服务端 21 项自检仍通过 |
| **A5** RTC 语音 | 通话按钮 + 入会/静音/挂断 + 说话指示 | 两台机器能实时对话（前提：Linux 包可用，见 4.7） |

预计 **2.5–3 天**（A1 半天，A2 一天，A3 一天，A4 半天）；RTC 视 Linux 包情况再加 **1–1.5 天**。

---

## 7. 已确认的决策

| 决策 | 结论 |
| --- | --- |
| 接入方式 | **双模式抽象**：`ChatTransport` 接口，自研服务端与声网两种实现，配置切换 |
| 产品范围 | **RTM（文字）+ RTC（实时语音）** |
| 鉴权 | **安全模式 + 临时 Token**（控制台生成，24 小时有效）；正式上线再自建 Token 服务 |

### 7.1 安全模式下的 Token 流程

**注意：临时 Token 生成器是「单选产品」的**，不能一个 Token 同时覆盖 RTM 和 RTC。
要生成两次，且两次的 **userId 必须一致**（否则两个产品认成两个人）：

```
① 产品选 RTM → 填 userId（如 u1）        → 得到 RTM Token
② 产品选 RTC → 填 userId（如 u1）+ 频道名 → 得到 RTC Token
                        ↓
        config/agora.json: rtm.token / rtc.token 分别填
                        ↓
        客户端：login(rtmToken) 与 joinChannel(rtcToken, 频道名)
```

**RTC Token 通常绑定频道名**：如果生成器要求填频道名，这个 Token 就只能进那个频道。
我们房间号是动态的，所以测试期要么固定用一个房间号（如 `3001`），
要么走正式方案——自研服务端加 `/agora-token?userId=xxx&channel=yyy` 接口，
用 App Certificate 现签。**建议测试期先用固定房间号 3001。**

两个 Token 都是 24 小时过期；过期后 `login` / `joinChannel` 会返回鉴权错误，
客户端提示「Token 已过期，请重新生成」。

### 7.2 下一步

我需要你提供 **App ID** 和 **临时 Token**（userId 随便填，例如 `u1`），
才能做 A1 的运行时验证。在此之前我可以先把 `ChatTransport` 抽象和 `RtmBridge` 骨架写完，
保证编译链接通过（SDK 已验证可用），但「真的能收发消息」要等凭证。
