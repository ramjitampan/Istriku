#pragma once

#include "MfccExtractor.h"

#include <optional>
#include <vector>

namespace Yuki::Audio {

class SpeakerVerifier
{
public:
    SpeakerVerifier();
    SpeakerVerifier(
        float threshold,
        MfccExtractor::Config mfccConfig);

    void Enroll(const std::vector<float>& audioSamples);
    bool IsEnrolled() const noexcept;

    bool Verify(const std::vector<float>& audioSamples) const;

    float GetThreshold() const noexcept;
    void SetThreshold(float threshold) noexcept;

private:
    static float CosineSimilarity(
        const std::vector<float>& a,
        const std::vector<float>& b);

    MfccExtractor m_extractor;
    std::optional<std::vector<float>> m_referenceEmbedding;
    float m_threshold;
};

} // namespace Yuki::Audio
