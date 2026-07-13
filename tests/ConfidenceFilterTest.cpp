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
    std::cout << "Confidence Filter Tests\n";

    std::cout << "\n=== Default threshold ===\n\n";

    VoiceConfig cfg;
    Test("confidence_threshold defaults to 0.5",
         cfg.confidence_threshold == 0.5f);

    std::cout << "\n=== Threshold validation ===\n\n";

    // Confidence should be 0.0 - 1.0 range
    Test("Default threshold >= 0.0", cfg.confidence_threshold >= 0.0f);
    Test("Default threshold <= 1.0", cfg.confidence_threshold <= 1.0f);

    // Simulate confidence checks (as done in WhisperRecognizer)
    float highConf = 0.85f;
    float lowConf = 0.30f;
    float mediumConf = 0.55f;

    Test("High confidence (0.85) above threshold",
         highConf >= cfg.confidence_threshold);
    Test("Low confidence (0.30) below threshold",
         lowConf < cfg.confidence_threshold);
    Test("Medium confidence (0.55) above threshold",
         mediumConf >= cfg.confidence_threshold);

    std::cout << "\n=== Custom threshold ===\n\n";

    VoiceConfig strict;
    strict.confidence_threshold = 0.8f;

    Test("Custom threshold 0.8", strict.confidence_threshold == 0.8f);
    Test("High 0.85 >= 0.8",
         0.85f >= strict.confidence_threshold);
    Test("Medium 0.55 < 0.8",
         0.55f < strict.confidence_threshold);

    VoiceConfig lenient;
    lenient.confidence_threshold = 0.2f;

    Test("Lenient threshold 0.2", lenient.confidence_threshold == 0.2f);
    Test("Low 0.30 >= 0.2",
         0.30f >= lenient.confidence_threshold);

    std::cout << "\n=== No-speech probability conversion ===\n\n";

    // whisper_full_get_segment_no_speech_prob returns 0.0-1.0
    // high value = likely NOT speech
    // confidence = 1.0 - no_speech_prob
    float noSpeechLow = 0.05f;  // 5% no-speech → 95% confidence
    float noSpeechHigh = 0.70f; // 70% no-speech → 30% confidence

    float confFromLow = 1.0f - noSpeechLow;
    float confFromHigh = 1.0f - noSpeechHigh;

    Test("Low no-speech prob (0.05) -> high confidence",
         confFromLow >= cfg.confidence_threshold);
    Test("High no-speech prob (0.70) -> low confidence",
         confFromHigh < cfg.confidence_threshold);
    Test("1.0 - 0.05 = 0.95",
         std::abs(confFromLow - 0.95f) < 0.001f);
    Test("1.0 - 0.70 = 0.30",
         std::abs(confFromHigh - 0.30f) < 0.001f);

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
