#include "TrayWindow.h"

#include "chat/ChatController.h"
#include "chat/CommandRouter.h"
#include "chat/Conversation.h"
#include "utils/Logger.h"

#include <windowsx.h>

#include <cstdlib>
#include <string>

const wchar_t* TrayWindow::kHiddenClass = L"YukiTrayHiddenClass";
const wchar_t* TrayWindow::kPttClass = L"YukiPttWindowClass";

static std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};

    int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};

    std::wstring result(len, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), &result[0], len);
    return result;
}

static std::string ToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};

    int len = WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};

    std::string result(len, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), &result[0], len, nullptr, nullptr);
    return result;
}

static std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

TrayWindow::TrayWindow(
    const Yuki::Config::Config& config,
    Yuki::Chat::CommandRouter& router)
    : m_config(config)
    , m_router(router)
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

    // Register chat window class
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
            Logger::Error("TrayWindow: gagal register chat class");
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

    // Create brushes and fonts
    m_hBgBrush = CreateSolidBrush(kBgColor);
    m_hChatBgBrush = CreateSolidBrush(kChatBg);
    m_hChatFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Create chat window (hidden initially)
    if (!CreatePttWindow())
    {
        Logger::Error("TrayWindow: gagal create chat window");
        return false;
    }

    // Create tray icon
    if (!CreateTrayIcon())
    {
        Logger::Error("TrayWindow: gagal create tray icon");
        return false;
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
    if (m_processThread.joinable())
        m_processThread.join();

    RemoveTrayIcon();

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
// Chat window procedure
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
            Logger::Info("TrayWindow: chat WM_CREATE — creating controls...");
            CreateChatLog();
            CreateInputBox();
            LayoutControls();
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
            DrawSendButton(hdc);

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
            bool overSend = PtInRect(&m_sendRect, pt);
            if (overSend != m_sendHover)
            {
                m_sendHover = overSend;
                InvalidateRect(m_hwndPtt, &m_sendRect, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hwndPtt, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
        {
            m_sendHover = false;
            InvalidateRect(m_hwndPtt, &m_sendRect, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&m_sendRect, pt))
            {
                SendInputText();
            }
            return 0;
        }

        default:
            return DefWindowProcW(m_hwndPtt, msg, wParam, lParam);
    }
}

// ============================================================
// Input edit control subclassing
// ============================================================

LRESULT CALLBACK TrayWindow::InputWndProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayWindow* self = reinterpret_cast<TrayWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (self)
        return self->InputMessageHandler(msg, wParam, lParam);

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TrayWindow::InputMessageHandler(
    UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        SendInputText();
        return 0;
    }

    if (m_prevInputWndProc)
    {
        return CallWindowProcW(
            m_prevInputWndProc, m_hInput, msg, wParam, lParam);
    }

    return DefWindowProcW(m_hInput, msg, wParam, lParam);
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
    wcscpy_s(m_nid.szTip, L"Yuki");

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
// Chat window
// ============================================================

bool TrayWindow::CreatePttWindow()
{
    DWORD style = WS_POPUP | WS_CLIPCHILDREN;
    DWORD exStyle = WS_EX_TOOLWINDOW;

    m_hwndPtt = CreateWindowExW(
        exStyle, kPttClass, L"Yuki",
        style,
        0, 0, kPttWidth, kPttHeight,
        nullptr, nullptr, GetModuleHandleW(nullptr), this);

    if (!m_hwndPtt)
    {
        Logger::Error("TrayWindow: CreateWindowEx chat gagal");
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

    PositionNearTray();

    ShowWindow(m_hwndPtt, SW_SHOW);
    SetWindowPos(m_hwndPtt, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(m_hwndPtt);
    if (m_hInput) SetFocus(m_hInput);

    InvalidateRect(m_hwndPtt, nullptr, TRUE);
}

void TrayWindow::HidePttWindow()
{
    if (m_hwndPtt && IsWindowVisible(m_hwndPtt))
    {
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
    if (!m_hwndPtt || !IsWindow(m_hwndPtt))
    {
        Logger::Error("TrayWindow: CreateChatLog — parent HWND invalid");
        return;
    }

    m_hChatLog = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 0, 0,
        m_hwndPtt, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_hChatLog)
    {
        Logger::Error("TrayWindow: CreateChatLog GAGAL");
        return;
    }

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
}

void TrayWindow::CreateInputBox()
{
    if (!m_hwndPtt || !IsWindow(m_hwndPtt))
    {
        Logger::Error("TrayWindow: CreateInputBox — parent HWND invalid");
        return;
    }

    m_hInput = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP,
        0, 0, 0, 0,
        m_hwndPtt, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_hInput)
    {
        Logger::Error("TrayWindow: CreateInputBox GAGAL");
        return;
    }

    if (m_hChatFont)
    {
        SendMessageW(m_hInput, WM_SETFONT,
                     reinterpret_cast<WPARAM>(m_hChatFont), TRUE);
    }

    // Remove client edge border (we draw our own)
    SetWindowLongPtrW(m_hInput, GWL_EXSTYLE,
        GetWindowLongPtrW(m_hInput, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    SetWindowPos(m_hInput, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    // Subclass the edit control so we can catch VK_RETURN (Enter)
    SetWindowLongPtrW(m_hInput, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this));
    m_prevInputWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(m_hInput, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(InputWndProc)));
}

void TrayWindow::LayoutControls()
{
    RECT rc;
    GetClientRect(m_hwndPtt, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Layout: top-down: chat log | padding | input + send button
    int inputY = height - kPadding - kInputHeight;
    int inputX = kPadding;
    int inputWidth = width - kPadding - kSendWidth - kPadding * 2;
    int sendX = inputX + inputWidth + kPadding;

    RECT chatRect = { kPadding, kPadding, width - kPadding, inputY - kPadding };

    if (m_hChatLog)
    {
        MoveWindow(m_hChatLog,
                   chatRect.left, chatRect.top,
                   chatRect.right - chatRect.left,
                   chatRect.bottom - chatRect.top,
                   TRUE);
    }

    if (m_hInput)
    {
        MoveWindow(m_hInput, inputX, inputY, inputWidth, kInputHeight, TRUE);
    }

    m_sendRect = {
        sendX,
        inputY,
        sendX + kSendWidth,
        inputY + kInputHeight
    };
}

void TrayWindow::AppendToChat(const std::string& text, bool isUser)
{
    if (!m_hChatLog) return;

    std::string prefix = isUser ? "\r\nAnda: " : "\r\nYuki: ";
    std::wstring line = ToWide(prefix + text);

    int len = GetWindowTextLengthW(m_hChatLog);
    SendMessageW(m_hChatLog, EM_SETSEL, len, len);
    SendMessageW(m_hChatLog, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(m_hChatLog, EM_SCROLLCARET, 0, 0);
}

void TrayWindow::LoadChatHistory()
{
    // Welcome message
    const char* mode = m_router.GetMode() == Yuki::Chat::ConversationMode::Curhat
        ? "Curhat" : "Perintah";
    AppendToChat(std::string("Yuki siap. Mode aktif: ") + mode +
        ". Gunakan /curhat atau /perintah untuk berpindah mode; /mode untuk melihat mode.", false);

    const auto& messages = m_router.GetChat().GetConversation().GetMessages();
    for (const auto& msg : messages)
    {
        std::string text = msg.content;
        AppendToChat(text, msg.role == Yuki::Chat::MessageRole::User);
    }
}

void TrayWindow::SendInputText()
{
    if (!m_hInput || !IsWindow(m_hInput)) return;

    int len = GetWindowTextLengthW(m_hInput);
    if (len <= 0) return;

    std::wstring wtext(len + 1, L'\0');
    GetWindowTextW(m_hInput, &wtext[0], len + 1);
    wtext.resize(len);

    std::string text = Trim(ToUtf8(wtext));
    if (text.empty()) return;

    SetWindowTextW(m_hInput, L"");

    AppendToChat(text, true);

    if (m_processThread.joinable())
        m_processThread.join();

    m_processThread = std::thread([this, text = std::move(text)]()
    {
        ProcessInput(text);
    });
}

void TrayWindow::ProcessInput(const std::string& text)
{
    std::string reply = m_router.HandleText(text);
    if (reply.empty()) return;

    if (!IsWindow(m_hwndHidden)) return;

    auto* replyText = new std::string(std::move(reply));
    PostMessage(m_hwndHidden, WM_UPDATE_CHAT, 0,
                reinterpret_cast<LPARAM>(replyText));
}

// ============================================================
// Drawing
// ============================================================

void TrayWindow::DrawSendButton(HDC hdc)
{
    if (m_sendRect.right <= m_sendRect.left) return;

    COLORREF color = m_sendHover ? kSendHover : kSendIdle;

    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    RoundRect(hdc, m_sendRect.left, m_sendRect.top,
              m_sendRect.right, m_sendRect.bottom, 10, 10);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);

    HFONT hOldFont = nullptr;
    if (m_hChatFont)
        hOldFont = (HFONT)SelectObject(hdc, m_hChatFont);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextColor);
    RECT textRect = m_sendRect;
    DrawTextW(hdc, L"Kirim", -1, &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (hOldFont)
        SelectObject(hdc, hOldFont);
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
