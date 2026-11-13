# Dialogue Pratice 客户端（Qt6）
#
# 分层结构：
#   core/      基础设施（配置、日志、主题样式）
#   net/       网络层（传输抽象、TCP 实现、连接管理）
#   media/     媒体层（录音、编解码、播放、语音识别、语音合成）
#   business/  业务层（消息模型、LLM、翻译、建议、本地历史）
#   ui/        界面层（窗口、页面、气泡控件）
#
# 依赖方向：ui -> business -> media/net -> core。反向依赖一律禁止。
# 只有 main.cpp 可以直接接触所有层（它负责组装）。

cmake_minimum_required(VERSION 3.16)
project(DialoguePraticeClient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia Sql)

# ---- 可选能力开关 ----------------------------------------------------------
# Android 交叉编译时用 -DDP_WITH_WHISPER=OFF -DDP_WITH_TTS=OFF 关掉：
# whisper.cpp 和 Piper 都是桌面二进制/本地推理，Android 上要额外交叉编译，
# 关闭后对应 .cpp 编译成空实现（接口不变，isAvailable()/initialize() 返回 false），
# 调用方无需改动。
option(DP_WITH_WHISPER "启用本地语音识别（whisper.cpp）" ON)
option(DP_WITH_TTS     "启用本地语音合成（Piper）"       ON)

# Opus：语音消息编码（20 秒语音约 30 KB，Base64 后仍能塞进单帧）
# 交叉编译时 Android toolchain 把 CMAKE_FIND_ROOT_PATH 锁在 sysroot 内，
# 默认会无视 CMAKE_PREFIX_PATH（$ANDROID_DEPS），必须加 NO_CMAKE_FIND_ROOT_PATH。
find_library(OPUS_LIBRARY NAMES opus
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
    NO_CMAKE_FIND_ROOT_PATH)
if(NOT OPUS_LIBRARY)
    message(FATAL_ERROR
        "缺少 libopus。\n"
        "  桌面端：sudo apt install libopus-dev\n"
        "  Android：先跑 ./scripts/build_opus_android.sh")
endif()
message(STATUS "Opus: ${OPUS_LIBRARY}")

# 从库路径反推头文件目录（.../lib/libopus.a -> .../include/opus/opus.h）。
# 桌面端 opus 装在 /usr/include 走系统路径，这里为空不影响；
# Android 交叉编译时头文件在 $ANDROID_DEPS/include，必须显式加。
set(OPUS_INCLUDE_DIR "")
get_filename_component(_opus_libdir "${OPUS_LIBRARY}" DIRECTORY)
get_filename_component(_opus_prefix "${_opus_libdir}" DIRECTORY)
if(EXISTS "${_opus_prefix}/include/opus/opus.h")
    set(OPUS_INCLUDE_DIR "${_opus_prefix}/include")
    message(STATUS "Opus includes: ${OPUS_INCLUDE_DIR}")
endif()

# ---- 语音识别：whisper.cpp（本地推理，模型在 models/ 下）--------------------
if(DP_WITH_WHISPER)
    set(WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp")
    if(NOT EXISTS "${WHISPER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到 third_party/whisper.cpp。请先执行：\n"
            "  cd ${CMAKE_CURRENT_SOURCE_DIR}/third_party\n"
            "  git clone --depth 1 https://github.com/ggerganov/whisper.cpp.git\n"
            "或加 -DDP_WITH_WHISPER=OFF 关闭语音识别。")
    endif()

    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
    add_subdirectory(${WHISPER_DIR} ${CMAKE_BINARY_DIR}/whisper EXCLUDE_FROM_ALL)
else()
    set(WHISPER_DIR "")
    message(STATUS "DP_WITH_WHISPER=OFF：语音识别编译为空实现")
endif()

if(DP_WITH_WHISPER)
    set(DP_MODEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/models/ggml-base.en.bin")
else()
    message(STATUS "DP_WITH_WHISPER=OFF：语音识别编译为空实现")
endif()

if(NOT DP_WITH_TTS)
    message(STATUS "DP_WITH_TTS=OFF：语音合成编译为空实现")
endif()

# 共享协议层：直接引用服务端工程的 common/，保证协议只有一份实现。
set(SHARED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Dialogue_Pratice"
    CACHE PATH "Dialogue_Pratice 工程根目录（提供 common/ 与 third_party/）")

if(NOT EXISTS "${SHARED_ROOT}/common/Protocol.h")
    message(FATAL_ERROR
        "未找到共享协议层：${SHARED_ROOT}/common/Protocol.h\n"
        "请确认 Dialogue_Pratice_Client 与 Dialogue_Pratice 位于同一父目录，"
        "或通过 -DSHARED_ROOT=<path> 指定。")
endif()

# ---- 各层源文件 ------------------------------------------------------------
set(CORE_SOURCES
    core/AppConfig.cpp      core/AppConfig.h
    core/Logger.cpp         core/Logger.h
    core/Theme.h
)

set(NET_SOURCES
    net/ChatTransport.h
    net/ProtocolBridge.cpp  net/ProtocolBridge.h
    net/ConnectionManager.cpp net/ConnectionManager.h
)

set(MEDIA_SOURCES
    media/VoiceCodec.cpp    media/VoiceCodec.h
    media/VoiceRecorder.cpp media/VoiceRecorder.h
    media/VoicePlayer.cpp   media/VoicePlayer.h
    media/WhisperWorker.cpp media/WhisperWorker.h
    media/TtsService.cpp    media/TtsService.h
)

set(BUSINESS_SOURCES
    business/ChatTypes.h
    business/ChatModel.cpp          business/ChatModel.h
    business/ChatController.cpp     business/ChatController.h
    business/LlmClient.cpp          business/LlmClient.h
    business/TranslationService.cpp business/TranslationService.h
    business/ReplyService.cpp       business/ReplyService.h
    business/MessageStore.cpp       business/MessageStore.h
    business/SessionContext.cpp     business/SessionContext.h
)

set(UI_SOURCES
    ui/MainWindow.cpp       ui/MainWindow.h
    ui/LoginPage.cpp        ui/LoginPage.h
    ui/HomePage.cpp         ui/HomePage.h
    ui/ProfileDialog.cpp    ui/ProfileDialog.h
    ui/RegisterDialog.cpp   ui/RegisterDialog.h
    ui/ChatPage.cpp         ui/ChatPage.h
    ui/widgets/MessageBubble.cpp   ui/widgets/MessageBubble.h
    ui/widgets/VoiceBar.cpp        ui/widgets/VoiceBar.h
    ui/widgets/TranslationCard.cpp ui/widgets/TranslationCard.h
    ui/widgets/SuggestionBar.cpp   ui/widgets/SuggestionBar.h
    ui/widgets/Avatar.cpp          ui/widgets/Avatar.h
    ui/widgets/SendButton.cpp      ui/widgets/SendButton.h
    ui/widgets/MicButton.cpp       ui/widgets/MicButton.h
    ui/widgets/PlayButton.cpp      ui/widgets/PlayButton.h
)

# 必须用 qt_add_executable 而不是 add_executable：
# 只有它才会给目标挂上 Android/iOS 的打包逻辑（androiddeployqt）。
# 用 add_executable 时 `--target apk` 会是个空 target，直接 "Built target apk"
# 却什么也不产出。桌面端行为不变。
qt_add_executable(dialogue_client
    main.cpp
    ${CORE_SOURCES}
    ${NET_SOURCES}
    ${MEDIA_SOURCES}
    ${BUSINESS_SOURCES}
    ${UI_SOURCES}
    ${SHARED_ROOT}/common/Protocol.cpp
)

target_include_directories(dialogue_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHARED_ROOT}
    ${SHARED_ROOT}/third_party
)
if(OPUS_INCLUDE_DIR)
    target_include_directories(dialogue_client PRIVATE ${OPUS_INCLUDE_DIR})
endif()
if(DP_WITH_WHISPER)
    target_include_directories(dialogue_client PRIVATE ${WHISPER_DIR}/include)
endif()

target_compile_definitions(dialogue_client PRIVATE
    DP_DEFAULT_MODEL_PATH="${DP_MODEL_PATH}"
    DP_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
    DP_PIPER_BIN="${CMAKE_CURRENT_SOURCE_DIR}/third_party/piper/piper"
    DP_PIPER_MODEL="${CMAKE_CURRENT_SOURCE_DIR}/models/en_US-lessac-medium.onnx"
)
if(DP_WITH_WHISPER)
    target_compile_definitions(dialogue_client PRIVATE DP_WITH_WHISPER)
endif()
if(DP_WITH_TTS)
    target_compile_definitions(dialogue_client PRIVATE DP_WITH_TTS)
endif()

target_link_libraries(dialogue_client PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Sql
    ${OPUS_LIBRARY}
)
if(DP_WITH_WHISPER)
    target_link_libraries(dialogue_client PRIVATE whisper)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dialogue_client PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- 诊断工具 --------------------------------------------------------------
# 这些是独立小工具，只链接它们真正需要的层，不参与主程序。
# Android 交叉编译时整体跳过：它们是桌面调试用的，没必要拖慢 APK 构建。
if(NOT ANDROID)
    # 验证 TTS 反复播放（rapid 模式模拟快速连点）
    if(DP_WITH_TTS)
        add_executable(tts_repeat tools/tts_repeat.cpp media/TtsService.cpp media/TtsService.h)
        target_include_directories(tts_repeat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(tts_repeat PRIVATE Qt6::Multimedia Qt6::Gui Qt6::Core)
        target_compile_definitions(tts_repeat PRIVATE
            DP_WITH_TTS
            DP_PIPER_BIN="${CMAKE_CURRENT_SOURCE_DIR}/third_party/piper/piper"
            DP_PIPER_MODEL="${CMAKE_CURRENT_SOURCE_DIR}/models/en_US-lessac-medium.onnx")
    endif()

    # 打印 AppConfig 解析结果（验证翻译专用模型配置）
    add_executable(config_probe tools/config_probe.cpp core/AppConfig.cpp core/AppConfig.h)
    target_include_directories(config_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(config_probe PRIVATE Qt6::Core)
    target_compile_definitions(config_probe PRIVATE
        DP_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config")

    # Opus 编解码自检
    add_executable(voice_codec_test tools/voice_codec_test.cpp media/VoiceCodec.cpp media/VoiceCodec.h)
    target_include_directories(voice_codec_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(voice_codec_test PRIVATE Qt6::Core ${OPUS_LIBRARY})

    # 把 WAV 编成协议用的 Base64（给 peer_bot 发语音用）
    add_executable(voice_make tools/voice_make.cpp media/VoiceCodec.cpp media/VoiceCodec.h)
    target_include_directories(voice_make PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(voice_make PRIVATE Qt6::Core ${OPUS_LIBRARY})
endif()
