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

using Yuki::Audio::MfccExtractor;

// ---------------------------------------------------------------------------
// FFT validation: 1000 Hz sine wave @ 16kHz, 512-point FFT
//   Bin resolution = 16000 / 512 = 31.25 Hz/bin
//   1000 Hz / 31.25 = bin 32
// ---------------------------------------------------------------------------
static void TestFFTWithSineWave()
{
    std::cout << "\n=== FFT validation — 1000 Hz sine wave ===\n\n";

    const int sampleRate = 16000;
    const int fftSize = 512;
    const float freq = 1000.0f;
    const float expectedBin = freq * fftSize / sampleRate; // = 32

    // Generate 512 samples of a 1000 Hz sine wave (amplitude 16000)
    std::vector<float> real(fftSize, 0.0f);
    std::vector<float> imag(fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        real[i] = std::sin(2.0f * 3.141592653589793f * freq * i / sampleRate) * 16000.0f;
    }

    // Run FFT
    MfccExtractor::ComputeFFT(real, imag);

    // Compute magnitude spectrum
    std::vector<float> magnitude(fftSize / 2 + 1);
    for (int i = 0; i <= fftSize / 2; ++i)
    {
        magnitude[i] = std::sqrt(real[i] * real[i] + imag[i] * imag[i]);
    }

    // Find peak bin (ignore DC at bin 0)
    int peakBin = 0;
    float peakMag = 0.0f;
    for (int i = 1; i <= fftSize / 2; ++i)
    {
        if (magnitude[i] > peakMag)
        {
            peakMag = magnitude[i];
            peakBin = i;
        }
    }

    Test("Peak bin is 32 (1000 Hz @ 16kHz, 512-point FFT)",
         peakBin == static_cast<int>(expectedBin));
    Test("Peak magnitude is positive", peakMag > 0.0f);
    Test("DC bin (0) is not the peak", peakBin > 0);

    // Check that peak is significantly larger than neighboring bins
    if (peakBin > 0 && peakBin < static_cast<int>(magnitude.size()) - 1)
    {
        float neighborsAvg = (magnitude[peakBin - 1] + magnitude[peakBin + 1]) / 2.0f;
        Test("Peak is at least 10x stronger than neighbors (no window)",
             peakMag > neighborsAvg * 10.0f);
    }
}

// ---------------------------------------------------------------------------
// MFCC basic property: output is always 13 coefficients for any valid input
// ---------------------------------------------------------------------------
static void TestMFCCConstantLength()
{
    std::cout << "\n=== MFCC output length ===\n\n";

    MfccExtractor extractor;

    // Short audio (~0.5s)
    std::vector<float> shortAudio(8000, 0.0f);
    auto mfcc1 = extractor.Extract(shortAudio);
    Test("Short audio (0.5s) produces 13 coefficients",
         mfcc1.size() == 13);

    // Longer audio (~2s)
    std::vector<float> longAudio(32000, 0.0f);
    auto mfcc2 = extractor.Extract(longAudio);
    Test("Long audio (2s) produces 13 coefficients",
         mfcc2.size() == 13);

    // Empty audio
    std::vector<float> emptyAudio;
    auto mfcc3 = extractor.Extract(emptyAudio);
    Test("Empty audio produces 13 coefficients (all zero)",
         mfcc3.size() == 13);
    bool allZero = true;
    for (auto v : mfcc3) if (v != 0.0f) { allZero = false; break; }
    Test("Empty audio coefficients are all zero", allZero);
}

// ---------------------------------------------------------------------------
// MFCC with different audio content produces different embeddings
// ---------------------------------------------------------------------------
static void TestMFCCDifferentContent()
{
    std::cout << "\n=== MFCC different content ===\n\n";

    MfccExtractor extractor;

    // Audio with different frequencies
    std::vector<float> audio1(16000, 0.0f);
    std::vector<float> audio2(16000, 0.0f);
    for (int i = 0; i < 16000; ++i)
    {
        audio1[i] = std::sin(2.0f * 3.141592653589793f * 200.0f * i / 16000.0f);
        audio2[i] = std::sin(2.0f * 3.141592653589793f * 2000.0f * i / 16000.0f);
    }

    auto mfcc1 = extractor.Extract(audio1);
    auto mfcc2 = extractor.Extract(audio2);

    float dot = 0.0f, n1 = 0.0f, n2 = 0.0f;
    for (size_t i = 0; i < 13; ++i)
    {
        dot += mfcc1[i] * mfcc2[i];
        n1 += mfcc1[i] * mfcc1[i];
        n2 += mfcc2[i] * mfcc2[i];
    }
    float sim = dot / (std::sqrt(n1) * std::sqrt(n2) + 1e-10f);
    Test("Different frequencies produce different embeddings (similarity < 0.95)",
         sim < 0.95f);
}

// ---------------------------------------------------------------------------
// Same audio repeated produces same embedding (deterministic)
// ---------------------------------------------------------------------------
static void TestMFCCDeterministic()
{
    std::cout << "\n=== MFCC deterministic ===\n\n";

    MfccExtractor extractor;
    std::vector<float> audio(16000, 0.0f);
    for (int i = 0; i < 16000; ++i)
    {
        audio[i] = std::sin(2.0f * 3.141592653589793f * 440.0f * i / 16000.0f);
    }

    auto mfcc1 = extractor.Extract(audio);
    auto mfcc2 = extractor.Extract(audio);

    bool identical = true;
    for (size_t i = 0; i < 13; ++i)
    {
        if (std::abs(mfcc1[i] - mfcc2[i]) > 1e-5f)
        {
            identical = false;
            break;
        }
    }
    Test("Same input produces identical MFCC twice", identical);
}

int main()
{
    std::cout << "MFCC Extractor Tests\n";

    TestFFTWithSineWave();
    TestMFCCConstantLength();
    TestMFCCDifferentContent();
    TestMFCCDeterministic();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
