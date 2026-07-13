#include "WindowsController.h"
#include "../utils/Logger.h"

#include <shellapi.h>
#include <powrprof.h>
#include <mmsystem.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Yuki::System {

namespace fs = std::filesystem;

// MinGW fix: define CLSID/IID lokal biar gak perlu link uuid.lib
static const GUID CLSID_MMDeviceEnumerator_Local =
    {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID IID_IMMDeviceEnumerator_Local =
    {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID IID_IAudioEndpointVolume_Local =
    {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};

CommandResult WindowsController::Execute(const Command& command)
{
    switch (command.type)
    {
        case CommandType::Shutdown:
        case CommandType::Restart:
        case CommandType::Sleep:
        case CommandType::Lock:
        case CommandType::Logout:
            return ExecuteSystemCommand(command);

        case CommandType::OpenVSCode:
        case CommandType::OpenSteam:
        case CommandType::OpenExplorer:
            return ExecuteOpenApplication(command);

        case CommandType::OpenYoutubeMusic:
        case CommandType::OpenGithub:
            return ExecuteOpenWeb(command);

        case CommandType::Screenshot:
            return ExecuteScreenshot();

        case CommandType::Volume:
            return ExecuteVolume(command);

        case CommandType::Brightness:
            return ExecuteBrightness(command);

        case CommandType::WifiOn:
        case CommandType::WifiOff:
            return ExecuteWifi(command);

        default:
            return {false, "Perintah tidak dikenal.", false, CommandType::Unknown};
    }
}

// ===================== SYSTEM COMMANDS =====================

CommandResult WindowsController::ExecuteSystemCommand(const Command& command)
{
    if (command.requiresConfirmation)
    {
        CommandResult result;
        result.confirmationRequired = true;
        result.commandType = command.type;
        result.success = false;

        switch (command.type)
        {
            case CommandType::Shutdown:
                result.message = "Apakah kamu yakin ingin mematikan laptop?";
                break;
            case CommandType::Restart:
                result.message = "Apakah kamu yakin ingin merestart laptop?";
                break;
            case CommandType::Sleep:
                result.message = "Apakah kamu yakin ingin sleep laptop?";
                break;
            default:
                result.message = "Apakah kamu yakin?";
                break;
        }

        return result;
    }

    switch (command.type)
    {
        case CommandType::Shutdown: return SystemShutdown();
        case CommandType::Restart:  return SystemRestart();
        case CommandType::Sleep:    return SystemSleep();
        case CommandType::Lock:     return SystemLock();
        case CommandType::Logout:   return SystemLogOut();
        default:
            return {false, "Perintah sistem tidak dikenal.", false, CommandType::Unknown};
    }
}

static bool EnableShutdownPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        return false;
    }

    LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME,
        &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
        static_cast<PTOKEN_PRIVILEGES>(nullptr), nullptr);

    DWORD err = GetLastError();
    CloseHandle(hToken);

    return result && (err == ERROR_SUCCESS);
}

CommandResult WindowsController::SystemShutdown()
{
    if (!EnableShutdownPrivilege())
    {
        DWORD err = GetLastError();
        Logger::Error("SystemShutdown: Gagal enable privilege. Err=" + std::to_string(err));
        return {false, "Gagal mendapatkan hak akses shutdown.", false, CommandType::Shutdown};
    }

    if (!InitiateShutdownW(nullptr, nullptr, 0, 0,
            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
    {
        // Fallback ke ExitWindowsEx
        if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG,
            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
        {
            DWORD err = GetLastError();
            Logger::Error("SystemShutdown: Gagal. Err=" + std::to_string(err));
            return {false, "Gagal mematikan laptop.", false, CommandType::Shutdown};
        }
    }

    return {true, "Laptop akan dimatikan.", false, CommandType::Shutdown};
}

CommandResult WindowsController::SystemRestart()
{
    if (!EnableShutdownPrivilege())
    {
        DWORD err = GetLastError();
        Logger::Error("SystemRestart: Gagal enable privilege. Err=" + std::to_string(err));
        return {false, "Gagal mendapatkan hak akses restart.", false, CommandType::Restart};
    }

    if (!InitiateShutdownW(nullptr, nullptr, 0, SHUTDOWN_RESTART,
            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
    {
        // Fallback ke ExitWindowsEx
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
        {
            DWORD err = GetLastError();
            Logger::Error("SystemRestart: Gagal. Err=" + std::to_string(err));
            return {false, "Gagal merestart laptop.", false, CommandType::Restart};
        }
    }

    return {true, "Laptop akan direstart.", false, CommandType::Restart};
}

CommandResult WindowsController::SystemSleep()
{
    if (!EnableShutdownPrivilege())
    {
        DWORD err = GetLastError();
        Logger::Warning("SystemSleep: Gagal enable privilege. Err=" + std::to_string(err));
        // Tetap lanjut, mungkin gak butuh privilege
    }

    if (!SetSuspendState(FALSE, FALSE, FALSE))
    {
        DWORD err = GetLastError();
        Logger::Error("SystemSleep: SetSuspendState failed. Err=" + std::to_string(err));
        return {false, "Gagal sleep laptop.", false, CommandType::Sleep};
    }

    return {true, "Laptop akan sleep.", false, CommandType::Sleep};
}

CommandResult WindowsController::SystemLock()
{
    if (!LockWorkStation())
    {
        DWORD err = GetLastError();
        Logger::Error("SystemLock: LockWorkStation failed. Err=" + std::to_string(err));
        return {false, "Gagal mengunci laptop.", false, CommandType::Lock};
    }

    return {true, "Laptop terkunci.", false, CommandType::Lock};
}

CommandResult WindowsController::SystemLogOut()
{
    if (!ExitWindowsEx(EWX_LOGOFF | EWX_FORCEIFHUNG, 0))
    {
        DWORD err = GetLastError();
        Logger::Error("SystemLogOut: ExitWindowsEx failed. Err=" + std::to_string(err));
        return {false, "Gagal logout.", false, CommandType::Logout};
    }

    return {true, "Logout berhasil.", false, CommandType::Logout};
}

// ===================== APPLICATION COMMANDS =====================

CommandResult WindowsController::ExecuteOpenApplication(const Command& command)
{
    switch (command.type)
    {
        case CommandType::OpenVSCode:        return OpenVSCodeApp();
        case CommandType::OpenSteam:         return OpenSteamApp();
        case CommandType::OpenExplorer:      return OpenExplorerApp();
        default:
            return {false, "Aplikasi tidak dikenal.", false, CommandType::Unknown};
    }
}

CommandResult WindowsController::OpenVSCodeApp()
{
    // Try via PATH first (most reliable if installed)
    HINSTANCE result = ShellExecuteA(nullptr, "open", "code",
        nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) > 32)
        return {true, "Membuka VS Code.", false, CommandType::OpenVSCode};

    // Fallback: try common install paths
    std::vector<std::string> paths = {
        GetLocalAppDataPath() + "\\Programs\\Microsoft VS Code\\Code.exe",
        GetProgramFilesPath() + "\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files\\Microsoft VS Code\\Code.exe",
        "C:\\Program Files (x86)\\Microsoft VS Code\\Code.exe"
    };

    for (const auto& path : paths)
    {
        if (fs::exists(path))
        {
            result = ShellExecuteA(nullptr, "open", path.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL);

            if (reinterpret_cast<INT_PTR>(result) > 32)
                return {true, "Membuka VS Code.", false, CommandType::OpenVSCode};
        }
    }

    Logger::Warning("VS Code tidak ditemukan.");
    return {false, "VS Code tidak ditemukan.", false, CommandType::OpenVSCode};
}

CommandResult WindowsController::OpenSteamApp()
{
    std::vector<std::string> paths = {
        GetProgramFilesPath() + "\\Steam\\steam.exe",
        "C:\\Program Files\\Steam\\steam.exe",
        "C:\\Program Files (x86)\\Steam\\steam.exe"
    };

    for (const auto& path : paths)
    {
        if (fs::exists(path))
        {
            HINSTANCE hResult = ShellExecuteA(nullptr, "open", path.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL);

            if (reinterpret_cast<INT_PTR>(hResult) > 32)
                return {true, "Membuka Steam.", false, CommandType::OpenSteam};
        }
    }

    Logger::Warning("Steam tidak ditemukan.");
    return {false, "Steam tidak ditemukan.", false, CommandType::OpenSteam};
}

CommandResult WindowsController::OpenExplorerApp()
{
    HINSTANCE hResult = ShellExecuteA(nullptr, "open", "explorer.exe",
        nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) <= 32)
    {
        Logger::Error("OpenExplorerApp: ShellExecuteA failed.");
        return {false, "Gagal membuka File Explorer.", false, CommandType::OpenExplorer};
    }

    return {true, "Membuka File Explorer.", false, CommandType::OpenExplorer};
}

// ===================== WEB COMMANDS =====================

CommandResult WindowsController::ExecuteOpenWeb(const Command& command)
{
    switch (command.type)
    {
        case CommandType::OpenYoutubeMusic: return OpenUrlAction("https://music.youtube.com");
        case CommandType::OpenGithub:       return OpenUrlAction("https://github.com");
        default:
            return {false, "Website tidak dikenal.", false, CommandType::Unknown};
    }
}

CommandResult WindowsController::OpenUrlAction(const std::string& url)
{
    HINSTANCE hResult = ShellExecuteA(nullptr, "open", url.c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) <= 32)
    {
        Logger::Error("OpenUrlAction: ShellExecuteA failed for " + url);
        return {false, "Gagal membuka browser.", false, CommandType::Unknown};
    }

    return {true, "Membuka " + url, false, CommandType::Unknown};
}

// ===================== SCREENSHOT =====================

CommandResult WindowsController::ExecuteScreenshot()
{
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    int width = GetDeviceCaps(hdcScreen, HORZRES);
    int height = GetDeviceCaps(hdcScreen, VERTRES);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    char userProfile[MAX_PATH];
    size_t size;
    getenv_s(&size, userProfile, MAX_PATH, "USERPROFILE");

    fs::path picturesPath = fs::path(std::string(userProfile)) / "Pictures" / "Screenshots";

    if (!fs::exists(picturesPath))
        fs::create_directories(picturesPath);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);

    std::ostringstream fileName;
    fileName << "Screenshot_"
             << std::put_time(&tm, "%Y_%m_%d_%H_%M_%S")
             << ".png";

    std::string fullPath = (picturesPath / fileName.str()).string();

    if (!SaveBitmapToFile(hBitmap, fullPath))
    {
        DeleteObject(hBitmap);
        Logger::Error("Screenshot: Gagal menyimpan bitmap.");
        return {false, "Gagal menyimpan screenshot.", false, CommandType::Screenshot};
    }

    DeleteObject(hBitmap);
    return {true, "Screenshot tersimpan di " + fullPath, false, CommandType::Screenshot};
}

bool WindowsController::SaveBitmapToFile(HBITMAP hBitmap, const std::string& filePath)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        return false;

    bool result = false;
    {
        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hBitmap, nullptr);
        if (bitmap)
        {
            std::wstring wpath(filePath.begin(), filePath.end());
            CLSID clsid = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};

            if (bitmap->Save(wpath.c_str(), &clsid, nullptr) == Gdiplus::Ok)
                result = true;

            delete bitmap;
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return result;
}

// ===================== VOLUME =====================

int WindowsController::GetCurrentVolumePercent()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return -1;

    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioEndpointVolume* pVol = nullptr;
    int percent = -1;

    if (SUCCEEDED(CoCreateInstance(CLSID_MMDeviceEnumerator_Local, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator_Local, reinterpret_cast<void**>(&pEnum))) &&
        SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) &&
        SUCCEEDED(pDevice->Activate(IID_IAudioEndpointVolume_Local, CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(&pVol))))
    {
        float current = 0.0f;
        if (SUCCEEDED(pVol->GetMasterVolumeLevelScalar(&current)))
            percent = static_cast<int>(std::round(current * 100.0f));
    }

    if (pVol) pVol->Release();
    if (pDevice) pDevice->Release();
    if (pEnum) pEnum->Release();
    if (SUCCEEDED(hr)) CoUninitialize();

    return percent;
}

CommandResult WindowsController::ExecuteVolume(const Command& command)
{
    int volumeLevel = command.parameter;

    if (volumeLevel < 0 || volumeLevel > 100)
        return {false, "Volume harus antara 0-100.", false, CommandType::Volume};

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return {false, "Gagal mengakses audio endpoint.", false, CommandType::Volume};

    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioEndpointVolume* pVol = nullptr;

    if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator_Local, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator_Local, reinterpret_cast<void**>(&pEnum))) ||
        FAILED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) ||
        FAILED(pDevice->Activate(IID_IAudioEndpointVolume_Local, CLSCTX_ALL,
            nullptr, reinterpret_cast<void**>(&pVol))))
    {
        if (pEnum) pEnum->Release();
        if (pDevice) pDevice->Release();
        if (SUCCEEDED(hr)) CoUninitialize();
        return {false, "Gagal mengakses audio endpoint.", false, CommandType::Volume};
    }

    float volumeScalar = volumeLevel / 100.0f;
    pVol->SetMasterVolumeLevelScalar(volumeScalar, nullptr);

    pVol->Release();
    pDevice->Release();
    pEnum->Release();
    if (SUCCEEDED(hr)) CoUninitialize();

    std::string message = (volumeLevel == 0)
        ? "Volume dimute."
        : "Volume diatur ke " + std::to_string(volumeLevel) + "%.";

    return {true, message, false, CommandType::Volume};
}

// ===================== BRIGHTNESS =====================

CommandResult WindowsController::ExecuteBrightness(const Command& command)
{
    int brightnessLevel = command.parameter;

    if (brightnessLevel < 0 || brightnessLevel > 100)
        return {false, "Kecerahan harus antara 0-100.", false, CommandType::Brightness};

    // Strategy 1: WMI (works on laptops)
    auto result = SetBrightnessWmi(brightnessLevel);
    if (result.success)
        return result;

    // Strategy 2: Gamma ramp (works on some desktop monitors)
    result = SetBrightnessGammaRamp(brightnessLevel);
    if (result.success)
        return result;

    // Both failed
    Logger::Warning("Brightness: Semua metode gagal.");
    return {false,
        "Gagal mengatur kecerahan. Monitor atau driver mungkin tidak mendukung.",
        false, CommandType::Brightness};
}

CommandResult WindowsController::SetBrightnessWmi(int percent)
{
    // Try WMI via wmic command (works on most laptops without extra setup)
    try
    {
        std::string cmd = "powershell -Command \"(Get-WmiObject -Namespace 'root/WMI' -Class 'WmiMonitorBrightnessMethods').WmiSetBrightness(1," + std::to_string(percent) + ")\" 2>nul";
        auto exitCode = std::system(cmd.c_str());

        // Also try the alternative WMI class
        if (exitCode != 0)
        {
            cmd = "powershell -Command \"(Get-WmiObject -Namespace 'root/WMI' -Class 'WmiMonitorBrightness' | Where-Object { $_.InstanceName -match 'Monitor' }).WmiSetBrightness(1," + std::to_string(percent) + ")\" 2>nul";
            exitCode = std::system(cmd.c_str());
        }

        if (exitCode == 0)
        {
            Logger::Info("Brightness: WMI berhasil, diatur ke " + std::to_string(percent) + "%");
            return {true, "Kecerahan diatur ke " + std::to_string(percent) + "%.", false, CommandType::Brightness};
        }
    }
    catch (...)
    {
        Logger::Warning("Brightness: WMI exception, fallback ke gamma ramp.");
    }

    return {false, "WMI brightness gagal.", false, CommandType::Brightness};
}

CommandResult WindowsController::SetBrightnessGammaRamp(int percent)
{
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return {false, "Gagal mendapatkan device context.", false, CommandType::Brightness};

    WORD gammaRamp[256 * 3];
    FillGammaRamp(percent, gammaRamp);

    bool success = SetDeviceGammaRamp(hdc, gammaRamp) != FALSE;
    ReleaseDC(nullptr, hdc);

    if (success)
    {
        Logger::Info("Brightness: GammaRamp berhasil, diatur ke " + std::to_string(percent) + "%");
        return {true, "Kecerahan diatur ke " + std::to_string(percent) + "%.", false, CommandType::Brightness};
    }

    Logger::Warning("Brightness: SetDeviceGammaRamp gagal.");
    return {false, "Gamma ramp gagal.", false, CommandType::Brightness};
}

void WindowsController::FillGammaRamp(int percent, WORD ramp[256*3])
{
    double factor = percent / 100.0;

    for (int i = 0; i < 256; ++i)
    {
        WORD value = static_cast<WORD>(i * factor);
        ramp[i]       = value;
        ramp[i + 256] = value;
        ramp[i + 512] = value;
    }
}

bool WindowsController::IsLaptop()
{
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps))
    {
        // If battery is present, it's likely a laptop
        if (sps.BatteryFlag != 255)
            return true;
    }
    return false;
}

// ===================== WIFI =====================

CommandResult WindowsController::ExecuteWifi(const Command& command)
{
    bool turnOn = (command.type == CommandType::WifiOn);
    std::string verb = turnOn ? "mengaktifkan" : "menonaktifkan";

    HINSTANCE hResult = ShellExecuteA(nullptr, "open",
        "ms-settings:network-wifi", nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) > 32)
    {
        return {true, "Membuka pengaturan WiFi. Silakan " + verb + " secara manual.", false, command.type};
    }

    Logger::Error("Wifi: ShellExecuteA gagal.");
    return {false, "Gagal " + verb + " WiFi. Adapter WiFi mungkin tidak tersedia.", false, command.type};
}

// ===================== HELPERS =====================

std::string WindowsController::GetProgramFilesPath()
{
    char buffer[MAX_PATH];
    size_t size;
    getenv_s(&size, buffer, MAX_PATH, "ProgramFiles");
    return std::string(buffer);
}

std::string WindowsController::GetLocalAppDataPath()
{
    char buffer[MAX_PATH];
    size_t size;
    getenv_s(&size, buffer, MAX_PATH, "LOCALAPPDATA");
    return std::string(buffer);
}

} // namespace Yuki::System