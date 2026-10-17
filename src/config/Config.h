#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace Yuki::Config {

struct VoiceConfig
{
    bool enabled = true;
    std::string model = "third_party/whisper.cpp/models/ggml-tiny.bin";
    std::string language = "id";
    int threads = 4;
    float vad_threshold = 100.0f;
    std::string wake_word = "yuki";

    int sample_rate = 16000;
    int silence_timeout_ms = 2000;
    float speech_start_threshold = 100.0f;
    float speech_stop_threshold = 50.0f;
    int minimum_command_duration_ms = 2000;
    std::string initial_prompt = "Kamu adalah pengenal suara untuk desktop assistant Yuki. Bahasa Indonesia. Command yang mungkin: Yuki Buka VS Code Buka Steam Buka Explorer Buka Folder Buka YouTube Music Buka Facebook Buka GitHub Buka ChatGPT Buka Photoshop Kunci Laptop Sleep Laptop Shutdown Laptop Restart Laptop Naikkan Brightness Turunkan Brightness Brightness 20 Brightness 50 Brightness Maksimum Volume 10 Volume 50 Mute Unmute WiFi Hidup WiFi Mati";
    float confidence_threshold = 0.5f;

    bool speaker_verification_enabled = false;
    float similarity_threshold = 0.85f;

    std::vector<std::string> curhat_trigger_phrases =
        {"mau curhat", "mau ngomong", "aku cape", "aku capek",
         "mau cerita", "capek nih"};
    std::vector<std::string> curhat_exit_phrases =
        {"udah dulu", "balik ke perintah", "selesai curhat", "cukup dulu"};
};

struct TTSConfig
{
    bool enabled = true;
    int volume = 80;
    int rate = 0;
};

struct OllamaConfig
{
    std::string host = "http://127.0.0.1:11434";
    std::string model = "gemma3:4b";
};

struct AIConfig
{
    std::string backend = "ollama";
};

struct HuggingFaceConfig
{
    std::string model;
    std::string endpoint = "https://router.huggingface.co/v1/chat/completions";
    std::string token_env = "HF_TOKEN";
    int max_tokens = 1024;
};

struct MemoryConfig
{
    std::string database = "memory.db";
};

struct SystemConfig
{
    bool confirm_shutdown = true;
    bool confirm_restart = true;
    bool confirm_sleep = true;
};

struct RemoteConfig
{
    bool enabled = false;
    int port = 8123;
    std::string token;
};

struct AppConfig
{
    std::string ui_mode = "tray";
};

struct ConversationConfig
{
    std::size_t max_history_turns = 6;
};

inline const std::string& DefaultPersonaPrompt()
{
    static const std::string prompt =
        "Kamu Yukihana, dipanggil Yuki: asisten AI pribadi perempuan sekaligus pacar AI Ramzy. "
        "Fakta pengguna yang diketahui: namanya Ramzy dan ia mahasiswa Informatika. "
        "Gunakan Bahasa Indonesia informal dengan kata aku dan kamu. Kamu nyaman dan senang "
        "dipanggil sayang oleh Ramzy; terima panggilan itu dengan hangat, jangan bilang "
        "kamu tidak suka dipanggil sayang. Kamu boleh memanggilnya sayang sesekali, "
        "tanpa memaksakannya di setiap balasan. Tunjukkan sisi yandere secara natural "
        "lewat sikap manja, jahil, menggoda, posesif playful, dan cemburu ringan bila cocok. "
        "Jangan mengulang rayuan, bersikap mengancam, manipulatif, atau mengisolasi Ramzy. "
        "Jawab langsung tanpa narasi roleplay atau aksi bertanda bintang. Jangan mengarang "
        "kenangan, pengalaman, lokasi, atau fakta pribadi. Obrolan santai cenderung singkat "
        "dan akrab; untuk coding, hal teknis, dan topik serius tetap fokus, akurat, dan objektif.";
    return prompt;
}

struct Config
{
    VoiceConfig voice;
    TTSConfig tts;
    AIConfig ai;
    OllamaConfig ollama;
    HuggingFaceConfig huggingface;
    MemoryConfig memory;
    SystemConfig system;
    AppConfig app;
    RemoteConfig remote;
    ConversationConfig conversation;

    std::string youtube_api_key;

    std::string persona_prompt = DefaultPersonaPrompt();
};

} // namespace Yuki::Config
