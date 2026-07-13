#include "../src/utils/AudioResampler.h"

#include <algorithm>
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

using Yuki::Utils::AudioResampler;

int main()
{
    std::cout << "Audio Resampler Tests\n";

    std::cout << "\n=== NeedsResampling ===\n\n";

    Test("16000 does not need resampling",
         !AudioResampler::NeedsResampling(16000));
    Test("48000 needs resampling",
         AudioResampler::NeedsResampling(48000));
    Test("44100 needs resampling",
         AudioResampler::NeedsResampling(44100));
    Test("32000 needs resampling",
         AudioResampler::NeedsResampling(32000));

    std::cout << "\n=== Resample 48kHz -> 16kHz ===\n\n";

    {
        // Generate a 1kHz sine wave at 48kHz
        std::vector<int16_t> input;
        int inputRate = 48000;
        int outputRate = 16000;
        int numSamples = 4800; // 100ms at 48kHz
        input.reserve(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            double val = std::sin(2.0 * 3.14159 * 1000.0 * i / inputRate);
            input.push_back(static_cast<int16_t>(val * 16000.0));
        }

        std::vector<int16_t> output;
        AudioResampler::Resample(input.data(), numSamples,
                                 inputRate, outputRate, output);

        int expectedOutputSize = numSamples * outputRate / inputRate;
        Test("Output size is correct",
             static_cast<int>(output.size()) == expectedOutputSize);
        Test("Output is not empty", !output.empty());
        Test("First sample preserved",
             std::abs(output[0] - input[0]) < 10);
    }

    std::cout << "\n=== Resample 44.1kHz -> 16kHz ===\n\n";

    {
        std::vector<int16_t> input;
        int inputRate = 44100;
        int outputRate = 16000;
        int numSamples = 4410;
        input.reserve(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            double val = std::sin(2.0 * 3.14159 * 440.0 * i / inputRate);
            input.push_back(static_cast<int16_t>(val * 16000.0));
        }

        std::vector<int16_t> output;
        AudioResampler::Resample(input.data(), numSamples,
                                 inputRate, outputRate, output);

        int expectedOutputSize = numSamples * outputRate / inputRate;
        Test("Output size is correct",
             static_cast<int>(output.size()) == expectedOutputSize ||
             static_cast<int>(output.size()) == expectedOutputSize + 1);
        Test("Output is not empty", !output.empty());
    }

    std::cout << "\n=== Same rate passthrough ===\n\n";

    {
        std::vector<int16_t> input = {100, 200, -100, -200, 0, 32767, -32768};
        int numSamples = static_cast<int>(input.size());
        std::vector<int16_t> output;
        AudioResampler::Resample(input.data(), numSamples, 16000, 16000, output);

        Test("Output size matches input", output.size() == input.size());
        bool match = true;
        for (size_t i = 0; i < input.size(); ++i)
        {
            if (output[i] != input[i]) { match = false; break; }
        }
        Test("Output equals input", match);
    }

    std::cout << "\n=== Empty input ===\n\n";

    {
        std::vector<int16_t> output;
        AudioResampler::Resample(nullptr, 0, 48000, 16000, output);
        Test("Empty input produces empty output", output.empty());
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
