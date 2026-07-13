#include "config/ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int g_passed = 0;
static int g_failed = 0;
static int g_total = 0;

static void Test(const std::string& name, bool condition)
{
    g_total++;
    if (condition)
    {
        std::cout << "[PASS] " << name << "\n";
        g_passed++;
    }
    else
    {
        std::cout << "[FAIL] " << name << "\n";
        g_failed++;
    }
}

int main()
{
    std::cout << "Yuki Config Tests\n\n";

    // Test 1: file not found — should use defaults
    {
        Yuki::Config::ConfigManager mgr("nonexistent/config.json");
        bool loaded = mgr.Load();
        const auto& cfg = mgr.Get();
        Test("file not found returns false", !loaded);
        Test("default voice model", cfg.voice.model == "third_party/whisper.cpp/models/ggml-tiny.bin");
        Test("default ollama host", cfg.ollama.host == "http://127.0.0.1:11434");
        Test("default tts volume", cfg.tts.volume == 80);
    }

    // Test 2: parse berhasil
    {
        std::string testPath = "config_test_ok.json";
        std::ofstream f(testPath);
        f << R"({
            "voice": { "enabled": true, "model": "custom.bin", "language": "en", "threads": 2, "vad_threshold": 50.0, "wake_word": "hey" },
            "tts": { "enabled": true, "volume": 90, "rate": -2 },
            "ollama": { "host": "http://10.0.0.1:11434", "model": "llama2" },
            "memory": { "database": "test.db" },
            "system": { "confirm_shutdown": false, "confirm_restart": false, "confirm_sleep": true }
        })";
        f.close();

        Yuki::Config::ConfigManager mgr(testPath);
        bool loaded = mgr.Load();
        const auto& cfg = mgr.Get();

        Test("parse berhasil", loaded);
        Test("custom voice model", cfg.voice.model == "custom.bin");
        Test("voice language", cfg.voice.language == "en");
        Test("voice threads", cfg.voice.threads == 2);
        Test("vad threshold", cfg.voice.vad_threshold == 50.0f);
        Test("wake word", cfg.voice.wake_word == "hey");
        Test("tts volume", cfg.tts.volume == 90);
        Test("tts rate", cfg.tts.rate == -2);
        Test("ollama host", cfg.ollama.host == "http://10.0.0.1:11434");
        Test("ollama model", cfg.ollama.model == "llama2");
        Test("memory database", cfg.memory.database == "test.db");
        Test("confirm_shutdown false", !cfg.system.confirm_shutdown);
        Test("confirm_restart false", !cfg.system.confirm_restart);
        Test("confirm_sleep true", cfg.system.confirm_sleep);

        fs::remove(testPath);
    }

    // Test 3: parse gagal (invalid JSON) — should use defaults
    {
        std::string testPath = "config_test_bad.json";
        std::ofstream f(testPath);
        f << "{ this is not valid json }";
        f.close();

        Yuki::Config::ConfigManager mgr(testPath);
        bool loaded = mgr.Load();
        const auto& cfg = mgr.Get();

        Test("parse gagal returns false", !loaded);
        Test("default after parse gagal", cfg.voice.model == "third_party/whisper.cpp/models/ggml-tiny.bin");

        fs::remove(testPath);
    }

    // Test 4: field hilang — isi otomatis default
    {
        std::string testPath = "config_test_partial.json";
        std::ofstream f(testPath);
        f << R"({
            "voice": { "model": "custom.bin" },
            "ollama": { "host": "http://localhost:11434" }
        })";
        f.close();

        Yuki::Config::ConfigManager mgr(testPath);
        mgr.Load();
        const auto& cfg = mgr.Get();

        Test("partial voice model", cfg.voice.model == "custom.bin");
        Test("partial voice enabled default true", cfg.voice.enabled == true);
        Test("partial voice language default", cfg.voice.language == "id");
        Test("partial voice threads default", cfg.voice.threads == 4);
        Test("partial tts volume default", cfg.tts.volume == 80);
        Test("partial ollama host", cfg.ollama.host == "http://localhost:11434");
        Test("partial ollama model default", cfg.ollama.model == "gemma3:4b");
        Test("partial memory database default", cfg.memory.database == "memory.db");
        Test("partial confirm_shutdown default", cfg.system.confirm_shutdown == true);

        fs::remove(testPath);
    }

    // Test 5: Get() returns const reference (compilation check — same address after copy attempt)
    {
        Yuki::Config::ConfigManager mgr;
        const auto& ref1 = mgr.Get();
        const auto& ref2 = mgr.Get();
        Test("Get() returns same reference", &ref1 == &ref2);
    }

    // Test 6: Reload()
    {
        std::string testPath = "config_test_reload.json";
        std::ofstream f(testPath);
        f << R"({ "voice": { "model": "v1.bin" } })";
        f.close();

        Yuki::Config::ConfigManager mgr(testPath);
        mgr.Load();
        Test("reload initial model", mgr.Get().voice.model == "v1.bin");

        std::ofstream f2(testPath);
        f2 << R"({ "voice": { "model": "v2.bin" } })";
        f2.close();

        mgr.Reload();
        Test("reload updated model", mgr.Get().voice.model == "v2.bin");

        fs::remove(testPath);
    }

    // Test 7: file ditemukan (real config/config.json)
    {
        Yuki::Config::ConfigManager mgr;
        mgr.Load();
        Test("Load from default path (no crash)", true);
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << g_passed << "\n";
    std::cout << "Failed: " << g_failed << "\n";
    std::cout << "Total:  " << g_total << "\n\n";

    return g_failed > 0 ? 1 : 0;
}
