#include "SpeakerVerifier.h"
#include "../utils/Logger.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace Yuki::Audio {

SpeakerVerifier::SpeakerVerifier()
    : SpeakerVerifier(0.85f, MfccExtractor::Config{})
{
}

SpeakerVerifier::SpeakerVerifier(
    float threshold,
    MfccExtractor::Config mfccConfig)
    : m_extractor(std::move(mfccConfig))
    , m_threshold(threshold)
{
}

void SpeakerVerifier::Enroll(const std::vector<float>& audioSamples)
{
    m_referenceEmbedding = m_extractor.Extract(audioSamples);
    Logger::Info("[SpeakerVerifier] Enrolled with " +
                 std::to_string(audioSamples.size()) + " samples");
}

bool SpeakerVerifier::IsEnrolled() const noexcept
{
    return m_referenceEmbedding.has_value();
}

bool SpeakerVerifier::Verify(const std::vector<float>& audioSamples) const
{
    if (!m_referenceEmbedding.has_value())
    {
        Logger::Warning("[SpeakerVerifier] Not enrolled — rejecting");
        return false;
    }

    std::vector<float> candidate = m_extractor.Extract(audioSamples);
    float similarity = CosineSimilarity(m_referenceEmbedding.value(), candidate);

    Logger::Info("[SpeakerVerifier] Similarity: " +
                 std::to_string(similarity) +
                 " (threshold: " + std::to_string(m_threshold) + ")");

    return similarity >= m_threshold;
}

float SpeakerVerifier::GetThreshold() const noexcept
{
    return m_threshold;
}

void SpeakerVerifier::SetThreshold(float threshold) noexcept
{
    m_threshold = threshold;
}

float SpeakerVerifier::CosineSimilarity(
    const std::vector<float>& a,
    const std::vector<float>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;

    float dot = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;

    for (size_t i = 0; i < a.size(); ++i)
    {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    float denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom < 1e-10f) return 0.0f;

    return std::clamp(dot / denom, -1.0f, 1.0f);
}

} // namespace Yuki::Audio
