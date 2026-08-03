#pragma once

#include "../config/Config.h"
#include "../voice/TextToSpeech.h"

#include <Windows.h>
#include <shellapi.h>

#include <string>
#include <thread>

namespace Yuki::Chat { class CommandRouter; }

class TrayWindow
{
public:
    explicit TrayWindow(
        const Yuki::Config::Config& config,
        Yuki::Chat::CommandRouter& router);
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
        WM_UPDATE_CHAT = WM_APP + 3
    };

    // Window procedures
    static LRESULT CALLBACK HiddenWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK PttWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK InputWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HiddenMessageHandler(UINT, WPARAM, LPARAM);
    LRESULT PttMessageHandler(UINT, WPARAM, LPARAM);
    LRESULT InputMessageHandler(UINT, WPARAM, LPARAM);

    // Tray icon
    bool CreateTrayIcon();
    void RemoveTrayIcon();

    // Chat window
    bool CreatePttWindow();
    void ShowPttWindow();
    void HidePttWindow();
    void TogglePttWindow();
    void PositionNearTray();

    // Controls
    void CreateChatLog();
    void CreateInputBox();
    void AppendToChat(const std::string& text, bool isUser);
    void LoadChatHistory();
    void LayoutControls();
    void SendInputText();
    void ProcessInput(const std::string& text);

    // Drawing
    void DrawSendButton(HDC hdc);

    // Helpers
    static HICON CreatePlaceholderIcon();

    // Window class registration
    static const wchar_t* kHiddenClass;
    static const wchar_t* kPttClass;

    // Dimensions
    static constexpr int kPttWidth = 380;
    static constexpr int kPttHeight = 480;
    static constexpr int kPadding = 12;
    static constexpr int kInputHeight = 34;
    static constexpr int kSendWidth = 72;

    // Theme colors
    static constexpr COLORREF kBgColor = RGB(30, 30, 46);
    static constexpr COLORREF kTextColor = RGB(205, 214, 244);
    static constexpr COLORREF kBorderColor = RGB(60, 60, 90);
    static constexpr COLORREF kSendIdle = RGB(70, 110, 180);
    static constexpr COLORREF kSendHover = RGB(90, 130, 210);
    static constexpr COLORREF kChatBg = RGB(25, 25, 40);

    // Dependencies
    const Yuki::Config::Config& m_config;
    Yuki::Chat::CommandRouter& m_router;

    // Text-to-speech (voice output only)
    Yuki::Voice::TextToSpeech m_tts;

    // Windows
    HWND m_hwndHidden = nullptr;
    HWND m_hwndPtt = nullptr;

    // Controls
    HWND m_hChatLog = nullptr;
    HWND m_hInput = nullptr;

    // Send button state (drawn directly on the window)
    bool m_sendHover = false;
    RECT m_sendRect = {};

    // Worker thread for AI/command processing
    std::thread m_processThread;

    // Original wndproc of the input edit control (for subclassing)
    WNDPROC m_prevInputWndProc = nullptr;

    // Tray
    NOTIFYICONDATAW m_nid = {};
    bool m_trayIconAdded = false;
    HICON m_hIcon = nullptr;

    // Brushes for dark theme
    HBRUSH m_hBgBrush = nullptr;
    HBRUSH m_hChatBgBrush = nullptr;
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
