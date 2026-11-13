# Android 真机打包

> 目标：把 Qt Widgets 客户端编译成 APK，装到 Android 手机上跑起来。
>
> 现状（2026-10-30）：**工具链已装好并验证**。
>
> | 组件 | 版本 | 状态 |
> | --- | --- | --- |
> | Qt | 6.12.0 LTS（`~/Qt/6.12.0/`，7.5 GB） | ✅ gcc_64 + 4 个 Android ABI |
> | JDK | Temurin 21.0.13（`~/opt/jdk-21.0.13+9`） | ✅ |
> | Android SDK | `~/Android/Sdk`，platforms;android-36 | ✅ |
> | NDK | r27c（`27.2.12479018`，2.0 GB） | ✅ |
> | CMake | 3.31.12（`~/opt/`，系统自带 3.22 不满足 Qt 6.12） | ✅ |
>
> **桌面端已用 Qt 6.12 编译通过**（123 个目标，零错误，代码零改动），
> 运行时链接 `~/Qt/6.12.0/gcc_64/lib/libQt6Core.so.6` 正常。
> 剩 opus 交叉编译和 APK 打包未做。
>
> 本文件记录完整步骤，可据此在新机器复现。

---

## 0. 先决策：在哪打包

| 方案 | 优点 | 缺点 |
| --- | --- | --- |
| **VM 里（推荐）** | 代码就在 `~/Code`，一条链路；Linux 下 Qt Android 工具链完整 | 装 JDK 需要 sudo；Qt 安装器要图形界面 |
| Windows 宿主 | 图形界面方便，Android Studio 成熟 | 代码要同步过去，维护两套环境 |

**推荐 VM**：只有「Qt 在线安装器」需要图形界面，而这一步的窗口也能从 SSH 用
`DISPLAY=:0` 直接投到桌面（VM 有 GNOME + gdm3，X 无鉴权限制），**唯一必须人工的是输 Qt 账号密码**。
其余全部可在 SSH 里完成。

> 提醒：VM 内存只有 7.7 GB（可用 5.3 GB）。Gradle 构建时若报 OOM，
> 在 `android/gradle.properties` 里把 `org.gradle.jvmargs` 调到 `-Xmx1536m`。

---

## 1. 装 JDK 21（不需要 sudo）

> **Qt 6.12 要求 JDK 21**（6.2.4 时代是 17，别混了）。
> apt 装 JDK 需要 sudo 密码，用免 root 的 tar 包更省事。

```bash
mkdir -p ~/opt && cd ~/opt

# Adoptium 官方 API，自动取最新 21 GA
curl -L 'https://api.adoptium.net/v3/binary/latest/21/ga/linux/x64/jdk/hotspot/normal/eclipse' \
     -o jdk21.tar.gz
tar xzf jdk21.tar.gz

export JAVA_HOME="$HOME/opt/$(ls -d jdk-21* | head -1)"
export PATH="$JAVA_HOME/bin:$PATH"

java -version   # 应输出 openjdk version "21.x"
```

把 `JAVA_HOME` 和 `PATH` 写进 `~/.bashrc`，否则新开终端会丢。

> 若坚持用 apt：Ubuntu 22.04 的 `openjdk-21-jdk` 在 `jammy-updates` 里有，
> 但装完记得 `update-alternatives` 选到 21，别让 17 抢了。

---

## 1.5 装 CMake 3.25+（Qt 6.12 的硬要求）

系统自带的 3.22.1 **不够**。不换的话 `find_package(Qt6)` 直接报错：

```
CMake Error at .../QtPublicCMakeVersionHelpers.cmake:96 (message):
  CMake 3.25 or higher is required to use Qt.  You are running version 3.22.1
```

官方留了 `-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=3.22` 的降级开关，但标着
"not officially supported … Use at your own risk"，不建议赌。装新的：

```bash
cd ~/opt
curl -sL -o cmake.tar.gz https://cmake.org/files/v3.31/cmake-3.31.12-linux-x86_64.tar.gz
tar xzf cmake.tar.gz
export PATH="$HOME/opt/cmake-3.31.12-linux-x86_64/bin:$PATH"   # 必须排在 /usr/bin 前
```

验证：`cmake --version` → `cmake version 3.31.12`

---

## 2. 装 Android SDK + NDK

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools"
cd "$ANDROID_SDK_ROOT/cmdline-tools"

# 版本号会更新，去 https://developer.android.com/studio#command-tools 拿最新的 linux 包
curl -LO https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip -q commandlinetools-linux-*.zip
mv cmdline-tools latest        # 目录名必须是 latest，否则 sdkmanager 找不到自己

export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$PATH"

yes | sdkmanager --licenses
sdkmanager "platform-tools" "platforms;android-36" "build-tools;36.0.0" "ndk;27.2.12479018"
```

**NDK 版本很关键**：Qt 6.12 官方用 **NDK r27c**（`27.2.12479018`）构建，
用别的版本会在 `androiddeployqt` 阶段报找不到工具链或 missing symbol。
`build-tools` 具体版本以 `sdkmanager --list | grep build-tools` 为准，不匹配时让 Gradle 自己拉也行。

装完检查：

```bash
ls "$ANDROID_SDK_ROOT/ndk/"          # 应该有 27.2.12479018
"$ANDROID_SDK_ROOT/platform-tools/adb" version
```

> **16 KB page size**：NDK r27+ 默认按 16 KB 对齐，正好满足 Google Play 2025-11 起的强制要求。
> 这也是必须放弃 6.2.4 / NDK 23 的核心理由。

---

## 3. 装 Qt for Android（需要图形界面）

apt 的 `qt6-base-dev` **不含 Android 支持**，必须用 Qt 在线安装器。

> 文件名注意：官方已改名为 `qt-online-installer-linux-x64-online.run`。
> 旧的 `qt-unified-linux-x64-online.run` 现在返回 **404**，别再用了。

```bash
cd ~/Downloads
curl -LO https://download.qt.io/official_releases/online_installers/qt-online-installer-linux-x64-online.run
chmod +x qt-online-installer-linux-x64-online.run
./qt-online-installer-linux-x64-online.run
```

VM 有 GNOME 桌面（`graphical.target` + gdm3），两种方式都行：

- **在 VMware 窗口里的终端直接跑**上一步。
- **在 SSH 里启动、窗口投到桌面**（X 无鉴权限制，`DISPLAY=:0 xdpyinfo` 可直接通）：

  ```bash
  cd ~/Downloads && setsid env DISPLAY=:0 ./qt-online-installer-linux-x64-online.run \
      > installer.log 2>&1 < /dev/null &
  ```

  窗口会出现在 VMware 桌面里。注意 `setsid` 必须带，否则 SSH 断开安装器就被杀。

安装器里：

1. 登录（或注册）Qt 账号——免费账号即可。**这一步只能人工做**，需要输密码。
2. 选 **Custom installation**
3. 展开 `Qt` → **`Qt 6.12.0`**（当前 LTS，2026-09-30 发布，**不用开 Archive**，最新版直接可见），勾选：
   - **Desktop gcc 64-bit** —— 桌面端开发也用这个版本
   - **Android** → **Android ARM64_v8a** —— 真机用
4. `Sources` / `Qt Creator` / QML 相关一律不勾，体积约 4–6 GB

> 为什么是 6.12 而不是 6.2.4：6.2 开源版已 EOL、只支持到 NDK 23、**不支持 16 KB page size**
> （Google Play 2025-11 起强制），且 Android 15/16 的 16 KB 内核设备上可能直接加载失败。
> 6.12 LTS 支持到 2031，NDK r27c，targetSdk 36。
>
> Ubuntu 22.04 在官方支持列表内，预编译包链接 glibc 2.34，本机 2.35 满足。
> 装完**立刻**验证：`~/Qt/6.12.0/gcc_64/bin/qmake --version`，
> 若报 `GLIBC_2.39 not found` 才需要升系统或源码编译。

装完应该是：

```
~/Qt/6.12.0/android_arm64_v8a/bin/qt-cmake
~/Qt/6.12.0/gcc_64/bin/qt-cmake
```

---

## 4. 交叉编译 opus

Android 没有系统 opus，必须自己编 arm64 静态库：

```bash
export TOOLCHAIN="$ANDROID_SDK_ROOT/ndk/27.2.12479018/toolchains/llvm/prebuilt/linux-x86_64"
mkdir -p ~/build && cd ~/build

curl -LO https://downloads.xiph.org/releases/opus/opus-1.3.1.tar.gz
tar xzf opus-1.3.1.tar.gz && cd opus-1.3.1

./configure \
    --host=aarch64-linux-android \
    --with-sysroot="$TOOLCHAIN/sysroot" \
    CC="$TOOLCHAIN/bin/aarch64-linux-android28-clang" \
    CXX="$TOOLCHAIN/bin/aarch64-linux-android28-clang++" \
    --prefix="$HOME/Android/deps/arm64" \
    --disable-shared --enable-static

make -j"$(nproc)" && make install
```

产物：`~/Android/deps/arm64/lib/libopus.a`。

> `aarch64-linux-android28-clang` 里的 `28` 是最低 API 等级，要和
> 构建脚本里的 `ANDROID_PLATFORM=android-28` 一致（Qt 6.12 最低支持 API 28，
> 6.2.4 时代是 24，已上调）。
>
> 编完**不要在静态库上查 16 KB 对齐**：`.a` 里的 `.o` 是 REL 类型，没有 LOAD 段，
> `readelf -l` 什么也查不到（踩过这个坑）。对齐要在最终产物上验证：
>
> ```bash
> llvm-readelf -l build-android/bin/dialogue_client | grep LOAD
> # 最后一列应为 0x4000，不是 0x1000
> ```
>
> 一键脚本：`./scripts/build_opus_android.sh`（已包含下载、配置、编译、安装）。

---

## 5. 构建 APK

```bash
cd ~/Code/Dialogue_Pratice_Client
./scripts/build_android.sh          # 配置 + 编译 + 打包 + 签名，一条龙
```

脚本会自动定位 Qt / SDK / NDK / JDK / build-tools，缺失时给出明确提示。

### 5.1 三个必须知道的点

**① 必须用 `qt_add_executable`，不能用 `add_executable`**

这是最容易卡住的地方。用 `add_executable` 时，`--target apk` 会是个**空 target**：

```
== 构建 APK ==
Built target apk            ← 什么都没编，也没有 APK
```

因为只有 `qt_add_executable` 会给目标挂上 `_qt_internal_android_executable_finalizer`
（调用 androiddeployqt 生成 Gradle 工程）。项目已改：

```cmake
qt_add_executable(dialogue_client
    main.cpp
    ...
)
```

桌面端行为不变，可以放心用。

**② 交叉编译时 `find_library` 会被 sysroot 限制**

Android toolchain 设了 `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY`，
`-DCMAKE_PREFIX_PATH=$ANDROID_DEPS` 对 `find_library` 无效，会报
「缺少 libopus」。必须加 `NO_CMAKE_FIND_ROOT_PATH`：

```cmake
find_library(OPUS_LIBRARY NAMES opus
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
    NO_CMAKE_FIND_ROOT_PATH)
```

头文件同理，要从库路径反推 `.../include` 并显式加进 `target_include_directories`。

**③ 产物是 unsigned，必须签名才能装**

androiddeployqt 产出 `*-unsigned.apk`，`adb install` 会直接拒绝。
脚本已经自动用 debug keystore 签名（`~/.android/debug.keystore`，
首次运行自动生成，口令 `android`），并用 `zipalign -p` 做 16 KB 页对齐。

### 5.2 脚本内部等价命令

```bash
~/Qt/6.12.0/android_arm64_v8a/bin/qt-cmake -S . -B build-android \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT \
    -DANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/27.2.12479018 \
    -DANDROID_PLATFORM=android-28 \
    -DCMAKE_PREFIX_PATH=$HOME/Android/deps/arm64 \
    -DDP_WITH_WHISPER=OFF -DDP_WITH_TTS=OFF

cmake --build build-android --target apk
```

产物：`build-android/android-build/build/outputs/apk/release/`

### 5.3 实测结果（2026-10-30）

| 项 | 值 |
| --- | --- |
| APK | `android-build-release-signed.apk`，**53.5 MB** |
| ABI | 仅 `arm64-v8a` |
| package | `org.qtproject.example.dialogue_client`（**建议改成自己的**） |
| targetSdk | 37，compileSdk 36 |
| 16 KB 对齐 | ✅ 主程序与全部 Qt `.so` 的 LOAD Align 均为 `0x4000` |
| 打包耗时 | Gradle 约 1m18s（首次含依赖下载） |

改包名（上架前必做）：

```cmake
set_target_properties(dialogue_client PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.yourcompany.dialoguepratice")
```

---

## 6. 装到手机

### 方式 A：adb over WiFi（推荐，不用 USB 直通）

手机先开「开发者选项 → 无线调试」，然后：

```bash
# 手机和 VM 要在同一网段
adb connect 192.168.x.x:5555
adb devices                    # 应看到设备
adb install -r build-android/.../android-build-release-signed.apk
```

### 方式 B：VM 直连 USB

VMware → 虚拟机设置 → USB 控制器 → 勾选「显示所有 USB 设备」，
然后 VM 菜单 → 可移动设备 → 连接手机。

```bash
adb devices        # 需要手机已开 USB 调试
```

### 方式 C：拷到 Windows 安装（最省事）

VM 里把 APK 拷到共享文件夹，Windows 上用 platform-tools 安装：

```powershell
adb install -r D:\share\android-build-release-signed.apk
```

---

## 7. 语音功能的处理

这是 Android 打包里**最麻烦**的部分：

| 组件 | 依赖 | Android 状态 |
| --- | --- | --- |
| `VoiceCodec` | opus | ✅ 交叉编译后可用 |
| `VoicePlayer` | opus + QSoundEffect | ✅ 可用 |
| `VoiceRecorder` | QAudioSource + opus | ✅ **录音发语音可用** |
| `WhisperWorker` | whisper.cpp | ❌ 需为 arm64 重编 |
| `TtsService` | Piper（外部进程） | ❌ Android 上跑不了 |

**两个开关已实现**（2026-10-30）：

- `CMakeLists.txt` 里 `option(DP_WITH_WHISPER ... ON)` / `option(DP_WITH_TTS ... ON)`，
  关闭时不 `add_subdirectory(whisper)`、不链接 `whisper`、不定义宏；
- `WhisperWorker.cpp` 用 `#ifdef DP_WITH_WHISPER` 包住真实现，`#else` 分支提供空实现
  （`initialize()` 发 `initialized(false, ...)`，`transcribe()` 发 `failed(...)`）；
- `TtsService.cpp` 用 `#ifdef DP_WITH_TTS`，空实现里 `isAvailable()` 返回 `false`，
  UI 自动隐藏朗读按钮；
- `tools/` 下的诊断工具整体放进 `if(NOT ANDROID)`，不拖慢 APK 构建。

**接口签名完全不变**，`main.cpp` / `ChatPage.cpp` 一行没改。
桌面端默认两个开关都是 ON，实测 123 个目标编译零错误。

**效果**：Android 上能录音发语音、能听对方语音，但不能本地转写、不能 TTS。
对移动端来说这是合理的取舍——手机上本来就更适合打字。

---

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `--target apk` 输出 `Built target apk` 但**没有 APK、也没编译任何源文件** | 用了 `add_executable` | 改用 `qt_add_executable`（见 5.1①） |
| `Could not locate Android SDK build tools under ".../build-tools"` | build-tools 没装上（多个包一起装时 sdkmanager 会静默跳过） | `sdkmanager "build-tools;36.0.0"`，用 `ls $ANDROID_SDK_ROOT/build-tools` 确认 |
| `CMake Error: 缺少 libopus`（交叉编译时） | `find_library` 被 `CMAKE_FIND_ROOT_PATH` 限制在 sysroot | 加 `NO_CMAKE_FIND_ROOT_PATH`（见 5.1②） |
| `fatal error: 'opus/opus.h' file not found` | 只找到了库，没加 include 目录 | 从库路径反推 `.../include` 加进 `target_include_directories` |
| `CMake 3.25 or higher is required to use Qt` | 系统 cmake 3.22 太旧 | 装 CMake 3.31（见 1.5 节），注意 PATH 要排在 `/usr/bin` 前 |
| `androiddeployqt: error: Could not find qmake` | 用错了 Qt kit | 确认用的是 `~/Qt/6.12.0/android_arm64_v8a/bin/qt-cmake` |
| `No such file: libc++_shared.so` | NDK 版本不匹配 | 必须用 NDK `27.2.12479018` |
| Gradle 报 `Unsupported class file major version` | JDK 版本不对 | Qt 6.12 用 **JDK 21**（6.2.4 时代才是 17） |
| 构建时 OOM | VM 内存 7.7 GB | `gradle.properties` 里 `-Xmx1536m` |
| `adb install` 报 `INSTALL_PARSE_FAILED_NO_CERTIFICATES` | APK 未签名 | 用 `apksigner` 签（脚本已自动处理） |
| 装到手机后闪退 | 缺 arm64 库 | `adb logcat \| grep -i "cannot locate"` |
| 应用启动白屏 | Qt 插件未打包 | 检查 `build-android/android-build/libs/arm64-v8a/` 有没有 `libplugins_platforms_*.so` |
| 静态库上查 16 KB 对齐查不到 | `.a` 里的 `.o` 是 REL，无 LOAD 段 | 在最终 `.so` 或 APK 里解出的 `.so` 上查 |

---

## 现在能做的验证（不需要 Android 环境）

UI 适配已经可以在桌面模拟：

```bash
# 手机竖屏尺寸
Xvfb :99 -screen 0 400x800x24 &
DISPLAY=:99 ./build/bin/dialogue_client
```

窗口会自动缩到 360×720，走紧凑布局（成员列表变弹窗、气泡按宽度收缩）。
