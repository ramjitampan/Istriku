#include "TrayWindow.h"

#include "chat/ChatController.h"
#include "chat/Conversation.h"
#include "system/Command.h"
#include "system/IntentDetector.h"
#include "system/WindowsController.h"
#include "utils/AudioResampler.h"
#include "utils/Logger.h"
#include "utils/TextNormalizer.h"
#include "utils/Utf8Sanitizer.h"

#include "whisper.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <stdexcept>


const wchar_t* TrayWindow::kHiddenClass = L"YukiTrayHiddenClass";
const wchar_t* TrayWindow::kPttClass = L"YukiPttWindowClass";

static constexpr int kTargetSampleRate = 16000;
static constexpr int kBufferDurationMs = 250;
static constexpr int kNumBuffers = 4;
static constexpr int kSampleSize = sizeof(int16_t);
static constexpr int kMaxSpeechMs = 30000;
static constexpr int kSilenceTimeoutMs = 2000;
static constexpr int kSilenceBufferTimeout = kSilenceTimeoutMs / kBufferDurationMs;
static constexpr int kMinSpeechSamples = (2000 * kTargetSampleRate) / 1000;
static constexpr int kMaxSpeechSamples = (kMaxSpeechMs * kTargetSampleRate) / 1000;
static constexpr float kRmsFullScale = 3000.0f;

TrayWindow::TrayWindow(
    const Yuki::Config::Config& config,
    Yuki::Chat::ChatController& chat,
    Yuki::System::WindowsController& windowsController,
    Yuki::System::IntentDetector& intentDetector)
    : m_config(config)
    , m_chat(chat)
    , m_windowsController(windowsController)
    , m_intentDetector(intentDetector)
{
    memset(&m_nid, 0, sizeof(m_nid));
}

TrayWindow::~TrayWindow()
{
    Destroy();
}

bool TrayWindow::Create()
{
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    // Register hidden window class
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = HiddenWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = kHiddenClass;
        wc.hbrBackground = nullptr;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&wc))
        {
            Logger::Error("TrayWindow: gagal register hidden class");
            return false;
        }
        m_hiddenClassRegistered = true;
    }

    // Register PTT window class
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PttWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = kPttClass;
        wc.hbrBackground = nullptr;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&wc))
        {
            Logger::Error("TrayWindow: gagal register PTT class");
            return false;
        }
        m_pttClassRegistered = true;
    }

    // Create hidden main window
    m_hwndHidden = CreateWindowExW(
        0, kHiddenClass, L"Yuki",
        WS_POPUP, 0, 0, 0, 0,
        nullptr, nullptr, hInstance, this);
    if (!m_hwndHidden)
    {
        Logger::Error("TrayWindow: gagal create hidden window");
        return false;
    }

    // Load whisper model
    {
        whisper_log_set([](ggml_log_level, const char*, void*) {}, nullptr);
        auto cparams = whisper_context_default_params();
        cparams.use_gpu = false;
        m_whisper = whisper_init_from_file_with_params(
            m_config.voice.model.c_str(), cparams);
        if (!m_whisper)
        {
            Logger::Error("TrayWindow: gagal load model: " + m_config.voice.model);
            MessageBoxA(nullptr,
                ("Gagal memuat model speech-to-text:\n" +
                 m_config.voice.model + "\n\nPastikan file tersebut ada.").c_str(),
                "Yuki - Error", MB_OK | MB_ICONERROR);
            return false;
        }
        Logger::Info("TrayWindow: model loaded");
    }

    // Create brushes and fonts
    m_hBgBrush = CreateSolidBrush(kBgColor);
    m_hChatBgBrush = CreateSolidBrush(kChatBg);
    m_kLevelBgBrush = CreateSolidBrush(kLevelBg);
    m_hMicIdleBrush = CreateSolidBrush(kMicIdle);
    m_hMicHoverBrush = CreateSolidBrush(kMicHover);
    m_hMicRecordingBrush = CreateSolidBrush(kMicRecording);
    m_hChatFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Create PTT window (hidden initially)
    if (!CreatePttWindow())
    {
        Logger::Error("TrayWindow: gagal create PTT window");
        return false;
    }

    // Create tray icon
    if (!CreateTrayIcon())
    {
        Logger::Error("TrayWindow: gagal create tray icon");
        return false;
    }

    // Start audio capture
    if (!StartAudioCapture())
    {
        Logger::Error("TrayWindow: gagal start audio capture (mic mungkin tidak tersedia)");
    }

    return true;
}

int TrayWindow::RunMessageLoop()
{
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void TrayWindow::Destroy()
{
    m_stopping = true;

    // Signal capture thread to stop
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
    }
    m_queueCv.notify_one();

    if (m_captureThread.joinable())
        m_captureThread.join();

    if (m_inferenceThread.joinable())
        m_inferenceThread.join();

    StopAudioCapture();

    RemoveTrayIcon();

    if (m_whisper)
    {
        whisper_free(m_whisper);
        m_whisper = nullptr;
    }

    if (m_hwndPtt)
    {
        DestroyWindow(m_hwndPtt);
        m_hwndPtt = nullptr;
    }

    if (m_hwndHidden)
    {
        DestroyWindow(m_hwndHidden);
        m_hwndHidden = nullptr;
    }

    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    if (m_hiddenClassRegistered)
    {
        UnregisterClassW(kHiddenClass, hInstance);
        m_hiddenClassRegistered = false;
    }
    if (m_pttClassRegistered)
    {
        UnregisterClassW(kPttClass, hInstance);
        m_pttClassRegistered = false;
    }

    if (m_hIcon)
    {
        DestroyIcon(m_hIcon);
        m_hIcon = nullptr;
    }

    SafeDeleteBrush(m_hBgBrush);
    SafeDeleteBrush(m_hChatBgBrush);
    SafeDeleteBrush(m_kLevelBgBrush);
    SafeDeleteBrush(m_hMicIdleBrush);
    SafeDeleteBrush(m_hMicHoverBrush);
    SafeDeleteBrush(m_hMicRecordingBrush);
    if (m_hChatFont) { DeleteObject(m_hChatFont); m_hChatFont = nullptr; }
}

// ============================================================
// Hidden window procedure
// ============================================================

LRESULT CALLBACK TrayWindow::HiddenWndProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayWindow* self = nullptr;
    if (msg == WM_CREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TrayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<TrayWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HiddenMessageHandler(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayWindow::HiddenMessageHandler(
    UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            Logger::Info("TrayWindow: hidden window created");
            return 0;
        }

        case WM_DESTROY:
        {
            Logger::Info("TrayWindow: shutdown");
            PostQuitMessage(0);
            return 0;
        }

        case WM_TRAY_NOTIFY:
        {
            switch (lParam)
            {
                case WM_LBUTTONUP:
                    TogglePttWindow();
                    return 0;

                case WM_RBUTTONUP:
                {
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, 1001, L"Keluar");
                    SetForegroundWindow(m_hwndHidden);
                    POINT pt;
                    GetCursorPos(&pt);
                    int cmd = TrackPopupMenu(hMenu,
                        TPM_RETURNCMD | TPM_NONOTIFY,
                        pt.x, pt.y, 0, m_hwndHidden, nullptr);
                    DestroyMenu(hMenu);
                    if (cmd == 1001)
                    {
                        DestroyWindow(m_hwndHidden);
                    }
                    return 0;
                }
            }
            return 0;
        }

        case WM_UPDATE_LEVEL:
        {
            int percent = static_cast<int>(lParam);
            UpdateLevelBar(static_cast<float>(percent) / 100.0f);
            return 0;
        }

        case WM_UPDATE_CHAT:
        {
            // wParam = 1 for user message, 0 for assistant reply
            auto* text = reinterpret_cast<std::string*>(lParam);
            if (text)
            {
                AppendToChat(*text, wParam != 0);
                delete text;
            }
            return 0;
        }

        default:
            return DefWindowProcW(m_hwndHidden, msg, wParam, lParam);
    }
}

// ============================================================
// PTT window procedure
// ============================================================

LRESULT CALLBACK TrayWindow::PttWndProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayWindow* self = nullptr;
    if (msg == WM_CREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TrayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
        self->m_hwndPtt = hwnd; // save HWND before WM_CREATE handlers run
    }
    else
    {
        self = reinterpret_cast<TrayWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->PttMessageHandler(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayWindow::PttMessageHandler(
    UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            Logger::Info("TrayWindow: PTT WM_CREATE — creating controls...");
            CreateChatLog();
            CreateMicButton();
            CreateLevelMeter();
            LayoutControls();

            RECT rc;
            GetClientRect(m_hwndPtt, &rc);
            Logger::Info("TrayWindow: PTT client rect=" +
                         std::to_string(rc.left) + "," +
                         std::to_string(rc.top) + "," +
                         std::to_string(rc.right) + "," +
                         std::to_string(rc.bottom));
            Logger::Info("TrayWindow: mic rect=" +
                         std::to_string(m_micRect.left) + "," +
                         std::to_string(m_micRect.top) + "," +
                         std::to_string(m_micRect.right) + "," +
                         std::to_string(m_micRect.bottom));
            if (m_hChatLog)
            {
                RECT cr;
                GetClientRect(m_hChatLog, &cr);
                Logger::Info("TrayWindow: edit client rect=" +
                             std::to_string(cr.left) + "," +
                             std::to_string(cr.top) + "," +
                             std::to_string(cr.right) + "," +
                             std::to_string(cr.bottom));
            }

            LoadChatHistory();
            return 0;
        }

        case WM_DESTROY:
        {
            return 0;
        }

        case WM_ACTIVATE:
        {
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                // Check if the mouse click was on the tray icon
                // (we shouldn't hide if user is just clicking tray)
                // For simplicity, hide on any deactivation
                HidePttWindow();
            }
            return 0;
        }

        case WM_NCACTIVATE:
        {
            if (wParam == FALSE)
            {
                HidePttWindow();
                return TRUE;
            }
            return DefWindowProcW(m_hwndPtt, msg, wParam, lParam);
        }

        case WM_SIZE:
        {
            LayoutControls();
            InvalidateRect(m_hwndPtt, nullptr, TRUE);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwndPtt, &ps);

            // Fill background
            RECT rc;
            GetClientRect(m_hwndPtt, &rc);
            FillRect(hdc, &rc, m_hBgBrush);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 1, kBorderColor);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);

            // Draw controls
            if (m_micRect.right > m_micRect.left)
                DrawMicButton(hdc, m_micHover);
            if (m_levelRect.right > m_levelRect.left)
                DrawLevelMeter(hdc);

            EndPaint(m_hwndPtt, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, kChatBg);
            SetTextColor(hdc, kTextColor);
            return reinterpret_cast<LRESULT>(m_hChatBgBrush);
        }

        case WM_ERASEBKGND:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc;
            GetClientRect(m_hwndPtt, &rc);
            FillRect(hdc, &rc, m_hBgBrush);
            return TRUE;
        }

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool overMic = PtInRect(&m_micRect, pt);
            if (overMic != m_micHover)
            {
                m_micHover = overMic;
                InvalidateRect(m_hwndPtt, &m_micRect, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hwndPtt, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
        {
            m_micHover = false;
            InvalidateRect(m_hwndPtt, &m_micRect, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&m_micRect, pt))
            {
                PttState expected = PttState::Idle;
                if (m_pttState.compare_exchange_strong(expected, PttState::Capturing))
                {
                    m_capturing.store(true);
                    {
                        std::lock_guard<std::mutex> lock(m_pttMutex);
                        m_pttBuffer.clear();
                    }
                    // Invalidate mic button to show recording state
                    InvalidateRect(m_hwndPtt, &m_micRect, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            PttState expected = PttState::Capturing;
            if (m_pttState.compare_exchange_strong(expected, PttState::Processing))
            {
                m_capturing.store(false);

                // Copy the captured audio
                std::vector<float> audio;
                {
                    std::lock_guard<std::mutex> lock(m_pttMutex);
                    audio = std::move(m_pttBuffer);
                    m_pttBuffer.clear();
                }

                // Reset level meter
                m_lastRms = 0.0f;
                PostMessage(m_hwndHidden, WM_UPDATE_LEVEL, 0, 0);

                // Invalidate mic button
                InvalidateRect(m_hwndPtt, &m_micRect, FALSE);

                if (audio.size() >= static_cast<size_t>(kMinSpeechSamples))
                {
                    // Run inference on a worker thread
                    if (m_inferenceThread.joinable())
                        m_inferenceThread.join();
                    m_inferenceThread = std::thread([this, audio = std::move(audio)]()
                    {
                        RunInference(audio);
                        m_pttState.store(PttState::Idle);
                    });
                }
                else
                {
                    // Too short, ignore
                    m_pttState.store(PttState::Idle);
                }
            }
            return 0;
        }

        default:
            return DefWindowProcW(m_hwndPtt, msg, wParam, lParam);
    }
}

// ============================================================
// Tray icon
// ============================================================

bool TrayWindow::CreateTrayIcon()
{
    m_hIcon = CreatePlaceholderIcon();

    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwndHidden;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_NOTIFY;
    m_nid.hIcon = m_hIcon;
    wcscpy_s(m_nid.szTip, L"Yuki - Push to Talk");

    m_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
    if (!m_trayIconAdded)
    {
        Logger::Error("TrayWindow: Shell_NotifyIcon NIM_ADD gagal");
    }
    return m_trayIconAdded;
}

void TrayWindow::RemoveTrayIcon()
{
    if (m_trayIconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayIconAdded = false;
    }
}

// ============================================================
// PTT window
// ============================================================

bool TrayWindow::CreatePttWindow()
{
    DWORD style = WS_POPUP | WS_CLIPCHILDREN;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

    m_hwndPtt = CreateWindowExW(
        exStyle, kPttClass, L"Yuki - Push to Talk",
        style,
        0, 0, kPttWidth, kPttHeight,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);

    if (!m_hwndPtt)
    {
        Logger::Error("TrayWindow: CreateWindowEx PTT gagal");
        return false;
    }

    return true;
}

void TrayWindow::ShowPttWindow()
{
    if (!m_hwndPtt)
    {
        Logger::Error("TrayWindow: ShowPttWindow — hwnd null");
        return;
    }

    Logger::Info("TrayWindow: ShowPttWindow — positioning...");
    PositionNearTray();

    Logger::Info("TrayWindow: ShowPttWindow — showing...");
    ShowWindow(m_hwndPtt, SW_SHOW);

    Logger::Info("TrayWindow: ShowPttWindow — setting topmost...");
    SetWindowPos(m_hwndPtt, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    Logger::Info("TrayWindow: ShowPttWindow — invalidating...");
    InvalidateRect(m_hwndPtt, nullptr, TRUE);

    RECT wr;
    GetWindowRect(m_hwndPtt, &wr);
    Logger::Info("TrayWindow: PTT window rect=" +
                 std::to_string(wr.left) + "," +
                 std::to_string(wr.top) + "," +
                 std::to_string(wr.right) + "," +
                 std::to_string(wr.bottom) +
                 " visible=" + std::to_string(IsWindowVisible(m_hwndPtt)));
}

void TrayWindow::HidePttWindow()
{
    if (m_hwndPtt && IsWindowVisible(m_hwndPtt))
    {
        // If currently capturing, stop it
        PttState expected = PttState::Capturing;
        if (m_pttState.compare_exchange_strong(expected, PttState::Idle))
        {
            m_capturing.store(false);
            std::lock_guard<std::mutex> lock(m_pttMutex);
            m_pttBuffer.clear();
            m_lastRms = 0.0f;
            PostMessage(m_hwndHidden, WM_UPDATE_LEVEL, 0, 0);
        }

        ShowWindow(m_hwndPtt, SW_HIDE);
    }
}

void TrayWindow::TogglePttWindow()
{
    if (m_hwndPtt && IsWindowVisible(m_hwndPtt))
        HidePttWindow();
    else
        ShowPttWindow();
}

void TrayWindow::PositionNearTray()
{
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    // Position near bottom-right of the work area
    int x = workArea.right - kPttWidth - 8;
    int y = workArea.bottom - kPttHeight - 8;

    // Clamp to positive coordinates
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    SetWindowPos(m_hwndPtt, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ============================================================
// Controls
// ============================================================

void TrayWindow::CreateChatLog()
{
    // Verify parent HWND is valid
    if (!m_hwndPtt || !IsWindow(m_hwndPtt))
    {
        Logger::Error("TrayWindow: CreateChatLog — parent HWND invalid");
        return;
    }

    Logger::Info("TrayWindow: CreateChatLog — parent HWND valid");

    m_hChatLog = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 0, 0,
        m_hwndPtt, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_hChatLog)
    {
        DWORD err = GetLastError();
        Logger::Error("TrayWindow: CreateChatLog GAGAL — GetLastError=" +
                      std::to_string(err));
        return;
    }

    Logger::Info("TrayWindow: Edit control created (HWND=" +
                 std::to_string(reinterpret_cast<uintptr_t>(m_hChatLog)) + ")");

    if (m_hChatFont)
    {
        SendMessageW(m_hChatLog, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_hChatFont), TRUE);
    }

    // Remove client edge border (we draw our own)
    SetWindowLongPtrW(m_hChatLog, GWL_EXSTYLE,
        GetWindowLongPtrW(m_hChatLog, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    SetWindowPos(m_hChatLog, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    Logger::Info("TrayWindow: Edit control styled");
}

void TrayWindow::CreateMicButton()
{
    // Mic button is drawn directly on the PTT window (not a separate HWND)
    // The rect is calculated in LayoutControls
}

void TrayWindow::CreateLevelMeter()
{
    // Level meter is drawn directly on the PTT window
    // The rect is calculated in LayoutControls
}

void TrayWindow::LayoutControls()
{
    RECT rc;
    GetClientRect(m_hwndPtt, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Layout: top-down: chat log | padding | level meter | padding | mic button | padding
    int micY = height - kPadding - kMicButtonSize;
    int levelY = micY - kPadding - kLevelMeterHeight;
    int chatBottom = levelY - kPadding;

    RECT chatRect = { kPadding, kPadding, width - kPadding, chatBottom };

    if (m_hChatLog)
    {
        MoveWindow(m_hChatLog,
                   chatRect.left, chatRect.top,
                   chatRect.right - chatRect.left,
                   chatRect.bottom - chatRect.top,
                   TRUE);
    }

    // Mic button: centered at bottom
    int micX = (width - kMicButtonSize) / 2;
    m_micRect = { micX, micY, micX + kMicButtonSize, micY + kMicButtonSize };

    // Level meter: between chat log and mic button
    m_levelRect = {
        kPadding + 20,
        levelY,
        width - kPadding - 20,
        levelY + kLevelMeterHeight
    };
}

void TrayWindow::AppendToChat(const std::string& text, bool isUser)
{
    if (!m_hChatLog) return;

    std::string prefix = isUser ? "\r\nAnda: " : "\r\nYuki: ";
    std::string line = prefix + text;

    int len = GetWindowTextLengthW(m_hChatLog);
    SendMessageA(m_hChatLog, EM_SETSEL, len, len);
    SendMessageA(m_hChatLog, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(m_hChatLog, EM_SCROLLCARET, 0, 0);
}

void TrayWindow::UpdateLevelBar(float percent)
{
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;
    m_lastRms = percent;

    if (m_hwndPtt && IsWindowVisible(m_hwndPtt))
    {
        InvalidateRect(m_hwndPtt, &m_levelRect, FALSE);
    }
}

void TrayWindow::LoadChatHistory()
{
    // Welcome message
    AppendToChat("Yuki siap — tekan tombol mic untuk bicara", false);

    const auto& messages = m_chat.GetConversation().GetMessages();
    for (const auto& msg : messages)
    {
        std::string text = msg.content;
        AppendToChat(text, msg.role == Yuki::Chat::MessageRole::User);
    }
}

// ============================================================
// Drawing
// ============================================================

void TrayWindow::DrawMicButton(HDC hdc, bool hover)
{
    COLORREF color;
    PttState state = m_pttState.load();

    if (state == PttState::Capturing)
    {
        color = hover ? kMicRecordingHover : kMicRecording;
    }
    else
    {
        color = hover ? kMicHover : kMicIdle;
    }

    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    int r = kMicButtonSize / 2;
    int cx = m_micRect.left + r;
    int cy = m_micRect.top + r;

    Ellipse(hdc, m_micRect.left, m_micRect.top,
            m_micRect.right, m_micRect.bottom);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);

    // Draw a simple microphone icon using GDI
    // Mic body: a rounded rect (approximated as rectangle + ellipse)
    HPEN hWhitePen = CreatePen(PS_SOLID, 2, kTextColor);
    HPEN hOldPen2 = (HPEN)SelectObject(hdc, hWhitePen);
    HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    int micW = r * 3 / 5;      // ~19
    int micH = r * 4 / 5;      // ~25
    int micX = cx - micW / 2;
    int micY = cy - micH / 2 + 2;

    // Mic capsule: rounded top, straight sides, flat bottom
    int capsuleW = micW;
    int capsuleH = micH * 3 / 5;  // top part
    RoundRect(hdc, micX, micY, micX + capsuleW, micY + capsuleH, micW, micW);

    // Mic stand: two lines going down
    int standTop = micY + capsuleH;
    int standBottom = cy + r * 2 / 5;
    int standCenterX = cx;
    MoveToEx(hdc, standCenterX, standTop, nullptr);
    LineTo(hdc, standCenterX, standBottom);

    // Arc at the bottom of the stand
    int arcR = micW / 3;
    Arc(hdc, standCenterX - arcR, standBottom - arcR,
        standCenterX + arcR, standBottom + arcR,
        standCenterX - arcR, standBottom,
        standCenterX + arcR, standBottom);

    // Small base
    int baseY = standBottom + arcR;
    MoveToEx(hdc, micX, baseY, nullptr);
    LineTo(hdc, micX + capsuleW, baseY);

    SelectObject(hdc, hOldPen2);
    SelectObject(hdc, hOldBrush2);
    DeleteObject(hWhitePen);

    // Draw recording indicator (red dot) when capturing
    if (state == PttState::Capturing)
    {
        HBRUSH hRedBrush = CreateSolidBrush(kLevelHigh);
        HBRUSH hOldBrush3 = (HBRUSH)SelectObject(hdc, hRedBrush);
        HPEN hNoPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, m_micRect.right - 12, m_micRect.top + 4,
                m_micRect.right - 4, m_micRect.top + 12);
        SelectObject(hdc, hOldBrush3);
        SelectObject(hdc, hNoPen);
        DeleteObject(hRedBrush);
    }
}

void TrayWindow::DrawLevelMeter(HDC hdc)
{
    if (m_levelRect.right <= m_levelRect.left) return;

    // Background
    FillRect(hdc, &m_levelRect, m_kLevelBgBrush);

    // Active bar
    int barWidth = m_levelRect.right - m_levelRect.left;
    int filledWidth = static_cast<int>(barWidth * m_lastRms);
    if (filledWidth > 0)
    {
        RECT fillRect = m_levelRect;
        fillRect.right = fillRect.left + filledWidth;

        COLORREF levelColor;
        float pct = m_lastRms;
        if (pct < 0.5f)
            levelColor = kLevelLow;
        else if (pct < 0.8f)
            levelColor = kLevelMid;
        else
            levelColor = kLevelHigh;

        HBRUSH brush = CreateSolidBrush(levelColor);
        FillRect(hdc, &fillRect, brush);
        DeleteObject(brush);
    }
}

// ============================================================
// Audio capture
// ============================================================

bool TrayWindow::StartAudioCapture()
{
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = kTargetSampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    MMRESULT result = waveInOpen(
        &m_micHandle, WAVE_MAPPER, &wfx,
        reinterpret_cast<DWORD_PTR>(&WaveCallbackStatic),
        reinterpret_cast<DWORD_PTR>(this),
        CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR)
    {
        Logger::Error("TrayWindow: waveInOpen gagal: " + std::to_string(result));
        return false;
    }

    m_audioSampleRate = kTargetSampleRate;
    DWORD bufferFrames = (m_audioSampleRate * kBufferDurationMs) / 1000;
    DWORD bufferBytes = bufferFrames * sizeof(int16_t);

    m_captureBuffers.resize(kNumBuffers);
    m_captureHeaders.resize(kNumBuffers);

    for (int i = 0; i < kNumBuffers; ++i)
    {
        m_captureBuffers[i].resize(bufferFrames);
        m_captureHeaders[i].lpData =
            reinterpret_cast<LPSTR>(m_captureBuffers[i].data());
        m_captureHeaders[i].dwBufferLength = bufferBytes;
        m_captureHeaders[i].dwFlags = 0;
        m_captureHeaders[i].dwUser = static_cast<DWORD_PTR>(i);

        result = waveInPrepareHeader(
            m_micHandle, &m_captureHeaders[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR)
        {
            Logger::Error("TrayWindow: waveInPrepareHeader gagal");
            waveInClose(m_micHandle);
            m_micHandle = nullptr;
            return false;
        }

        waveInAddBuffer(m_micHandle, &m_captureHeaders[i], sizeof(WAVEHDR));
    }

    // Start capture thread
    m_stopping = false;
    m_captureThread = std::thread([this] { CaptureThread(); });

    result = waveInStart(m_micHandle);
    if (result != MMSYSERR_NOERROR)
    {
        Logger::Error("TrayWindow: waveInStart gagal");
        waveInClose(m_micHandle);
        m_micHandle = nullptr;
        return false;
    }

    Logger::Info("TrayWindow: audio capture started");
    return true;
}

void TrayWindow::StopAudioCapture()
{
    if (m_micHandle)
    {
        waveInReset(m_micHandle);

        for (auto& hdr : m_captureHeaders)
        {
            if (hdr.dwFlags & WHDR_PREPARED)
            {
                waveInUnprepareHeader(m_micHandle, &hdr, sizeof(WAVEHDR));
            }
        }

        waveInClose(m_micHandle);
        m_micHandle = nullptr;

        m_captureBuffers.clear();
        m_captureHeaders.clear();
        Logger::Info("TrayWindow: audio capture stopped");
    }
}

void CALLBACK TrayWindow::WaveCallbackStatic(
    HWAVEIN, UINT uMsg, DWORD_PTR dwInstance,
    DWORD_PTR dwParam1, DWORD_PTR)
{
    if (uMsg != WIM_DATA) return;

    auto* self = reinterpret_cast<TrayWindow*>(dwInstance);
    auto* header = reinterpret_cast<WAVEHDR*>(dwParam1);

    if (self->m_stopping.load()) return;

    {
        std::lock_guard<std::mutex> lock(self->m_queueMutex);
        self->m_audioQueue.push(header);
    }
    self->m_queueCv.notify_one();
}

void TrayWindow::CaptureThread()
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

        if (m_capturing.load())
        {
            float rms = ComputeRMS(samples, nSamples);
            float normalized = std::min(1.0f, rms / kRmsFullScale);
            int percent = static_cast<int>(normalized * 100.0f);

            PostMessage(m_hwndHidden, WM_UPDATE_LEVEL, 0,
                        static_cast<LPARAM>(percent));

            // Append to PTT buffer
            {
                std::lock_guard<std::mutex> lock(m_pttMutex);
                for (int i = 0; i < nSamples; ++i)
                {
                    m_pttBuffer.push_back(
                        static_cast<float>(samples[i]) / 32768.0f);
                }
            }
        }

        if (!m_stopping.load())
        {
            waveInAddBuffer(m_micHandle, header, sizeof(WAVEHDR));
        }
    }
}

float TrayWindow::ComputeRMS(const int16_t* samples, int nSamples)
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

// ============================================================
// Whisper inference
// ============================================================

void TrayWindow::RunInference(const std::vector<float>& audio)
{
    auto startTime = std::chrono::steady_clock::now();

    auto wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.single_segment   = true;
    wparams.suppress_blank   = true;
    wparams.suppress_nst     = true;
    wparams.language         = m_config.voice.language.c_str();
    wparams.n_threads        = m_config.voice.threads;
    wparams.no_timestamps    = true;
    wparams.no_context       = true;
    wparams.temperature      = 0.0f;
    wparams.greedy.best_of   = 2;
    wparams.initial_prompt   = m_config.voice.initial_prompt.c_str();

    int ret = whisper_full(m_whisper, wparams,
        audio.data(), static_cast<int>(audio.size()));

    if (ret != 0)
    {
        Logger::Error("TrayWindow: whisper gagal: " + std::to_string(ret));
        auto* err = new std::string("Whisper inference gagal.");
        PostMessage(m_hwndHidden, WM_UPDATE_CHAT, 0,
                    reinterpret_cast<LPARAM>(err));
        return;
    }

    // Extract text and confidence
    int nSegments = whisper_full_n_segments(m_whisper);
    std::string rawText;
    float confidence = 1.0f;

    for (int i = 0; i < nSegments; ++i)
    {
        const char* seg = whisper_full_get_segment_text(m_whisper, i);
        if (seg)
        {
            if (!rawText.empty()) rawText += ' ';
            rawText += seg;
        }

        float noSpeechProb = whisper_full_get_segment_no_speech_prob(
            m_whisper, i);
        float segConf = 1.0f - noSpeechProb;
        if (segConf < confidence)
            confidence = segConf;
    }

    auto inferEnd = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        inferEnd - startTime).count();

    // Sanitize and normalize
    std::string sanitized = Yuki::Utils::Utf8Sanitizer::Sanitize(rawText);
    std::string normalized = Yuki::Utils::TextNormalizer::NormalizeRecognition(sanitized);

    Logger::Info("TrayWindow: Raw: \"" + rawText + "\"");
    Logger::Info("TrayWindow: Normalized: \"" + normalized + "\"");
    Logger::Info("TrayWindow: Confidence: " + std::to_string(confidence));
    Logger::Info("TrayWindow: Execution: " + std::to_string(totalMs) + "ms");

    // Confidence filter
    if (confidence < m_config.voice.confidence_threshold)
    {
        auto* text = new std::string("Maaf, aku kurang yakin mendengarnya.");
        PostMessage(m_hwndHidden, WM_UPDATE_CHAT, 0,
                    reinterpret_cast<LPARAM>(text));
        return;
    }

    // Too short noise filter
    if (normalized.size() < 3 && confidence < 0.8f) return;

    // Show user's transcribed text in chat
    auto* userText = new std::string(normalized);
    PostMessage(m_hwndHidden, WM_UPDATE_CHAT, 1,
                reinterpret_cast<LPARAM>(userText));

    // Process — either desktop command or AI
    std::string reply = ProcessCommandText(normalized);

    if (!reply.empty())
    {
        auto* replyText = new std::string(reply);
        PostMessage(m_hwndHidden, WM_UPDATE_CHAT, 0,
                    reinterpret_cast<LPARAM>(replyText));
    }
}

std::string TrayWindow::ProcessCommandText(const std::string& text)
{
    using Yuki::Chat::ConversationMode;

    if (m_chat.GetMode() == ConversationMode::Curhat)
    {
        // ============================================================
        // Mode CURHAT — command dikenal tetap dieksekusi, sisanya ke AI
        // ============================================================

        // Cek exit phrase dulu
        if (Yuki::Chat::ChatController::IsExitPhrase(
                text, m_config.voice.curhat_exit_phrases))
        {
            Logger::Info("[INFO] Mode changed: Curhat -> Perintah (exit: \"" +
                         text + "\")");
            m_chat.SetMode(ConversationMode::Perintah);

            // JANGAN kirim ke AI, cukup balasan lokal
            const char* replies[] = {
                "Oke, aku balik siaga ya~",
                "Baik sayang, kalau butuh aku lagi, panggil aja.",
                "Siap, kembali ke mode perintah. Aku di sini kok."
            };
            int idx = (std::rand() / 32) % 3;
            return replies[idx];
        }

        // Command yang dikenal tetap dieksekusi langsung walau lagi curhat
        Yuki::System::Command command = m_intentDetector.Detect(text);
        if (command.type != Yuki::System::CommandType::Unknown)
        {
            Logger::Info("TrayWindow: Intent (curhat mode): " +
                         std::string(Yuki::System::CommandTypeToString(command.type)));

            if (command.type == Yuki::System::CommandType::Brightness ||
                command.type == Yuki::System::CommandType::Volume)
            {
                return m_chat.SendMessage(text);
            }

            Yuki::System::CommandResult result =
                m_windowsController.Execute(command);

            if (result.confirmationRequired)
            {
                return m_chat.SendMessage(text);
            }

            return result.message;
        }

        // Bukan command — kirim ke AI dengan persona
        Logger::Info("TrayWindow: Sending to AI (curhat mode)...");
        return m_chat.SendMessage(text);
    }

    // ============================================================
    // Mode PERINTAH (default)
    // ============================================================

    // Check if it's a desktop command (pre-AI routing)
    Yuki::System::Command command = m_intentDetector.Detect(text);

    if (command.type != Yuki::System::CommandType::Unknown)
    {
        Logger::Info("TrayWindow: Intent: " +
                     std::string(Yuki::System::CommandTypeToString(command.type)));

        if (command.type == Yuki::System::CommandType::Brightness ||
            command.type == Yuki::System::CommandType::Volume)
        {
            // Let ChatController handle brightness/volume logic
            return m_chat.SendMessage(text);
        }

        Yuki::System::CommandResult result =
            m_windowsController.Execute(command);

        if (result.confirmationRequired)
        {
            return m_chat.SendMessage(text);
        }

        return result.message;
    }

    // Intent tidak dikenal — cek trigger phrase untuk masuk mode curhat
    if (Yuki::Chat::ChatController::IsTriggerPhrase(
            text, m_config.voice.curhat_trigger_phrases))
    {
        Logger::Info("[INFO] Mode changed: Perintah -> Curhat (trigger: \"" +
                     text + "\")");
        m_chat.SetMode(ConversationMode::Curhat);

        // Kirim teks ini ke AI dengan persona
        Logger::Info("TrayWindow: Sending to AI (curhat trigger)...");
        return m_chat.SendMessage(text);
    }

    // Bukan intent, bukan trigger curhat, dan bukan exit — fallback lokal
    Logger::Info("TrayWindow: No match — local fallback reply (no AI)");
    return GetRandomFallbackReply();
}

std::string TrayWindow::GetRandomFallbackReply()
{
    static const char* replies[] = {
        "Perintah apa sayang? Aku belum ngerti. Bilang 'aku mau curhat' kalau mau ngobrol santai ya.",
        "Hmm, aku gak nangkep maksudnya. Ulangi perintahnya, atau ajak aku curhat kalau lagi pengen cerita~",
        "Maaf, aku belum paham. Coba perintah lagi, atau bilang 'mau cerita' kalau ingin ngobrol."
    };
    int idx = (std::rand() / 32) % 3;
    return replies[idx];
}

// ============================================================
// Placeholder icon
// ============================================================

HICON TrayWindow::CreatePlaceholderIcon()
{
    const int size = 32;
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC)
    {
        ReleaseDC(nullptr, hdc);
        return LoadIcon(nullptr, IDI_APPLICATION);
    }

    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;
    bi.bV5AlphaMask = 0xFF000000;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(
        memDC, reinterpret_cast<BITMAPINFO*>(&bi),
        DIB_RGB_COLORS, &bits, nullptr, 0);

    if (!hBitmap || !bits)
    {
        if (hBitmap) DeleteObject(hBitmap);
        DeleteDC(memDC);
        ReleaseDC(nullptr, hdc);
        return LoadIcon(nullptr, IDI_APPLICATION);
    }

    auto* pixels = static_cast<uint32_t*>(bits);

    // Draw: blue rounded square with white "Y"
    const uint32_t blue = 0xFF0078D7;     // ARGB
    const uint32_t white = 0xFFFFFFFF;
    const uint32_t transparent = 0x00000000;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // Rounded square with radius ~6px
            int dx = (x < 6) ? (6 - x) : (x > size - 7) ? (x - (size - 7)) : 0;
            int dy = (y < 6) ? (6 - y) : (y > size - 7) ? (y - (size - 7)) : 0;
            bool inside = (dx == 0 && dy == 0) || (dx * dx + dy * dy <= 36);

            if (inside)
            {
                pixels[y * size + x] = blue;
            }
            else
            {
                pixels[y * size + x] = transparent;
            }
        }
    }

    // Draw white "Y" character using simple pixel pattern
    // Centered at (16, 16)
    const int cy = 16;
    const int cx = 16;
    // Y shape: top- Left diagonal, top-Right diagonal, stem down
    for (int dy = -6; dy <= 6; ++dy)
    {
        int px = cx + dy;
        int py = cy + dy;
        if (px >= 0 && px < size && py >= 0 && py < size)
            pixels[py * size + px] = white;

        px = cx - dy;
        py = cy + dy;
        if (px >= 0 && px < size && py >= 0 && py < size)
            pixels[py * size + px] = white;
    }
    // Stem (lower half of Y)
    for (int dy = 0; dy <= 3; ++dy)
    {
        int px = cx;
        int py = cy + dy;
        if (px >= 0 && px < size && py >= 0 && py < size)
            pixels[py * size + px] = white;
    }

    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hBitmap);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hBitmap;
    ii.hbmMask = CreateBitmap(size, size, 1, 1, nullptr);

    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(memDC, hOldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(ii.hbmMask);
    DeleteObject(hBitmap);

    return hIcon;
}
