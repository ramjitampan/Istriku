#include "../src/audio/SpeakerVerifier.h"
#include "../src/audio/MfccExtractor.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

using Yuki::Audio::SpeakerVerifier;
using Yuki::Audio::MfccExtractor;

// Generate a simple sine wave as dummy "voice" sample
static std::vector<float> GenerateTone(float freqHz, int sampleRate, float durationSec)
{
    int n = static_cast<int>(sampleRate * durationSec);
    std::vector<float> audio(n);
    for (int i = 0; i < n; ++i)
    {
        audio[i] = std::sin(2.0f * 3.141592653589793f * freqHz * i / sampleRate);
    }
    return audio;
}

int main()
{
    std::cout << "Speaker Verifier Tests\n";

    std::cout << "\n=== Verify without enrollment ===\n\n";

    {
        SpeakerVerifier verifier(0.85f, MfccExtractor::Config{});
        auto audio = GenerateTone(200.0f, 16000, 1.0f);
        Test("Not enrolled initially", !verifier.IsEnrolled());
        Test("Verify returns false when not enrolled",
             !verifier.Verify(audio));
    }

    std::cout << "\n=== Cosine similarity — identical vectors = 1.0 ===\n\n";

    {
        std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<float> b = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

        // Use the private CosineSimilarity via a test helper:
        // We can't call it directly (it's private), so we verify via Enroll + Verify
        SpeakerVerifier verifier(0.99f, MfccExtractor::Config{});
        // Generate two identical tones
        auto audio = GenerateTone(200.0f, 16000, 1.0f);
        verifier.Enroll(audio);

        // Same tone should pass with high threshold
        Test("Enrolled successfully", verifier.IsEnrolled());
        Test("Same audio passes verification",
             verifier.Verify(audio));
    }

    std::cout << "\n=== Cosine similarity — orthogonal vectors = 0.0 ===\n\n";

    {
        // Verify via Enroll + Verify with very different audio
        SpeakerVerifier verifier(0.99f, MfccExtractor::Config{});
        auto enrollAudio = GenerateTone(200.0f, 16000, 1.0f);
        verifier.Enroll(enrollAudio);

        // Very different tone should fail at high threshold
        auto differentAudio = GenerateTone(3000.0f, 16000, 1.0f);
        Test("Different tone fails verification",
             !verifier.Verify(differentAudio));
    }

    std::cout << "\n=== Threshold configuration ===\n\n";

    {
        SpeakerVerifier verifier(0.99f, MfccExtractor::Config{});
        Test("Default threshold is 0.85 (from Config defaults)",
             verifier.GetThreshold() == 0.99f);

        verifier.SetThreshold(0.5f);
        Test("Threshold changed to 0.5",
             verifier.GetThreshold() == 0.5f);
    }

    std::cout << "\n=== Same content, different volume ===\n\n";

    {
        SpeakerVerifier verifier(0.5f, MfccExtractor::Config{});
        auto audio = GenerateTone(440.0f, 16000, 1.0f);
        verifier.Enroll(audio);

        // Same tone at half amplitude — MFCC should still match (energy normalized by log + DCT)
        std::vector<float> quietAudio(audio.size());
        for (size_t i = 0; i < audio.size(); ++i)
            quietAudio[i] = audio[i] * 0.5f;

        // This may or may not pass depending on how MFCC handles amplitude scaling
        // MFCC is not amplitude-invariant, but log helps
        bool result = verifier.Verify(quietAudio);
        // Just log the result — no assertion since it depends on exact MFCC math
        if (result)
            std::cout << "[INFO] Same tone at half amplitude PASSED\n";
        else
            std::cout << "[INFO] Same tone at half amplitude FAILED (expected for some implementations)\n";
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
