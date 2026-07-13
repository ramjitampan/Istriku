#include "AudioResampler.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Yuki::Utils {

void AudioResampler::Resample(
    const int16_t* input, int inputSize,
    int inputSampleRate, int outputSampleRate,
    std::vector<int16_t>& output)
{
    if (inputSampleRate == outputSampleRate || inputSize == 0)
    {
        output.assign(input, input + inputSize);
        return;
    }

    double ratio = static_cast<double>(outputSampleRate) / inputSampleRate;
    int outputSize = static_cast<int>(inputSize * ratio);

    output.clear();
    output.reserve(outputSize);

    for (int i = 0; i < outputSize; ++i)
    {
        double srcPos = static_cast<double>(i) / ratio;
        int srcIndex = static_cast<int>(srcPos);
        double frac = srcPos - srcIndex;

        if (srcIndex + 1 >= inputSize)
        {
            output.push_back(input[srcIndex]);
        }
        else
        {
            double sample = (1.0 - frac) * input[srcIndex] +
                            frac * input[srcIndex + 1];
            sample = std::clamp(sample,
                static_cast<double>(INT16_MIN),
                static_cast<double>(INT16_MAX));
            output.push_back(static_cast<int16_t>(sample));
        }
    }
}

bool AudioResampler::NeedsResampling(int sampleRate) noexcept
{
    return sampleRate != 16000;
}

} // namespace Yuki::Utils
