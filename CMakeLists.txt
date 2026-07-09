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
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
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
# Source Files (Sprint 1)
# =====================================================
set(SOURCES

    src/main.cpp

    src/core/Application.cpp
    src/ui/MainWindow.cpp

    src/utils/Logger.cpp
    src/utils/HttpClient.cpp
    src/utils/Json.cpp

    src/ai/OllamaClient.cpp

    src/chat/ChatController.cpp
    src/chat/Conversation.cpp
    src/chat/PromptBuilder.cpp

    src/memory/SQLiteMemoryStorage.cpp

    third_party/sqlite/sqlite3.c
)

# =====================================================
# Executable
# =====================================================
add_executable(Yuki
    ${SOURCES}
)

add_executable(OllamaTest
    tests/OllamaClientTest.cpp
    src/ai/OllamaClient.cpp
    src/utils/HttpClient.cpp
    src/utils/Json.cpp
    src/utils/Logger.cpp
)

add_executable(MemoryTest
    tests/MemoryTest.cpp
    src/memory/SQLiteMemoryStorage.cpp
    src/utils/Logger.cpp
    third_party/sqlite/sqlite3.c
)
# =====================================================
# Include
# =====================================================

target_include_directories(Yuki PRIVATE
    src
    third_party
    third_party/nlohmann
    third_party/sqlite
)

target_include_directories(OllamaTest PRIVATE
    src
    third_party
)

target_include_directories(MemoryTest PRIVATE
    src
    third_party
    third_party/sqlite
)

# =====================================================
# Windows Libraries
# =====================================================

target_link_libraries(Yuki PRIVATE
    user32
    gdi32
    winhttp
)

target_link_libraries(OllamaTest PRIVATE
    user32
    gdi32
    winhttp
)

target_link_libraries(MemoryTest PRIVATE
    user32
    gdi32
)

message(STATUS "Yuki Sprint 3 Build")