#include "MfccExtractor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Yuki::Audio {

MfccExtractor::MfccExtractor()
    : MfccExtractor(Config{})
{
}

MfccExtractor::MfccExtractor(const Config& config)
    : m_config(config)
{
    m_frameSize = (config.sampleRate * config.frameSizeMs) / 1000;
    m_hopSize = (config.sampleRate * config.hopSizeMs) / 1000;

    // FFT size: next power of 2 >= frameSize
    m_fftSize = 1;
    while (m_fftSize < m_frameSize) m_fftSize <<= 1;
    m_numFftBins = m_fftSize / 2 + 1;

    // Precompute all tables once
    PrecomputeHammingWindow();
    PrecomputeMelFilterbank();
    PrecomputeDCTMatrix();

    // Reserve all intermediate buffers — never reallocate
    m_preEmphasized.reserve(16000 * 3);
    m_frameBuffer.resize(m_frameSize);
    m_fftReal.resize(m_fftSize);
    m_fftImag.resize(m_fftSize);
    m_powerSpectrum.resize(m_numFftBins);
    m_melEnergies.resize(config.numMelFilters);
    m_logEnergies.resize(config.numMelFilters);
    m_mfccBuffer.resize(config.numCoeffs);
    m_mfccSum.resize(config.numCoeffs);
}

void MfccExtractor::PrecomputeHammingWindow()
{
    m_hammingWindow.resize(m_frameSize);
    for (int i = 0; i < m_frameSize; ++i)
    {
        m_hammingWindow[i] = 0.54f - 0.46f *
            std::cos(2.0f * 3.141592653589793f * i / (m_frameSize - 1));
    }
}

void MfccExtractor::PrecomputeMelFilterbank()
{
    float melMin = 2595.0f * std::log10(1.0f + m_config.minFreq / 700.0f);
    float melMax = 2595.0f * std::log10(1.0f + m_config.maxFreq / 700.0f);

    int numFilters = m_config.numMelFilters;
    int numBins = m_numFftBins;

    m_melFilterbank.resize(numFilters);
    for (auto& row : m_melFilterbank)
        row.assign(numBins, 0.0f);

    // Equally spaced mel points
    std::vector<float> melPoints(numFilters + 2);
    for (int i = 0; i < numFilters + 2; ++i)
    {
        float mel = melMin + (melMax - melMin) * i / (numFilters + 1);
        // Convert mel back to Hz
        float freq = 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
        // Map to FFT bin
        melPoints[i] = freq * m_fftSize / m_config.sampleRate;
    }

    for (int m = 0; m < numFilters; ++m)
    {
        int startBin = static_cast<int>(std::ceil(melPoints[m]));
        int centerBin = static_cast<int>(melPoints[m + 1]);
        int endBin = static_cast<int>(std::floor(melPoints[m + 2]));

        if (centerBin < 0) centerBin = 0;
        if (endBin >= numBins) endBin = numBins - 1;

        for (int k = startBin; k <= centerBin && k < numBins; ++k)
        {
            float val = (k - melPoints[m]) / (melPoints[m + 1] - melPoints[m]);
            if (val > 0.0f)
                m_melFilterbank[m][k] = val;
        }
        for (int k = centerBin + 1; k <= endBin && k < numBins; ++k)
        {
            float val = (melPoints[m + 2] - k) / (melPoints[m + 2] - melPoints[m + 1]);
            if (val > 0.0f)
                m_melFilterbank[m][k] = val;
        }
    }
}

void MfccExtractor::PrecomputeDCTMatrix()
{
    int M = m_config.numMelFilters;
    int N = m_config.numCoeffs;

    m_dctMatrix.resize(N);
    for (auto& row : m_dctMatrix)
        row.resize(M);

    for (int k = 0; k < N; ++k)
    {
        for (int n = 0; n < M; ++n)
        {
            m_dctMatrix[k][n] = std::cos(
                3.141592653589793f * k * (n + 0.5f) / M);
        }
    }
}

void MfccExtractor::PreEmphasis(const std::vector<float>& input) const
{
    int n = static_cast<int>(input.size());
    m_preEmphasized.clear();
    if (n == 0) return;
    m_preEmphasized.push_back(input[0]);
    for (int i = 1; i < n; ++i)
    {
        m_preEmphasized.push_back(
            input[i] - m_config.preEmphasisAlpha * input[i - 1]);
    }
}

void MfccExtractor::FrameSignal(int totalSamples) const
{
    m_frameCount = 0;
    std::fill(m_mfccSum.begin(), m_mfccSum.end(), 0.0f);

    int maxStart = totalSamples - m_frameSize;
    if (maxStart < 0) return;

    for (int start = 0; start <= maxStart; start += m_hopSize)
    {
        // Copy frame from pre-emphasized signal
        for (int i = 0; i < m_frameSize; ++i)
        {
            m_frameBuffer[i] = m_preEmphasized[start + i];
        }

        ApplyHamming();
        RunFFT();
        ComputePowerSpectrum();
        ApplyMelFilterbank();
        ApplyLog();
        ApplyDCT();

        for (int i = 0; i < m_config.numCoeffs; ++i)
        {
            m_mfccSum[i] += m_mfccBuffer[i];
        }
        ++m_frameCount;
    }
}

void MfccExtractor::ApplyHamming() const
{
    for (int i = 0; i < m_frameSize; ++i)
    {
        m_frameBuffer[i] *= m_hammingWindow[i];
    }
}

void MfccExtractor::ComputeFFT(std::vector<float>& real, std::vector<float>& imag)
{
    int n = static_cast<int>(real.size());

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    // Cooley-Tukey radix-2 FFT
    for (int len = 2; len <= n; len <<= 1)
    {
        float ang = 2.0f * 3.141592653589793f / len;
        float wReal = std::cos(ang);
        float wImag = -std::sin(ang);

        for (int i = 0; i < n; i += len)
        {
            float curReal = 1.0f, curImag = 0.0f;
            int half = len / 2;
            for (int j = 0; j < half; ++j)
            {
                float tReal = curReal * real[i + j + half] -
                              curImag * imag[i + j + half];
                float tImag = curReal * imag[i + j + half] +
                              curImag * real[i + j + half];

                real[i + j + half] = real[i + j] - tReal;
                imag[i + j + half] = imag[i + j] - tImag;
                real[i + j] += tReal;
                imag[i + j] += tImag;

                float newReal = curReal * wReal - curImag * wImag;
                curImag = curReal * wImag + curImag * wReal;
                curReal = newReal;
            }
        }
    }
}

void MfccExtractor::RunFFT() const
{
    // Zero-pad frame to FFT size
    std::fill(m_fftReal.begin(), m_fftReal.end(), 0.0f);
    std::fill(m_fftImag.begin(), m_fftImag.end(), 0.0f);

    for (int i = 0; i < m_frameSize; ++i)
    {
        m_fftReal[i] = m_frameBuffer[i];
    }

    ComputeFFT(m_fftReal, m_fftImag);
}

void MfccExtractor::ComputePowerSpectrum() const
{
    for (int i = 0; i < m_numFftBins; ++i)
    {
        m_powerSpectrum[i] = m_fftReal[i] * m_fftReal[i] +
                             m_fftImag[i] * m_fftImag[i];
    }
}

void MfccExtractor::ApplyMelFilterbank() const
{
    std::fill(m_melEnergies.begin(), m_melEnergies.end(), 0.0f);

    for (int m = 0; m < m_config.numMelFilters; ++m)
    {
        float sum = 0.0f;
        for (int k = 0; k < m_numFftBins; ++k)
        {
            sum += m_powerSpectrum[k] * m_melFilterbank[m][k];
        }
        m_melEnergies[m] = sum;
    }
}

void MfccExtractor::ApplyLog() const
{
    for (int i = 0; i < m_config.numMelFilters; ++i)
    {
        float val = m_melEnergies[i];
        m_logEnergies[i] = (val > 1e-10f) ? std::log10(val) : 0.0f;
    }
}

void MfccExtractor::ApplyDCT() const
{
    for (int k = 0; k < m_config.numCoeffs; ++k)
    {
        float sum = 0.0f;
        for (int n = 0; n < m_config.numMelFilters; ++n)
        {
            sum += m_logEnergies[n] * m_dctMatrix[k][n];
        }
        m_mfccBuffer[k] = sum;
    }
}

void MfccExtractor::AverageFrames() const
{
    if (m_frameCount > 0)
    {
        for (int i = 0; i < m_config.numCoeffs; ++i)
        {
            m_mfccBuffer[i] = m_mfccSum[i] / static_cast<float>(m_frameCount);
        }
    }
    else
    {
        std::fill(m_mfccBuffer.begin(), m_mfccBuffer.end(), 0.0f);
    }
}

std::vector<float> MfccExtractor::Extract(const std::vector<float>& audioSamples) const
{
    if (audioSamples.empty())
        return std::vector<float>(m_config.numCoeffs, 0.0f);

    PreEmphasis(audioSamples);
    FrameSignal(static_cast<int>(m_preEmphasized.size()));
    AverageFrames();

    return m_mfccBuffer;
}

} // namespace Yuki::Audio
