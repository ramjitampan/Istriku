#include "WindowsController.h"
#include "../utils/Logger.h"
#include "../utils/Json.h"

#include <shellapi.h>
#include <powrprof.h>
#include <mmsystem.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace Yuki::System {

namespace fs = std::filesystem;

namespace {

// Minimal HTTPS GET via WinHTTP — dipakai untuk pencarian lagu (YouTube Data API).
std::string HttpGet(const std::string& url, long timeoutMs = 15000)
{
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Yuki/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession)
        return result;

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    wchar_t extraInfo[4096] = {};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = 2048;
    urlComp.lpszExtraInfo = extraInfo;
    urlComp.dwExtraInfoLength = 4096;

    std::wstring wurl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp))
    {
        WinHttpCloseHandle(hSession);
        return result;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    HINTERNET hConnect = WinHttpConnect(hSession, host,
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return result;
    }

    std::wstring requestPath(path);
    if (urlComp.dwExtraInfoLength > 0)
        requestPath.append(extraInfo, urlComp.dwExtraInfoLength);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", requestPath.c_str(),
        nullptr, nullptr, nullptr, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0)
        {
            std::vector<char> buf(available);
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf.data(), available, &read) || read == 0)
                break;
            result.append(buf.data(), read);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

std::string UrlEncode(const std::string& value)
{
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (unsigned char c : value)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            oss << c;
        else
            oss << '%' << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::string ExtractFirstVideoId(const std::string& jsonBody)
{
    static const std::string key = "\"videoId\":\"";
    size_t pos = jsonBody.find(key);
    if (pos == std::string::npos)
        return {};
    pos += key.size();
    size_t end = jsonBody.find('"', pos);
    if (end == std::string::npos)
        return {};
    return jsonBody.substr(pos, end - pos);
}

} // anonymous namespace

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
        case CommandType::OpenNotepad:
        case CommandType::OpenPowerShell:
        case CommandType::OpenCommandPrompt:
        case CommandType::OpenWhatsApp:
        case CommandType::OpenWord:
        case CommandType::OpenChrome:
        case CommandType::OpenSteam:
        case CommandType::OpenExplorer:
            return ExecuteOpenApplication(command);

        case CommandType::OpenYoutubeMusic:
        case CommandType::OpenGithub:
        case CommandType::OpenChromeSite:
            return ExecuteOpenWeb(command);

        case CommandType::SystemInfo:
            return ExecuteSystemInfo();

        case CommandType::DateTime:
            return ExecuteDateTime();

        case CommandType::WeatherHome:
        case CommandType::WeatherCampus:
            return ExecuteWeather(command);

        case CommandType::WeatherHelp:
            return {true, "Pilih lokasi: ucapkan 'cuaca rumah' atau 'cuaca kampus'.",
                    false, CommandType::WeatherHelp};

        case CommandType::Screenshot:
            return ExecuteScreenshot();

        case CommandType::PlaySong:
            return ExecutePlaySong(command);

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
        case CommandType::OpenNotepad:
        {
            HINSTANCE hResult = ShellExecuteA(nullptr, "open", "notepad.exe",
                nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(hResult) <= 32)
                return {false, "Gagal membuka Notepad.", false, CommandType::OpenNotepad};
            return {true, "Membuka Notepad.", false, CommandType::OpenNotepad};
        }
        case CommandType::OpenPowerShell: return OpenPowerShellApp();
        case CommandType::OpenCommandPrompt: return OpenCommandPromptApp();
        case CommandType::OpenWhatsApp:  return OpenWhatsAppApp();
        case CommandType::OpenWord:      return OpenWordApp();
        case CommandType::OpenChrome:    return OpenChromeHome();
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

CommandResult WindowsController::OpenPowerShellApp()
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", "powershell.exe",
        nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        return {false, "Gagal membuka PowerShell.", false, CommandType::OpenPowerShell};
    return {true, "Membuka PowerShell.", false, CommandType::OpenPowerShell};
}

CommandResult WindowsController::OpenCommandPromptApp()
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", "cmd.exe",
        nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        return {false, "Gagal membuka Command Prompt.", false, CommandType::OpenCommandPrompt};
    return {true, "Membuka Command Prompt.", false, CommandType::OpenCommandPrompt};
}

CommandResult WindowsController::OpenWhatsAppApp()
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", "whatsapp:",
        nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        const std::string appPath = GetLocalAppDataPath() + "\\WhatsApp\\WhatsApp.exe";
        if (!fs::exists(appPath))
            return {false, "WhatsApp tidak ditemukan atau protokol aplikasinya belum terdaftar.",
                    false, CommandType::OpenWhatsApp};
        result = ShellExecuteA(nullptr, "open", appPath.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);
    }

    if (reinterpret_cast<INT_PTR>(result) <= 32)
        return {false, "Gagal membuka WhatsApp.", false, CommandType::OpenWhatsApp};
    return {true, "Membuka WhatsApp.", false, CommandType::OpenWhatsApp};
}

CommandResult WindowsController::OpenWordApp()
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", "winword.exe",
        nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32)
        return {true, "Membuka Microsoft Word.", false, CommandType::OpenWord};

    const std::string pf = GetProgramFilesPath();
    char x86Buffer[MAX_PATH] = {};
    size_t x86Size = 0;
    getenv_s(&x86Size, x86Buffer, MAX_PATH, "ProgramFiles(x86)");
    const std::string pf86 = x86Size > 1 ? std::string(x86Buffer) : pf;
    const std::vector<std::string> paths = {
        pf + "\\Microsoft Office\\root\\Office16\\WINWORD.EXE",
        pf86 + "\\Microsoft Office\\root\\Office16\\WINWORD.EXE",
        pf + "\\Microsoft Office\\Office16\\WINWORD.EXE",
        pf86 + "\\Microsoft Office\\Office16\\WINWORD.EXE"
    };
    for (const auto& path : paths)
    {
        if (fs::exists(path))
        {
            result = ShellExecuteA(nullptr, "open", path.c_str(),
                nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) > 32)
                return {true, "Membuka Microsoft Word.", false, CommandType::OpenWord};
        }
    }
    return {false, "Microsoft Word tidak ditemukan.", false, CommandType::OpenWord};
}

CommandResult WindowsController::OpenChromeHome()
{
    return OpenChromeUrl("chrome://newtab/", "Membuka halaman utama Chrome.",
                         CommandType::OpenChrome);
}

// ===================== WEB COMMANDS =====================

CommandResult WindowsController::ExecuteOpenWeb(const Command& command)
{
    switch (command.type)
    {
        case CommandType::OpenYoutubeMusic:
            return OpenChromeUrl("https://music.youtube.com", "Membuka YouTube Music.", command.type);
        case CommandType::OpenGithub:
            return OpenChromeUrl("https://github.com", "Membuka GitHub.", command.type);
        case CommandType::OpenChromeSite:
        {
            if (command.query == "smile_unp")
                return OpenChromeUrl("https://smile.unp.ac.id/elearning/login/index.php",
                    "Membuka e-learning SMILE UNP di Chrome.", command.type);
            if (command.query == "chatgpt")
                return OpenChromeUrl("https://chatgpt.com", "Membuka ChatGPT di Chrome.", command.type);
            if (command.query == "facebook")
                return OpenChromeUrl("https://www.facebook.com", "Membuka Facebook di Chrome.", command.type);
            if (command.query == "tiktok")
                return OpenChromeUrl("https://www.tiktok.com", "Membuka TikTok di Chrome.", command.type);
            if (command.query == "instagram")
                return OpenChromeUrl("https://www.instagram.com", "Membuka Instagram di Chrome.", command.type);
            if (command.query == "youtube")
                return OpenChromeUrl("https://www.youtube.com", "Membuka YouTube di Chrome.", command.type);
            if (command.query == "claude")
                return OpenChromeUrl("https://claude.ai", "Membuka Claude di Chrome.", command.type);
            return {false, "Situs Chrome tidak dikenal.", false, CommandType::OpenChromeSite};
        }
        default:
            return {false, "Website tidak dikenal.", false, CommandType::Unknown};
    }
}

CommandResult WindowsController::OpenChromeUrl(const std::string& url,
    const std::string& successMessage, CommandType commandType)
{
    HINSTANCE result = ShellExecuteA(nullptr, "open", "chrome.exe",
        url.c_str(), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32)
        return {true, successMessage, false, commandType};

    char x86Buffer[MAX_PATH] = {};
    size_t x86Size = 0;
    getenv_s(&x86Size, x86Buffer, MAX_PATH, "ProgramFiles(x86)");
    const std::string pf86 = x86Size > 1 ? std::string(x86Buffer) : "C:\\Program Files (x86)";
    const std::vector<std::string> paths = {
        GetProgramFilesPath() + "\\Google\\Chrome\\Application\\chrome.exe",
        pf86 + "\\Google\\Chrome\\Application\\chrome.exe",
        GetLocalAppDataPath() + "\\Google\\Chrome\\Application\\chrome.exe"
    };
    for (const auto& path : paths)
    {
        if (fs::exists(path))
        {
            result = ShellExecuteA(nullptr, "open", path.c_str(),
                url.c_str(), nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) > 32)
                return {true, successMessage, false, commandType};
        }
    }
    return {false, "Google Chrome tidak ditemukan.", false, commandType};
}

CommandResult WindowsController::ExecuteSystemInfo() const
{
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);

    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    const bool hasMemory = GlobalMemoryStatusEx(&memoryStatus) != FALSE;

    char computerName[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD computerNameSize = sizeof(computerName);
    GetComputerNameA(computerName, &computerNameSize);

    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    using RtlGetVersionFn = LONG (WINAPI*)(OSVERSIONINFOW*);
    RtlGetVersionFn getVersion = nullptr;
    const FARPROC versionProcedure = ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : nullptr;
    static_assert(sizeof(getVersion) == sizeof(versionProcedure),
                  "Windows function pointer representations must match");
    std::memcpy(&getVersion, &versionProcedure, sizeof(getVersion));
    const bool hasVersion = getVersion && getVersion(&version) >= 0;

    std::string osName = "Windows";
    if (hasVersion && version.dwMajorVersion == 10)
        osName = version.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10";
    else if (hasVersion)
        osName += " NT " + std::to_string(version.dwMajorVersion) + "." +
                  std::to_string(version.dwMinorVersion);

    const char* architecture = "tidak diketahui";
    switch (systemInfo.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64: architecture = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: architecture = "ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: architecture = "x86"; break;
    }

    std::ostringstream report;
    report << "Nama perangkat: " << (computerName[0] ? computerName : "tidak tersedia")
           << "\nSistem operasi: " << osName;
    if (hasVersion)
        report << " (build " << version.dwBuildNumber << ")";
    report << "\nArsitektur: " << architecture
           << "\nProsesor logis: " << systemInfo.dwNumberOfProcessors;
    if (hasMemory)
        report << "\nRAM total: " << (memoryStatus.ullTotalPhys / (1024ULL * 1024ULL))
               << " MB\nRAM tersedia: "
               << (memoryStatus.ullAvailPhys / (1024ULL * 1024ULL)) << " MB";
    return {true, report.str(), false, CommandType::SystemInfo};
}

CommandResult WindowsController::ExecuteDateTime() const
{
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    std::ostringstream report;
    report << "Tanggal dan waktu lokal: " << std::setfill('0')
           << std::setw(2) << localTime.wDay << '/'
           << std::setw(2) << localTime.wMonth << '/'
           << localTime.wYear << ' ' << std::setw(2) << localTime.wHour << ':'
           << std::setw(2) << localTime.wMinute << ':'
           << std::setw(2) << localTime.wSecond;
    return {true, report.str(), false, CommandType::DateTime};
}

CommandResult WindowsController::ExecuteWeather(const Command& command) const
{
    const bool home = command.type == CommandType::WeatherHome;
    const char* location = home
        ? "Rumah (Kelurahan Nangka, Binjai Utara)"
        : "Kampus Universitas Negeri Padang, Padang";
    // Approximate coordinates: Nangka locality center and UNP main campus.
    const char* latitude = home ? "3.6192" : "-0.897455";
    const char* longitude = home ? "98.5045" : "100.350767";

    const std::string url =
        std::string("https://api.open-meteo.com/v1/forecast?latitude=") + latitude +
        "&longitude=" + longitude +
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m"
        "&timezone=Asia%2FJakarta";
    const std::string body = HttpGet(url, 10000);
    const auto json = Yuki::Utils::Json::Parse(body);
    const auto& root = json.Raw();
    if (!root.is_object() || !root.contains("current") || !root["current"].is_object())
    {
        Logger::Warning("Weather: Open-Meteo did not return current conditions.");
        return {false, "Data cuaca belum bisa diambil. Periksa koneksi internet lalu coba lagi.",
                false, command.type};
    }

    const auto& current = root["current"];
    auto number = [&](const char* key) -> std::optional<double>
    {
        const auto it = current.find(key);
        if (it == current.end() || !it->is_number())
            return std::nullopt;
        return it->get<double>();
    };

    const auto temperature = number("temperature_2m");
    const auto humidity = number("relative_humidity_2m");
    const auto feelsLike = number("apparent_temperature");
    const auto precipitation = number("precipitation");
    const auto wind = number("wind_speed_10m");
    const auto code = number("weather_code");
    const auto timeIt = current.find("time");

    if (!temperature || !humidity || !code)
        return {false, "Respons cuaca tidak lengkap. Coba lagi nanti.", false, command.type};

    const int weatherCode = static_cast<int>(*code);
    const char* condition = "Kondisi tidak diketahui";
    switch (weatherCode)
    {
        case 0: condition = "Cerah"; break;
        case 1: condition = "Cerah berawan"; break;
        case 2: condition = "Berawan sebagian"; break;
        case 3: condition = "Mendung"; break;
        case 45: case 48: condition = "Berkabut"; break;
        case 51: case 53: case 55: condition = "Gerimis"; break;
        case 56: case 57: condition = "Gerimis beku"; break;
        case 61: case 63: case 65: condition = "Hujan"; break;
        case 66: case 67: condition = "Hujan beku"; break;
        case 71: case 73: case 75: case 77: condition = "Salju"; break;
        case 80: case 81: case 82: condition = "Hujan lokal"; break;
        case 85: case 86: condition = "Hujan salju"; break;
        case 95: case 96: case 99: condition = "Badai petir"; break;
    }

    std::ostringstream report;
    report << "Cuaca saat ini — " << location;
    if (timeIt != current.end() && timeIt->is_string())
        report << " (" << timeIt->get<std::string>() << " WIB)";
    report << "\nKondisi: " << condition
           << "\nSuhu: " << std::fixed << std::setprecision(1) << *temperature << " °C"
           << "\nKelembapan: " << std::setprecision(0) << *humidity << "%";
    if (feelsLike)
        report << "\nTerasa seperti: " << std::setprecision(1) << *feelsLike << " °C";
    if (precipitation)
        report << "\nCurah hujan 1 jam terakhir: " << std::setprecision(1)
               << *precipitation << " mm";
    if (wind)
        report << "\nKecepatan angin: " << std::setprecision(1) << *wind << " km/jam";
    report << "\nSumber: Open-Meteo (data model cuaca).";
    return {true, report.str(), false, command.type};
}

CommandResult WindowsController::OpenUrlAction(const std::string& url, const std::string& successMessage)
{
    HINSTANCE hResult = ShellExecuteA(nullptr, "open", url.c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) <= 32)
    {
        Logger::Error("OpenUrlAction: ShellExecuteA failed for " + url);
        return {false, "Gagal membuka browser.", false, CommandType::Unknown};
    }

    return {true, successMessage, false, CommandType::Unknown};
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

// ===================== PLAY SONG (YouTube Music) =====================

CommandResult WindowsController::ExecutePlaySong(const Command& command)
{
    std::string query = command.query;
    if (query.empty())
        return {false, "Judul lagu kosong.", false, CommandType::PlaySong};

    std::string url;

    // Auto-play: cari videoId pertama lewat YouTube Data API (butuh key).
    if (!m_youtubeApiKey.empty())
    {
        std::string apiUrl =
            "https://www.googleapis.com/youtube/v3/search?part=id&type=video&maxResults=1&q="
            + UrlEncode(query) + "&key=" + UrlEncode(m_youtubeApiKey);

        std::string videoId = ExtractFirstVideoId(HttpGet(apiUrl));
        if (!videoId.empty())
            url = "https://music.youtube.com/watch?v=" + videoId;
    }

    // Fallback: tanpa key / gagal cari → buka halaman pencarian.
    if (url.empty())
        url = "https://music.youtube.com/search?q=" + UrlEncode(query);

    return OpenUrlAction(url, "Memutar: " + query);
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

int WindowsController::GetCurrentBrightnessPercent()
{
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 0))
        return -1;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::string commandLine =
        "powershell.exe -NoLogo -NoProfile -NonInteractive -Command \""
        "(Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness | "
        "Select-Object -First 1 -ExpandProperty CurrentBrightness)\"";

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    startupInfo.hStdOutput = writePipe;
    startupInfo.hStdError = writePipe;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
    CloseHandle(writePipe);
    if (!created)
    {
        CloseHandle(readPipe);
        return -1;
    }

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 5000);
    if (waitResult != WAIT_OBJECT_0)
    {
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(readPipe);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return -1;
    }

    std::string output;
    char buffer[128];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        if (output.size() < 512)
            output.append(buffer, std::min<std::size_t>(bytesRead, 512 - output.size()));
    }
    CloseHandle(readPipe);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    const auto firstDigit = output.find_first_of("0123456789");
    if (firstDigit == std::string::npos)
        return -1;
    try
    {
        const int value = std::stoi(output.substr(firstDigit));
        return value >= 0 && value <= 100 ? value : -1;
    }
    catch (const std::exception&)
    {
        return -1;
    }
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
