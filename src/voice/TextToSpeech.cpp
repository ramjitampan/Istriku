#include "TextToSpeech.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <windows.h>
#include <sapi.h>

// Kompatibilitas MinGW-w64 vs MSVC untuk flag SAPI
#ifndef SPF_PURGEBEFORE
#define SPF_PURGEBEFORE 2
#endif
#ifndef SPF_ASYNC
#define SPF_ASYNC 1
#endif

namespace Yuki::Voice
{

static std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};

    int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()),
        nullptr, 0);

    if (len <= 0) return {};

    std::wstring result(len, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()),
        &result[0], len);

    return result;
}

// =====================================================
// Implementation (PIMPL)
// =====================================================
struct TextToSpeech::Impl
{
    Impl()
    {
        m_thread = std::thread([this] { ThreadProc(); });
    }

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_cv.notify_one();

        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void Speak(const std::string& text)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_textQueue.push(text);
        }
        m_cv.notify_one();
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingStop = true;
        }
        m_cv.notify_one();
    }

    int GetVolume() const noexcept
    {
        return m_volume.load();
    }

    int GetRate() const noexcept
    {
        return m_rate.load();
    }

    void Mute(bool mute)
    {
        m_muted.store(mute);
    }

    bool IsMuted() const noexcept
    {
        return m_muted.load();
    }

    bool IsSpeaking() const noexcept
    {
        return m_isSpeaking.load();
    }

    // =====================================================
    // Worker thread (COM + SAPI)
    // =====================================================
    void ThreadProc()
    {
        HRESULT hr = CoInitializeEx(
            nullptr, COINIT_MULTITHREADED);

        ISpVoice* spVoice = nullptr;
        if (SUCCEEDED(hr))
        {
            CoCreateInstance(
                CLSID_SpVoice, nullptr,
                CLSCTX_ALL, IID_ISpVoice,
                reinterpret_cast<void**>(&spVoice));
        }

        if (spVoice)
        {
            spVoice->SetVolume(
                static_cast<USHORT>(m_volume.load()));
            spVoice->SetRate(m_rate.load());
        }

        std::unique_lock<std::mutex> lock(m_mutex);

        while (!m_stopping)
        {
            m_cv.wait(lock, [this]
            {
                return !m_textQueue.empty() || m_stopping;
            });

            while (!m_textQueue.empty())
            {
                std::string text = std::move(m_textQueue.front());
                m_textQueue.pop();

                bool stopRequested = m_pendingStop;
                m_pendingStop = false;

                if (stopRequested && spVoice)
                {
                    spVoice->Speak(nullptr, SPF_PURGEBEFORE, nullptr);
                    continue;
                }

                if (text.empty() || !spVoice) continue;

                std::wstring wtext = ToWide(text);

                lock.unlock();

                if (m_muted.load() || m_volume.load() == 0)
                {
                    // Skip speaking, tapi tetap tunggu (instant)
                    lock.lock();
                    continue;
                }

                m_isSpeaking.store(true);

                spVoice->Speak(wtext.c_str(), SPF_ASYNC, nullptr);

                // Tunggu hingga selesai speaking
                while (true)
                {
                    SPVOICESTATUS status;
                    spVoice->GetStatus(&status, nullptr);
                    if (status.dwRunningState != SPRS_IS_SPEAKING)
                        break;
                    Sleep(50); // Hindari busy wait
                }

                m_isSpeaking.store(false);

                // Fire completion callback
                if (m_completionHandler)
                {
                    m_completionHandler();
                }

                lock.lock();
            }
        }

        if (spVoice)
        {
            spVoice->Release();
        }

        if (SUCCEEDED(hr))
        {
            CoUninitialize();
        }
    }

    void SetCompletionHandler(CompletionHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_completionHandler = std::move(handler);
    }

    // =====================================================
    // Data
    // =====================================================
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::string> m_textQueue;
    bool m_stopping = false;
    bool m_pendingStop = false;

    CompletionHandler m_completionHandler;

    // Thread-safe
    std::atomic<int> m_volume{100};
    std::atomic<int> m_rate{0};
    std::atomic<bool> m_muted{false};
    std::atomic<bool> m_isSpeaking{false};
};

// =====================================================
// TextToSpeech public API
// =====================================================

TextToSpeech::TextToSpeech()
    : m_impl(std::make_unique<Impl>())
{
}

TextToSpeech::TextToSpeech(const Yuki::Config::TTSConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    SetVolume(config.volume);
    SetRate(config.rate);
}

TextToSpeech::~TextToSpeech() = default;

TextToSpeech::TextToSpeech(TextToSpeech&&) noexcept = default;
TextToSpeech& TextToSpeech::operator=(TextToSpeech&&) noexcept = default;

void TextToSpeech::Speak(const std::string& text)
{
    m_impl->Speak(text);
}

void TextToSpeech::Stop()
{
    m_impl->Stop();
}

void TextToSpeech::SetVolume(int volume)
{
    m_impl->m_volume.store(std::clamp(volume, 0, 100));
}

int TextToSpeech::GetVolume() const noexcept
{
    return m_impl->GetVolume();
}

void TextToSpeech::SetRate(int rate)
{
    m_impl->m_rate.store(std::clamp(rate, -10, 10));
}

int TextToSpeech::GetRate() const noexcept
{
    return m_impl->GetRate();
}

void TextToSpeech::Mute(bool mute)
{
    m_impl->Mute(mute);
}

bool TextToSpeech::IsMuted() const noexcept
{
    return m_impl->IsMuted();
}

bool TextToSpeech::IsSpeaking() const noexcept
{
    return m_impl->IsSpeaking();
}

void TextToSpeech::SetCompletionHandler(CompletionHandler handler)
{
    m_impl->SetCompletionHandler(std::move(handler));
}

} // namespace Yuki::Voice
