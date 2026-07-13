#include "WhisperRecognizer.h"
#include "utils/Logger.h"
#include "utils/Utf8Sanitizer.h"
#include "utils/TextNormalizer.h"
#include "utils/AudioResampler.h"
#include "audio/SpeakerVerifier.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmsystem.h>

#include "whisper.h"

namespace Yuki::Voice
{

struct WhisperRecognizer::Impl
{
    static constexpr int kTargetSampleRate = 16000;
    static constexpr int kChannels         = 1;
    static constexpr int kBitsPerSample    = 16;

    static constexpr int kBufferDurationMs = 250;
    static constexpr int kBufferFrames     = 4000;
    static constexpr int kNumBuffers       = 4;

    struct WhisperDeleter
    {
        void operator()(struct whisper_context* ctx) const noexcept
        {
            if (ctx)
            {
                whisper_free(ctx);
                Logger::Info("Model Freed");
            }
        }
    };

    using WhisperPtr = std::unique_ptr<struct whisper_context, WhisperDeleter>;

    static void WhisperLogCallback(ggml_log_level level, const char* text, void*)
    {
        (void)level;
        (void)text;
    }

    explicit Impl(const Yuki::Config::VoiceConfig& config)
        : m_modelPath(config.model)
        , m_config(config)
    {
        whisper_log_set(WhisperLogCallback, nullptr);

        auto cparams = whisper_context_default_params();
        cparams.use_gpu = false;

        m_whisper = WhisperPtr(
            whisper_init_from_file_with_params(m_modelPath.c_str(), cparams));

        if (!m_whisper)
        {
            throw std::runtime_error(
                "WhisperRecognizer: gagal memuat model dari: " +
                std::string(m_modelPath) +
                "\nPastikan file tersebut ada.");
        }

        Logger::Info("Model Loaded: " + std::string(m_modelPath));

        if (config.speaker_verification_enabled)
        {
            m_speakerVerifier = std::make_unique<Yuki::Audio::SpeakerVerifier>(
                config.similarity_threshold,
                Yuki::Audio::MfccExtractor::Config{});
            Logger::Info("Speaker verification enabled (threshold=" +
                         std::to_string(config.similarity_threshold) + ")");
        }

        // Pre-compute thresholds from config
        int maxSpeechMs = 30000;
        m_minSpeechSamples = (config.minimum_command_duration_ms * kTargetSampleRate) / 1000;
        m_maxSpeechSamples = (maxSpeechMs * kTargetSampleRate) / 1000;

        // Silence timeout in buffers
        int silenceMs = config.silence_timeout_ms;
        m_silenceBufferTimeout = std::max(1, silenceMs / kBufferDurationMs);

        m_speechStartThreshold = config.speech_start_threshold;
        m_speechStopThreshold = config.speech_stop_threshold;

        // Pre-allocate buffers
        m_speechBuffer.reserve(m_maxSpeechSamples);
        m_resampleBuffer.reserve(kBufferFrames * 3);
    }

    ~Impl()
    {
        Stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    bool Start()
    {
        if (m_running.load()) return true;

        m_stopping = false;
        m_speechBuffer.clear();
        m_silenceBufferCount = 0;
        m_isSpeaking = false;
        m_commandIdleCount = 0;
        m_decodedInCommandMode = false;
        m_mode.store(RecognizerMode::WakeWord);
        m_audioSampleRate = kTargetSampleRate;

        if (!OpenMicrophone())
        {
            Logger::Error("WhisperRecognizer: gagal membuka microphone");
            return false;
        }

        m_workerThread = std::thread([this] { ProcessingLoop(); });
        m_running.store(true);

        Logger::Info("Voice Started");
        return true;
    }

    void Stop()
    {
        m_stopping = true;
        m_queueCv.notify_one();

        if (m_micHandle)
        {
            waveInReset(m_micHandle);
            Sleep(50);

            for (auto& hdr : m_waveHeaders)
            {
                if (hdr.dwFlags & WHDR_PREPARED)
                {
                    waveInUnprepareHeader(m_micHandle, &hdr, sizeof(WAVEHDR));
                }
            }
            waveInClose(m_micHandle);
            m_micHandle = nullptr;
        }

        if (m_workerThread.joinable())
        {
            m_workerThread.join();
        }

        m_running.store(false);
        Logger::Info("Voice Stopped");
    }

    bool IsRunning() const noexcept
    {
        return m_running.load();
    }

    bool OpenMicrophone()
    {
        // Try target sample rate first (16000), fallback to higher rates with resampling
        int ratesToTry[] = { m_config.sample_rate, 48000, 44100, 32000 };
        int numRates = sizeof(ratesToTry) / sizeof(ratesToTry[0]);
        MMRESULT result = MMSYSERR_NOERROR;
        bool micOpened = false;

        for (int ri = 0; ri < numRates; ++ri)
        {
            int tryRate = ratesToTry[ri];
            if (ri > 0 && tryRate == ratesToTry[0]) continue;

            WAVEFORMATEX wfx = {};
            wfx.wFormatTag      = WAVE_FORMAT_PCM;
            wfx.nChannels       = kChannels;
            wfx.nSamplesPerSec  = tryRate;
            wfx.wBitsPerSample  = kBitsPerSample;
            wfx.nBlockAlign     = (wfx.nChannels * wfx.wBitsPerSample) / 8;
            wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
            wfx.cbSize          = 0;

            result = waveInOpen(
                &m_micHandle, WAVE_MAPPER, &wfx,
                reinterpret_cast<DWORD_PTR>(&WaveCallbackStatic),
                reinterpret_cast<DWORD_PTR>(this),
                CALLBACK_FUNCTION);

            if (result == MMSYSERR_NOERROR)
            {
                m_audioSampleRate = tryRate;
                micOpened = true;

                Logger::Info("Audio Sample Rate: " + std::to_string(tryRate) + " Hz");
                Logger::Info("Audio Channels: " + std::to_string(kChannels));
                Logger::Info("Audio Bits: " + std::to_string(kBitsPerSample));

                if (tryRate != kTargetSampleRate)
                {
                    Logger::Info("Audio Resampling: " + std::to_string(tryRate) +
                                 " -> " + std::to_string(kTargetSampleRate) + " Hz");
                }
                break;
            }
        }

        if (!micOpened)
        {
            Logger::Error(
                "Microphone gagal: waveInOpen error " +
                std::to_string(result));
            return false;
        }

        // Adjust buffer frames for actual sample rate
        DWORD bufferFrames = (m_audioSampleRate * kBufferDurationMs) / 1000;
        const DWORD bufferBytes = bufferFrames * sizeof(int16_t);

        m_buffers.resize(kNumBuffers);
        m_waveHeaders.resize(kNumBuffers);

        for (int i = 0; i < kNumBuffers; ++i)
        {
            m_buffers[i].resize(bufferFrames);
            m_waveHeaders[i].lpData =
                reinterpret_cast<LPSTR>(m_buffers[i].data());
            m_waveHeaders[i].dwBufferLength = bufferBytes;
            m_waveHeaders[i].dwFlags = 0;
            m_waveHeaders[i].dwUser = static_cast<DWORD_PTR>(i);

            MMRESULT prep = waveInPrepareHeader(
                m_micHandle, &m_waveHeaders[i], sizeof(WAVEHDR));

            if (prep != MMSYSERR_NOERROR)
            {
                Logger::Error(
                    "Microphone gagal: waveInPrepareHeader error " +
                    std::to_string(prep));
                CloseMicrophone();
                return false;
            }

            waveInAddBuffer(m_micHandle, &m_waveHeaders[i], sizeof(WAVEHDR));
        }

        result = waveInStart(m_micHandle);
        if (result != MMSYSERR_NOERROR)
        {
            Logger::Error(
                "Microphone gagal: waveInStart error " +
                std::to_string(result));
            CloseMicrophone();
            return false;
        }

        return true;
    }

    void CloseMicrophone()
    {
        if (!m_micHandle) return;

        waveInReset(m_micHandle);

        for (auto& hdr : m_waveHeaders)
        {
            if (hdr.dwFlags & WHDR_PREPARED)
            {
                waveInUnprepareHeader(m_micHandle, &hdr, sizeof(WAVEHDR));
            }
        }

        waveInClose(m_micHandle);
        m_micHandle = nullptr;
        m_waveHeaders.clear();
        m_buffers.clear();
    }

    static void CALLBACK WaveCallbackStatic(
        HWAVEIN, UINT uMsg, DWORD_PTR dwInstance,
        DWORD_PTR dwParam1, DWORD_PTR)
    {
        if (uMsg != WIM_DATA) return;

        auto* self = reinterpret_cast<Impl*>(dwInstance);
        auto* header = reinterpret_cast<WAVEHDR*>(dwParam1);

        if (self->m_stopping.load()) return;

        {
            std::lock_guard<std::mutex> lock(self->m_queueMutex);
            self->m_audioQueue.push(header);
        }
        self->m_queueCv.notify_one();
    }

    void ProcessingLoop()
    {
        while (!m_stopping.load())
        {
            WAVEHDR* header;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCv.wait(lock, [this]
                {
                    return !m_audioQueue.empty() || m_stopping.load();
                });

                if (m_stopping.load()) break;

                header = m_audioQueue.front();
                m_audioQueue.pop();
            }

            auto* samples = reinterpret_cast<int16_t*>(header->lpData);
            int nSamples = static_cast<int>(
                header->dwBytesRecorded / sizeof(int16_t));

            // Resample if needed
            if (m_audioSampleRate != kTargetSampleRate)
            {
                m_resampleBuffer.clear();
                Yuki::Utils::AudioResampler::Resample(
                    samples, nSamples,
                    m_audioSampleRate, kTargetSampleRate,
                    m_resampleBuffer);
                ProcessAudio(m_resampleBuffer.data(),
                    static_cast<int>(m_resampleBuffer.size()));
            }
            else
            {
                ProcessAudio(samples, nSamples);
            }

            if (!m_stopping.load())
            {
                waveInAddBuffer(m_micHandle, header, sizeof(WAVEHDR));
            }
        }

        FlushSpeech();
    }

    void ProcessAudio(const int16_t* samples, int nSamples)
    {
        if (m_paused.load()) return;

        if (m_decodedInCommandMode &&
            m_mode.load() == RecognizerMode::CommandRecording)
        {
            return;
        }

        float rms = ComputeRMS(samples, nSamples);

        // VAD with hysteresis
        if (m_isSpeaking)
        {
            // In speaking state — use stop threshold
            if (rms < m_speechStopThreshold)
            {
                m_silenceBufferCount++;
            }
            else
            {
                m_silenceBufferCount = 0;
                m_commandIdleCount = 0;
            }
        }
        else
        {
            // In silence state — use start threshold
            if (rms > m_speechStartThreshold)
            {
                m_isSpeaking = true;
                m_silenceBufferCount = 0;
                m_commandIdleCount = 0;
                m_speechBuffer.clear();
            }
        }

        // Append samples to speech buffer while speaking (including silence tail)
        if (m_isSpeaking)
        {
            for (int i = 0; i < nSamples; ++i)
            {
                m_speechBuffer.push_back(
                    static_cast<float>(samples[i]) / 32768.0f);
            }

            // Check if speech has ended (silence timeout)
            if (m_silenceBufferCount >= m_silenceBufferTimeout)
            {
                m_isSpeaking = false;
                m_silenceBufferCount = 0;

                if (static_cast<int>(m_speechBuffer.size()) >= m_minSpeechSamples)
                {
                    if (m_mode.load() == RecognizerMode::CommandRecording)
                    {
                        m_decodedInCommandMode = true;
                        Logger::Info("[Voice] Running Whisper (command)...");
                    }
                    else
                    {
                        Logger::Info("[Voice] Running Whisper (wake word)...");
                    }
                    RunInference();
                }
                m_speechBuffer.clear();
            }
        }

        // Abort CommandRecording if no speech for idle timeout
        if (m_mode.load() == RecognizerMode::CommandRecording &&
            !m_isSpeaking)
        {
            m_commandIdleCount++;
            int idleTimeout = (20 * kBufferDurationMs) / kBufferDurationMs;
            if (m_commandIdleCount >= idleTimeout)
            {
                m_commandIdleCount = 0;
                m_mode.store(RecognizerMode::WakeWord);
                Logger::Info("[Voice] Command recording expired (no speech)");
            }
        }
    }

    void FlushSpeech()
    {
        if (m_isSpeaking &&
            static_cast<int>(m_speechBuffer.size()) >= m_minSpeechSamples)
        {
            RunInference();
        }
        m_isSpeaking = false;
        m_speechBuffer.clear();
        m_silenceBufferCount = 0;
    }

    void RunInference()
    {
        // Speaker verification (before whisper — saves compute)
        if (m_speakerVerifier)
        {
            if (m_speakerVerifier->IsEnrolled() &&
                !m_speakerVerifier->Verify(m_speechBuffer))
            {
                Logger::Info("[Voice] Speaker mismatch — skipping whisper");
                if (m_errorHandler)
                    m_errorHandler(
                        "Maaf, suara Anda tidak dikenali. "
                        "Silakan enroll terlebih dahulu.");
                m_speechBuffer.clear();
                return;
            }
        }

        auto startTime = std::chrono::steady_clock::now();

        auto wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.print_progress   = false;
        wparams.print_special    = false;
        wparams.print_realtime   = false;
        wparams.print_timestamps = false;
        wparams.single_segment   = true;
        wparams.suppress_blank   = true;
        wparams.suppress_nst     = true;
        wparams.language         = m_config.language.c_str();
        wparams.n_threads        = m_config.threads;
        wparams.no_timestamps    = true;
        wparams.no_context       = true;
        wparams.temperature      = 0.0f;
        wparams.greedy.best_of   = 2;
        wparams.initial_prompt   = m_config.initial_prompt.c_str();

        int ret = whisper_full(m_whisper.get(), wparams,
            m_speechBuffer.data(),
            static_cast<int>(m_speechBuffer.size()));

        if (ret != 0)
        {
            Logger::Error(
                "Whisper inference gagal dengan kode: " +
                std::to_string(ret));

            if (m_errorHandler)
                m_errorHandler("Whisper inference gagal");

            return;
        }

        // Extract text and confidence
        int nSegments = whisper_full_n_segments(m_whisper.get());
        std::string rawText;
        float confidence = 1.0f;

        for (int i = 0; i < nSegments; ++i)
        {
            const char* seg = whisper_full_get_segment_text(
                m_whisper.get(), i);

            if (seg)
            {
                if (!rawText.empty()) rawText += ' ';
                rawText += seg;
            }

            float noSpeechProb = whisper_full_get_segment_no_speech_prob(
                m_whisper.get(), i);
            float segConfidence = 1.0f - noSpeechProb;
            if (segConfidence < confidence)
                confidence = segConfidence;
        }

        auto inferEnd = std::chrono::steady_clock::now();
        auto totalMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                inferEnd - startTime).count();

        // Sanitize UTF-8
        std::string sanitized = Yuki::Utils::Utf8Sanitizer::Sanitize(rawText);

        // Normalize
        std::string normalized = Yuki::Utils::TextNormalizer::NormalizeRecognition(sanitized);

        // Confidence filter
        if (confidence < m_config.confidence_threshold)
        {
            Logger::Info("Raw: \"" + rawText + "\"");
            Logger::Info("Normalized: \"" + normalized + "\"");
            Logger::Info("Confidence: " + std::to_string(confidence) +
                         " (below threshold " +
                         std::to_string(m_config.confidence_threshold) + ")");
            Logger::Info("Execution Time: " + std::to_string(totalMs) + "ms");

            if (m_errorHandler)
            {
                m_errorHandler(
                    "Maaf, aku kurang yakin mendengarnya. Bisa diulangi?");
            }
            return;
        }

        // Skip noise — too short
        if (normalized.size() < 3 && confidence < 0.8f) return;

        Logger::Info("Raw: \"" + rawText + "\"");
        Logger::Info("Normalized: \"" + normalized + "\"");
        Logger::Info("Confidence: " + std::to_string(confidence));
        Logger::Info("Execution Time: " + std::to_string(totalMs) + "ms");

        if (m_recognitionHandler)
        {
            m_recognitionHandler(std::move(normalized));
        }
    }

    static float ComputeRMS(const int16_t* samples, int nSamples)
    {
        if (nSamples <= 0) return 0.0f;

        double sum = 0.0;
        for (int i = 0; i < nSamples; ++i)
        {
            double s = static_cast<double>(samples[i]);
            sum += s * s;
        }

        return static_cast<float>(std::sqrt(sum / nSamples));
    }

    std::string m_modelPath;
    Yuki::Config::VoiceConfig m_config;
    WhisperPtr  m_whisper;
    std::unique_ptr<Yuki::Audio::SpeakerVerifier> m_speakerVerifier;

    HWAVEIN m_micHandle = nullptr;
    int m_audioSampleRate = kTargetSampleRate;
    std::vector<std::vector<int16_t>> m_buffers;
    std::vector<WAVEHDR> m_waveHeaders;

    std::queue<WAVEHDR*> m_audioQueue;
    std::mutex           m_queueMutex;
    std::condition_variable m_queueCv;

    std::vector<float> m_speechBuffer;
    std::vector<int16_t> m_resampleBuffer;
    int  m_silenceBufferCount = 0;
    bool m_isSpeaking   = false;

    int m_commandIdleCount = 0;
    bool m_decodedInCommandMode = false;
    std::atomic<RecognizerMode> m_mode{RecognizerMode::WakeWord};

    int m_minSpeechSamples = 8000;
    int m_maxSpeechSamples = 480000;
    int m_silenceBufferTimeout = 8;
    float m_speechStartThreshold = 100.0f;
    float m_speechStopThreshold = 50.0f;

    void Pause()
    {
        if (m_paused.exchange(true)) return;
        if (m_micHandle)
        {
            waveInStop(m_micHandle);
        }
        m_isSpeaking = false;
        m_silenceBufferCount = 0;
        m_speechBuffer.clear();
    }

    void Resume()
    {
        if (!m_paused.exchange(false)) return;
        m_decodedInCommandMode = false;
        m_commandIdleCount = 0;
        if (m_micHandle)
        {
            waveInStart(m_micHandle);
        }
    }

    void SetMode(RecognizerMode mode)
    {
        m_mode.store(mode);
        m_decodedInCommandMode = false;
        m_commandIdleCount = 0;
    }

    std::thread  m_workerThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_paused{false};

    RecognitionHandler m_recognitionHandler;
    ErrorHandler       m_errorHandler;
};

WhisperRecognizer::WhisperRecognizer(std::string_view modelPath)
    : m_impl(std::make_unique<Impl>(
          [modelPath]() -> Yuki::Config::VoiceConfig
          {
              Yuki::Config::VoiceConfig cfg;
              cfg.model = std::string(modelPath);
              return cfg;
          }()))
{
}

WhisperRecognizer::WhisperRecognizer(const Yuki::Config::VoiceConfig& config)
    : m_impl(std::make_unique<Impl>(config))
{
}

WhisperRecognizer::~WhisperRecognizer() = default;

WhisperRecognizer::WhisperRecognizer(WhisperRecognizer&&) noexcept = default;
WhisperRecognizer& WhisperRecognizer::operator=(
    WhisperRecognizer&&) noexcept = default;

bool WhisperRecognizer::Start() { return m_impl->Start(); }
void WhisperRecognizer::Stop()  { m_impl->Stop(); }
void WhisperRecognizer::Pause() { m_impl->Pause(); }
void WhisperRecognizer::Resume() { m_impl->Resume(); }
bool WhisperRecognizer::IsRunning() const noexcept
{
    return m_impl->IsRunning();
}

void WhisperRecognizer::SetRecognitionHandler(RecognitionHandler handler)
{
    m_impl->m_recognitionHandler = std::move(handler);
}

void WhisperRecognizer::SetErrorHandler(ErrorHandler handler)
{
    m_impl->m_errorHandler = std::move(handler);
}

void WhisperRecognizer::SetMode(RecognizerMode mode)
{
    m_impl->SetMode(mode);
}

RecognizerMode WhisperRecognizer::GetMode() const
{
    return m_impl->m_mode.load();
}

void WhisperRecognizer::EnrollSpeaker(const std::vector<float>& audioSamples)
{
    if (!m_impl->m_speakerVerifier)
    {
        Logger::Info("[Voice] Creating SpeakerVerifier on-demand for enrollment");
        m_impl->m_speakerVerifier =
            std::make_unique<Yuki::Audio::SpeakerVerifier>(
                m_impl->m_config.similarity_threshold,
                Yuki::Audio::MfccExtractor::Config{});
    }
    m_impl->m_speakerVerifier->Enroll(audioSamples);
}

} // namespace Yuki::Voice
