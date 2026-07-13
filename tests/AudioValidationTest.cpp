#include "../src/config/Config.h"

#include <iostream>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Config::VoiceConfig;

int main()
{
    std::cout << "Audio Validation & Config Tests\n";

    std::cout << "\n=== VoiceConfig defaults ===\n\n";

    VoiceConfig cfg;
    Test("sample_rate defaults to 16000", cfg.sample_rate == 16000);
    Test("silence_timeout_ms defaults to 2000", cfg.silence_timeout_ms == 2000);
    Test("speech_start_threshold defaults to 100.0",
         cfg.speech_start_threshold == 100.0f);
    Test("speech_stop_threshold defaults to 50.0",
         cfg.speech_stop_threshold == 50.0f);
    Test("minimum_command_duration_ms defaults to 2000",
         cfg.minimum_command_duration_ms == 2000);
    Test("initial_prompt is not empty", !cfg.initial_prompt.empty());
    Test("confidence_threshold defaults to 0.5",
         cfg.confidence_threshold == 0.5f);

    std::cout << "\n=== Config overrides ===\n\n";

    VoiceConfig custom;
    custom.sample_rate = 48000;
    custom.silence_timeout_ms = 3000;
    custom.speech_start_threshold = 200.0f;
    custom.speech_stop_threshold = 80.0f;
    custom.minimum_command_duration_ms = 3000;
    custom.confidence_threshold = 0.7f;

    Test("sample_rate override", custom.sample_rate == 48000);
    Test("silence_timeout_ms override", custom.silence_timeout_ms == 3000);
    Test("speech_start_threshold override",
         custom.speech_start_threshold == 200.0f);
    Test("speech_stop_threshold override",
         custom.speech_stop_threshold == 80.0f);
    Test("minimum_command_duration_ms override",
         custom.minimum_command_duration_ms == 3000);
    Test("confidence_threshold override",
         custom.confidence_threshold == 0.7f);

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
