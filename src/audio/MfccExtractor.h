#pragma once

#include <vector>

namespace Yuki::Audio {

class MfccExtractor
{
public:
    struct Config
    {
        int sampleRate = 16000;
        int numMelFilters = 26;
        int numCoeffs = 13;
        float minFreq = 300.0f;
        float maxFreq = 8000.0f;
        float preEmphasisAlpha = 0.97f;
        int frameSizeMs = 25;
        int hopSizeMs = 10;
    };

    MfccExtractor();
    explicit MfccExtractor(const Config& config);
    ~MfccExtractor() = default;

    MfccExtractor(const MfccExtractor&) = delete;
    MfccExtractor& operator=(const MfccExtractor&) = delete;

    MfccExtractor(MfccExtractor&&) noexcept = default;
    MfccExtractor& operator=(MfccExtractor&&) noexcept = default;

    std::vector<float> Extract(const std::vector<float>& audioSamples) const;

    static void ComputeFFT(std::vector<float>& real, std::vector<float>& imag);

private:
    void PrecomputeHammingWindow();
    void PrecomputeMelFilterbank();
    void PrecomputeDCTMatrix();
    void PreEmphasis(const std::vector<float>& input) const;
    void FrameSignal(int totalSamples) const;
    void ApplyHamming() const;
    void RunFFT() const;
    void ComputePowerSpectrum() const;
    void ApplyMelFilterbank() const;
    void ApplyLog() const;
    void ApplyDCT() const;
    void AverageFrames() const;

    Config m_config;

    int m_frameSize = 0;
    int m_hopSize = 0;
    int m_fftSize = 0;
    int m_numFftBins = 0;

    // Precomputed tables
    std::vector<float> m_hammingWindow;
    std::vector<std::vector<float>> m_melFilterbank;
    std::vector<std::vector<float>> m_dctMatrix;

    // Reusable intermediate buffers (reserved once, reused — mutable for const Extract)
    mutable std::vector<float> m_preEmphasized;
    mutable std::vector<float> m_frameBuffer;
    mutable std::vector<float> m_fftReal;
    mutable std::vector<float> m_fftImag;
    mutable std::vector<float> m_powerSpectrum;
    mutable std::vector<float> m_melEnergies;
    mutable std::vector<float> m_logEnergies;
    mutable std::vector<float> m_mfccBuffer;

    // Per-frame accumulation for averaging
    mutable std::vector<float> m_mfccSum;
    mutable int m_frameCount = 0;
};

} // namespace Yuki::Audio
