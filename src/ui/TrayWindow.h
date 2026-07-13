#pragma once

#include "../config/Config.h"

#include <Windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <mmsystem.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace Yuki::Chat { class ChatController; }
namespace Yuki::System { class WindowsController; class IntentDetector; }

class TrayWindow
{
public:
    TrayWindow(
        const Yuki::Config::Config& config,
        Yuki::Chat::ChatController& chat,
        Yuki::System::WindowsController& windowsController,
        Yuki::System::IntentDetector& intentDetector);
    ~TrayWindow();

    TrayWindow(const TrayWindow&) = delete;
    TrayWindow& operator=(const TrayWindow&) = delete;

    bool Create();
    int RunMessageLoop();
    void Destroy();

private:
    enum CustomMessage
    {
        WM_TRAY_NOTIFY = WM_APP + 1,
        WM_UPDATE_LEVEL = WM_APP + 2,
        WM_UPDATE_CHAT = WM_APP + 3,

        WM_MIC_DOWN = WM_APP + 10,
        WM_MIC_UP = WM_APP + 11
    };

    enum class PttState { Idle, Capturing, Processing };

    // Window procedures
    static LRESULT CALLBACK HiddenWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK PttWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HiddenMessageHandler(UINT, WPARAM, LPARAM);
    LRESULT PttMessageHandler(UINT, WPARAM, LPARAM);

    // Tray icon
    bool CreateTrayIcon();
    void RemoveTrayIcon();

    // PTT window
    bool CreatePttWindow();
    void ShowPttWindow();
    void HidePttWindow();
    void TogglePttWindow();
    void PositionNearTray();

    // Controls
    void CreateChatLog();
    void CreateMicButton();
    void CreateLevelMeter();
    void AppendToChat(const std::string& text, bool isUser);
    void UpdateLevelBar(float percent);
    void LoadChatHistory();
    void LayoutControls();

    // Drawing
    void DrawMicButton(HDC hdc, bool hover);
    void DrawLevelMeter(HDC hdc);

    // Audio capture
    bool StartAudioCapture();
    void StopAudioCapture();
    void CaptureThread();
    void ProcessCapturedAudio(std::vector<float> audio);
    std::string ProcessCommandText(const std::string& text);

    // Whisper inference
    void RunInference(const std::vector<float>& audio);

    // Helpers
    static float ComputeRMS(const int16_t* samples, int nSamples);
    static HICON CreatePlaceholderIcon();
    static std::string GetRandomFallbackReply();

    // WaveIn callback
    static void CALLBACK WaveCallbackStatic(
        HWAVEIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

    // Window class registration
    static const wchar_t* kHiddenClass;
    static const wchar_t* kPttClass;

    // Dimensions
    static constexpr int kPttWidth = 360;
    static constexpr int kPttHeight = 480;
    static constexpr int kMicButtonSize = 64;
    static constexpr int kLevelMeterHeight = 8;
    static constexpr int kPadding = 12;

    // Theme colors
    static constexpr COLORREF kBgColor = RGB(30, 30, 46);
    static constexpr COLORREF kTextColor = RGB(205, 214, 244);
    static constexpr COLORREF kBorderColor = RGB(60, 60, 90);
    static constexpr COLORREF kMicIdle = RGB(70, 110, 180);
    static constexpr COLORREF kMicHover = RGB(90, 130, 210);
    static constexpr COLORREF kMicRecording = RGB(220, 50, 50);
    static constexpr COLORREF kMicRecordingHover = RGB(180, 40, 40);
    static constexpr COLORREF kLevelBg = RGB(40, 40, 60);
    static constexpr COLORREF kLevelLow = RGB(0, 200, 100);
    static constexpr COLORREF kLevelMid = RGB(200, 200, 0);
    static constexpr COLORREF kLevelHigh = RGB(220, 50, 50);
    static constexpr COLORREF kChatBg = RGB(25, 25, 40);

    // Dependencies
    const Yuki::Config::Config& m_config;
    Yuki::Chat::ChatController& m_chat;
    Yuki::System::WindowsController& m_windowsController;
    Yuki::System::IntentDetector& m_intentDetector;

    // Windows
    HWND m_hwndHidden = nullptr;
    HWND m_hwndPtt = nullptr;

    // Controls
    HWND m_hChatLog = nullptr;
    HWND m_hMicButton = nullptr;
    HWND m_hLevelMeter = nullptr;

    // Tray
    NOTIFYICONDATAW m_nid = {};
    bool m_trayIconAdded = false;
    HICON m_hIcon = nullptr;

    // Mic button state
    bool m_micHover = false;
    RECT m_micRect = {};
    RECT m_levelRect = {};

    // Audio capture
    HWAVEIN m_micHandle = nullptr;
    int m_audioSampleRate = 16000;
    std::vector<std::vector<int16_t>> m_captureBuffers;
    std::vector<WAVEHDR> m_captureHeaders;
    std::queue<WAVEHDR*> m_audioQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::thread m_captureThread;
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_stopping{false};
    std::vector<float> m_pttBuffer;
    std::mutex m_pttMutex;
    std::atomic<PttState> m_pttState{PttState::Idle};
    std::vector<int16_t> m_resampleBuffer;
    float m_lastRms = 0.0f;
    std::thread m_inferenceThread;

    // Whisper context
    struct whisper_context* m_whisper = nullptr;

    // Brushes for dark theme
    HBRUSH m_hBgBrush = nullptr;
    HBRUSH m_hChatBgBrush = nullptr;
    HBRUSH m_kLevelBgBrush = nullptr;
    HBRUSH m_hMicIdleBrush = nullptr;
    HBRUSH m_hMicHoverBrush = nullptr;
    HBRUSH m_hMicRecordingBrush = nullptr;
    HFONT m_hChatFont = nullptr;

    // Brush cleanup helper
    static void SafeDeleteBrush(HBRUSH& brush)
    {
        if (brush) { DeleteObject(brush); brush = nullptr; }
    }

    // Window class registration tracking
    bool m_hiddenClassRegistered = false;
    bool m_pttClassRegistered = false;
};
