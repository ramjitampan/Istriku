#include "../src/config/Config.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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
    std::cout << "Command Recording Config Tests\n";

    std::cout << "\n=== Default duration ===\n\n";

    VoiceConfig cfg;
    int sampleRate = cfg.sample_rate;
    int minDurationMs = cfg.minimum_command_duration_ms;
    int minSamples = (minDurationMs * sampleRate) / 1000;

    Test("minimum_command_duration_ms is 2000", minDurationMs == 2000);
    Test("At 16kHz, 2000ms = 32000 samples",
         minSamples == 32000);

    std::cout << "\n=== Silence timeout ===\n\n";

    int silenceMs = cfg.silence_timeout_ms;
    Test("silence_timeout_ms is 2000", silenceMs == 2000);

    std::cout << "\n=== Custom durations ===\n\n";

    VoiceConfig custom;
    custom.sample_rate = 16000;
    custom.minimum_command_duration_ms = 1000;
    custom.silence_timeout_ms = 1500;

    int customMinSamples = (custom.minimum_command_duration_ms * custom.sample_rate) / 1000;
    Test("1000ms at 16kHz = 16000 samples", customMinSamples == 16000);
    Test("silence timeout 1500ms configurable", custom.silence_timeout_ms == 1500);

    std::cout << "\n=== VAD thresholds ===\n\n";

    Test("speech_start_threshold > speech_stop_threshold (hysteresis)",
         cfg.speech_start_threshold > cfg.speech_stop_threshold);
    Test("speech_start_threshold is 100.0",
         cfg.speech_start_threshold == 100.0f);
    Test("speech_stop_threshold is 50.0",
         cfg.speech_stop_threshold == 50.0f);

    // Test custom VAD config
    VoiceConfig vadCustom;
    vadCustom.speech_start_threshold = 80.0f;
    vadCustom.speech_stop_threshold = 30.0f;
    Test("Custom start threshold", vadCustom.speech_start_threshold == 80.0f);
    Test("Custom stop threshold", vadCustom.speech_stop_threshold == 30.0f);
    Test("Custom hysteresis preserved",
         vadCustom.speech_start_threshold > vadCustom.speech_stop_threshold);

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
