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
    src/utils/StringFuzzy.cpp

    src/ai/OllamaClient.cpp

    src/chat/ChatController.cpp
    src/chat/CommandRouter.cpp
    src/chat/Conversation.cpp
    src/chat/PromptBuilder.cpp

    src/memory/SQLiteMemoryStorage.cpp
    src/memory/MemoryExtractor.cpp
    src/memory/MemoryManager.cpp
    src/memory/MemoryRules.cpp

    src/system/WindowsController.cpp
    src/system/IntentDetector.cpp

    src/remote/RemoteServer.cpp

    src/voice/TextToSpeech.cpp

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

add_executable(RemoteServerTest
    tests/RemoteServerTest.cpp
    src/remote/RemoteServer.cpp
    src/utils/HttpClient.cpp src/utils/Json.cpp src/utils/Logger.cpp
)
target_include_directories(RemoteServerTest PRIVATE
    src
    third_party
    third_party/nlohmann
)
target_link_libraries(RemoteServerTest PRIVATE
    user32 gdi32 winhttp ws2_32
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

# =====================================================
# Include Directories
# =====================================================
target_include_directories(Yuki PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/sqlite
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
    user32 gdi32 winhttp gdiplus ole32 oleaut32 uuid powrprof winmm sapi advapi32 shell32 ws2_32
)
target_link_libraries(OllamaTest PRIVATE user32 gdi32 winhttp)
target_link_libraries(MemoryTest PRIVATE user32 gdi32)
target_link_libraries(RetrieverTest PRIVATE user32 gdi32)
target_link_libraries(PromptBuilderTest PRIVATE user32 gdi32)
target_link_libraries(SystemTest PRIVATE user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32 winhttp)
target_link_libraries(WakeWordTest PRIVATE user32 gdi32)
target_link_libraries(TextToSpeechTest PRIVATE user32 gdi32 ole32 sapi)

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
    user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32 winhttp
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

add_executable(NoAICmdExecutionTest
    tests/NoAICmdExecutionTest.cpp
    src/chat/ChatController.cpp src/chat/Conversation.cpp
    src/chat/PromptBuilder.cpp
    src/config/ConfigManager.cpp src/config/Config.cpp
    src/system/IntentDetector.cpp src/system/WindowsController.cpp
    src/memory/MemoryManager.cpp src/memory/MemoryRules.cpp
    src/memory/MemoryExtractor.cpp src/memory/SQLiteMemoryStorage.cpp
    src/utils/Logger.cpp src/utils/StringFuzzy.cpp
    third_party/sqlite/sqlite3.c
)
target_include_directories(NoAICmdExecutionTest PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/sqlite
)
target_link_libraries(NoAICmdExecutionTest PRIVATE
    user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32 winhttp
)

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
    user32 gdi32 gdiplus ole32 oleaut32 uuid powrprof winmm advapi32 winhttp
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
message(STATUS "Yuki - System Tray & Typing Chat UI")
