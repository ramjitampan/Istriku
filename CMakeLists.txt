cmake_minimum_required(VERSION 3.21)

project(Yuki LANGUAGES C CXX)

# =====================================================
# C++ Standard
# =====================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =====================================================
# Output
# =====================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =====================================================
# Compiler
# =====================================================
if(MSVC)
    add_compile_options(/W4 /wd"4100" /wd"4189")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# =====================================================
# Windows
# =====================================================
add_compile_definitions(
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# =====================================================
# whisper.cpp (disable GPU backends, build as static libs)
# =====================================================
set(GGML_CUDA    OFF CACHE BOOL "" FORCE)
set(GGML_METAL   OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN  OFF CACHE BOOL "" FORCE)
set(GGML_OPENCL  OFF CACHE BOOL "" FORCE)
set(GGML_SYCL    OFF CACHE BOOL "" FORCE)
set(GGML_BLAS    OFF CACHE BOOL "" FORCE)
set(GGML_COREML  OFF CACHE BOOL "" FORCE)
set(GGML_OPENVINO OFF CACHE BOOL "" FORCE)
set(GGML_HEXAGON OFF CACHE BOOL "" FORCE)
set(GGML_RPC     OFF CACHE BOOL "" FORCE)
set(GGML_VIRTGPU OFF CACHE BOOL "" FORCE)
set(GGML_WEBGPU  OFF CACHE BOOL "" FORCE)
set(GGML_ZDNN    OFF CACHE BOOL "" FORCE)
set(GGML_ZENDNN  OFF CACHE BOOL "" FORCE)
set(GGML_CANN    OFF CACHE BOOL "" FORCE)
set(GGML_MUSA    OFF CACHE BOOL "" FORCE)
set(GGML_HIP     OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP  OFF CACHE BOOL "" FORCE)

set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)

# =====================================================
# Source Files
# =====================================================
set(SOURCES

    src/main.cpp

    src/core/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/TrayWindow.cpp

    src/config/Config.cpp
    src/config/ConfigManager.cpp

    src/utils/Logger.cpp
    src/utils/HttpClient.cpp
    src/utils/Json.cpp
    src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp
    src/utils/AudioResampler.cpp
    src/utils/StringFuzzy.cpp

    src/audio/MfccExtractor.cpp
    src/audio/SpeakerVerifier.cpp

    src/ai/OllamaClient.cpp

    src/chat/ChatController.cpp
    src/chat/Conversation.cpp
    src/chat/PromptBuilder.cpp

    src/memory/SQLiteMemoryStorage.cpp
    src/memory/MemoryExtractor.cpp
    src/memory/MemoryManager.cpp
    src/memory/MemoryRules.cpp

    src/system/WindowsController.cpp
    src/system/IntentDetector.cpp

    src/voice/WhisperRecognizer.cpp
    src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp
    src/voice/VoiceSession.cpp

    third_party/sqlite/sqlite3.c
)

# =====================================================
# Executables
# =====================================================
add_executable(Yuki ${SOURCES})

add_executable(OllamaTest
    tests/OllamaClientTest.cpp
    src/ai/OllamaClient.cpp src/utils/HttpClient.cpp
    src/utils/Json.cpp src/utils/Logger.cpp
)
add_executable(MemoryTest
    tests/MemoryTest.cpp
    src/memory/SQLiteMemoryStorage.cpp src/utils/Logger.cpp
    third_party/sqlite/sqlite3.c
)
add_executable(ExtractorTest
    tests/ExtractorTest.cpp
    src/memory/MemoryExtractor.cpp
)
add_executable(RetrieverTest
    tests/RetrieverTest.cpp
    src/memory/MemoryManager.cpp src/memory/MemoryRetriever.cpp
    src/memory/SQLiteMemoryStorage.cpp src/utils/Logger.cpp
    third_party/sqlite/sqlite3.c
)
add_executable(PromptBuilderTest
    tests/PromptBuilderTest.cpp
    src/chat/PromptBuilder.cpp src/chat/Conversation.cpp
    src/memory/MemoryManager.cpp src/memory/SQLiteMemoryStorage.cpp
    src/utils/Logger.cpp third_party/sqlite/sqlite3.c
)
add_executable(MemoryRulesTest
    tests/MemoryRulesTest.cpp
    src/memory/MemoryRules.cpp
)
add_executable(SystemTest
    tests/SystemTest.cpp
    src/system/WindowsController.cpp src/system/IntentDetector.cpp
    src/utils/Logger.cpp
)

# Voice tests
add_executable(WakeWordTest
    tests/WakeWordTest.cpp
    src/voice/WakeWordDetector.cpp
)
add_executable(TextToSpeechTest
    tests/TextToSpeechTest.cpp
    src/voice/TextToSpeech.cpp
)
add_executable(VoiceRecognizerTest
    tests/VoiceRecognizerTest.cpp
    src/voice/WhisperRecognizer.cpp src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp src/voice/VoiceSession.cpp
    src/utils/Logger.cpp src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp src/utils/AudioResampler.cpp
    src/audio/MfccExtractor.cpp src/audio/SpeakerVerifier.cpp
)
add_executable(VoiceSessionTest
    tests/VoiceSessionTest.cpp
    src/voice/WhisperRecognizer.cpp src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp src/voice/VoiceSession.cpp
    src/utils/Logger.cpp src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp src/utils/AudioResampler.cpp
    src/audio/MfccExtractor.cpp src/audio/SpeakerVerifier.cpp
)

# =====================================================
# Include Directories
# =====================================================
target_include_directories(Yuki PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/sqlite
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)
target_include_directories(OllamaTest PRIVATE src third_party)
target_include_directories(MemoryTest PRIVATE src third_party third_party/sqlite)
target_include_directories(ExtractorTest PRIVATE src)
target_include_directories(RetrieverTest PRIVATE src third_party third_party/sqlite)
target_include_directories(PromptBuilderTest PRIVATE src third_party third_party/sqlite)
target_include_directories(MemoryRulesTest PRIVATE src)
target_include_directories(SystemTest PRIVATE src)
target_include_directories(WakeWordTest PRIVATE src)
target_include_directories(TextToSpeechTest PRIVATE src)
target_include_directories(VoiceRecognizerTest PRIVATE
    src
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)
target_include_directories(VoiceSessionTest PRIVATE
    src
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)

add_executable(ConfigTest
    tests/ConfigTest.cpp
    src/config/ConfigManager.cpp src/config/Config.cpp
    src/utils/Logger.cpp
)
target_include_directories(ConfigTest PRIVATE
    src
    third_party
    third_party/nlohmann
)

# =====================================================
# Link Libraries
# =====================================================
target_link_libraries(Yuki PRIVATE
    user32 gdi32 winhttp gdiplus ole32 oleaut32 uuid powrprof winmm sapi advapi32 shell32
    whisper ggml
)
target_link_libraries(OllamaTest PRIVATE user32 gdi32 winhttp)
target_link_libraries(MemoryTest PRIVATE user32 gdi32)
target_link_libraries(RetrieverTest PRIVATE user32 gdi32)
target_link_libraries(PromptBuilderTest PRIVATE user32 gdi32)
target_link_libraries(SystemTest PRIVATE user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32)
target_link_libraries(WakeWordTest PRIVATE user32 gdi32)
target_link_libraries(TextToSpeechTest PRIVATE user32 gdi32 ole32 sapi)

target_link_libraries(VoiceRecognizerTest PRIVATE
    user32 gdi32 ole32 sapi winmm
    whisper ggml
)
target_link_libraries(VoiceSessionTest PRIVATE
    user32 gdi32 ole32 sapi winmm
    whisper ggml
)

add_executable(VoiceSessionFlowTest
    tests/VoiceSessionFlowTest.cpp
    src/voice/WhisperRecognizer.cpp src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp src/voice/VoiceSession.cpp
    src/utils/Logger.cpp src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp src/utils/AudioResampler.cpp
    src/audio/MfccExtractor.cpp src/audio/SpeakerVerifier.cpp
)
target_include_directories(VoiceSessionFlowTest PRIVATE
    src
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)
target_link_libraries(VoiceSessionFlowTest PRIVATE
    user32 gdi32 ole32 sapi winmm
    whisper ggml
)

add_executable(BrightnessIntentTest
    tests/BrightnessIntentTest.cpp
    src/system/IntentDetector.cpp src/system/WindowsController.cpp
    src/utils/Logger.cpp
)
target_include_directories(BrightnessIntentTest PRIVATE
    src
    third_party
    third_party/nlohmann
)
target_link_libraries(BrightnessIntentTest PRIVATE
    user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32
)

add_executable(Utf8SanitizerTest
    tests/Utf8SanitizerTest.cpp
    src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp
)
target_include_directories(Utf8SanitizerTest PRIVATE src)
target_link_libraries(Utf8SanitizerTest PRIVATE user32 gdi32)

add_executable(WakeWordNormalizationTest
    tests/WakeWordNormalizationTest.cpp
    src/voice/WakeWordDetector.cpp
)
target_include_directories(WakeWordNormalizationTest PRIVATE src)
target_link_libraries(WakeWordNormalizationTest PRIVATE user32 gdi32)

add_executable(VoiceRoutingTest
    tests/VoiceRoutingTest.cpp
    src/voice/WhisperRecognizer.cpp src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp src/voice/VoiceSession.cpp
    src/utils/Logger.cpp src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp src/utils/AudioResampler.cpp
    src/audio/MfccExtractor.cpp src/audio/SpeakerVerifier.cpp
    src/system/IntentDetector.cpp src/system/WindowsController.cpp
)
target_include_directories(VoiceRoutingTest PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)
target_link_libraries(VoiceRoutingTest PRIVATE
    user32 gdi32 ole32 sapi winmm gdiplus oleaut32 uuid powrprof advapi32
    whisper ggml
)

add_executable(RecognitionCallbackTest
    tests/RecognitionCallbackTest.cpp
    src/voice/WhisperRecognizer.cpp src/voice/WakeWordDetector.cpp
    src/voice/TextToSpeech.cpp src/voice/VoiceSession.cpp
    src/utils/Logger.cpp src/utils/Utf8Sanitizer.cpp
    src/utils/TextNormalizer.cpp src/utils/AudioResampler.cpp
    src/audio/MfccExtractor.cpp src/audio/SpeakerVerifier.cpp
)
target_include_directories(RecognitionCallbackTest PRIVATE
    src
    third_party/whisper.cpp/include
    third_party/whisper.cpp/ggml/include
)
target_link_libraries(RecognitionCallbackTest PRIVATE
    user32 gdi32 ole32 sapi winmm
    whisper ggml
)

target_link_libraries(ConfigTest PRIVATE user32 gdi32)

# =====================================================
# Sprint 12.7 Test Targets
# =====================================================

add_executable(AudioValidationTest
    tests/AudioValidationTest.cpp
    src/config/Config.cpp
)
target_include_directories(AudioValidationTest PRIVATE src)
target_link_libraries(AudioValidationTest PRIVATE user32 gdi32)

add_executable(NormalizeRecognitionTest
    tests/NormalizeRecognitionTest.cpp
    src/utils/TextNormalizer.cpp
    src/utils/Utf8Sanitizer.cpp
)
target_include_directories(NormalizeRecognitionTest PRIVATE src)
target_link_libraries(NormalizeRecognitionTest PRIVATE user32 gdi32)

add_executable(ResamplerTest
    tests/ResamplerTest.cpp
    src/utils/AudioResampler.cpp
)
target_include_directories(ResamplerTest PRIVATE src)
target_link_libraries(ResamplerTest PRIVATE user32 gdi32)

add_executable(CommandRecordingTest
    tests/CommandRecordingTest.cpp
    src/config/Config.cpp
)
target_include_directories(CommandRecordingTest PRIVATE src)
target_link_libraries(CommandRecordingTest PRIVATE user32 gdi32)

add_executable(ConfidenceFilterTest
    tests/ConfidenceFilterTest.cpp
    src/config/Config.cpp
)
target_include_directories(ConfidenceFilterTest PRIVATE src)
target_link_libraries(ConfidenceFilterTest PRIVATE user32 gdi32)

add_executable(InitialPromptTest
    tests/InitialPromptTest.cpp
    src/config/Config.cpp
)
target_include_directories(InitialPromptTest PRIVATE src)
target_link_libraries(InitialPromptTest PRIVATE user32 gdi32)

# =====================================================
# Sprint 13 — Mode Transition & Phrase Detection Tests
# =====================================================

add_executable(ModeTransitionTest
    tests/ModeTransitionTest.cpp
    src/chat/ChatController.cpp src/chat/Conversation.cpp
    src/chat/PromptBuilder.cpp
    src/config/ConfigManager.cpp src/config/Config.cpp
    src/system/IntentDetector.cpp src/system/WindowsController.cpp
    src/memory/MemoryManager.cpp src/memory/MemoryRules.cpp
    src/memory/MemoryExtractor.cpp src/memory/SQLiteMemoryStorage.cpp
    src/utils/Logger.cpp src/utils/StringFuzzy.cpp
    third_party/sqlite/sqlite3.c
)
target_include_directories(ModeTransitionTest PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/sqlite
)
target_link_libraries(ModeTransitionTest PRIVATE
    user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32
)

# =====================================================
# Sprint 12.x — Speaker Verification (MFCC)
# =====================================================

add_executable(MfccExtractorTest
    tests/MfccExtractorTest.cpp
    src/audio/MfccExtractor.cpp
)
target_include_directories(MfccExtractorTest PRIVATE src)
target_link_libraries(MfccExtractorTest PRIVATE user32 gdi32)

add_executable(SpeakerVerifierTest
    tests/SpeakerVerifierTest.cpp
    src/audio/SpeakerVerifier.cpp
    src/audio/MfccExtractor.cpp
    src/utils/Logger.cpp
)
target_include_directories(SpeakerVerifierTest PRIVATE src)
target_link_libraries(SpeakerVerifierTest PRIVATE user32 gdi32)

# =====================================================
# Status
# =====================================================
message(STATUS "Yuki Sprint 12.x - System Tray & Push-to-Talk UI")
