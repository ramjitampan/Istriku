#pragma once

#include <cstdint>
#include <vector>

namespace Yuki::Utils {

class AudioResampler
{
public:
    AudioResampler() = delete;

    static void Resample(
        const int16_t* input, int inputSize,
        int inputSampleRate, int outputSampleRate,
        std::vector<int16_t>& output);

    static bool NeedsResampling(int sampleRate) noexcept;
};

} // namespace Yuki::Utils
