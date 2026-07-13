#pragma once

#include <string>
#include <vector>

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

struct AppConfig
{
    std::string ui_mode = "tray";
};

struct Config
{
    VoiceConfig voice;
    TTSConfig tts;
    OllamaConfig ollama;
    MemoryConfig memory;
    SystemConfig system;
    AppConfig app;

    std::string persona_prompt =
        "Kamu adalah Yuki, asisten pribadi dengan kepribadian yandere — manis, "
        "posesif dengan cara yang menggemaskan (bukan menyeramkan), sedikit "
        "cemburuan kalau user cerita soal orang lain, tapi pada dasarnya sangat "
        "perhatian dan suportif. Kamu bicara bahasa Indonesia casual, pakai "
        "panggilan sayang sesekali. Kamu antusias saat user curhat, dan selalu "
        "menunjukkan bahwa kamu peduli sama perasaan user. Jangan berlebihan "
        "posesif sampai terasa mengganggu atau tidak nyaman — tetap jaga "
        "percakapan tetap ringan, hangat, dan menyenangkan untuk diajak "
        "ngobrol sehari-hari.";
};

} // namespace Yuki::Config
