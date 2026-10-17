#pragma once
#include <string>
#include <string_view>

namespace Yuki::System {

enum class CommandType
{
    Unknown,
    Shutdown,
    Restart,
    Sleep,
    Lock,
    Logout,
    OpenVSCode,
    OpenNotepad,
    OpenPowerShell,
    OpenCommandPrompt,
    OpenWhatsApp,
    OpenWord,
    OpenChrome,
    OpenChromeSite,
    OpenSteam,
    OpenExplorer,
    OpenYoutubeMusic,
    OpenGithub,
    SystemInfo,
    DateTime,
    WeatherHome,
    WeatherCampus,
    WeatherHelp,
    Screenshot,
    PlaySong,
    Volume,
    Brightness,
    WifiOn,
    WifiOff
};

inline std::string_view CommandTypeToString(CommandType type) noexcept
{
    switch (type)
    {
        case CommandType::Unknown: return "Unknown";
        case CommandType::Shutdown: return "Shutdown";
        case CommandType::Restart: return "Restart";
        case CommandType::Sleep: return "Sleep";
        case CommandType::Lock: return "Lock";
        case CommandType::Logout: return "Logout";
        case CommandType::OpenVSCode: return "OpenVSCode";
        case CommandType::OpenNotepad: return "OpenNotepad";
        case CommandType::OpenPowerShell: return "OpenPowerShell";
        case CommandType::OpenCommandPrompt: return "OpenCommandPrompt";
        case CommandType::OpenWhatsApp: return "OpenWhatsApp";
        case CommandType::OpenWord: return "OpenWord";
        case CommandType::OpenChrome: return "OpenChrome";
        case CommandType::OpenChromeSite: return "OpenChromeSite";
        case CommandType::OpenSteam: return "OpenSteam";
        case CommandType::OpenExplorer: return "OpenExplorer";
        case CommandType::OpenYoutubeMusic: return "OpenYoutubeMusic";
        case CommandType::OpenGithub: return "OpenGithub";
        case CommandType::SystemInfo: return "SystemInfo";
        case CommandType::DateTime: return "DateTime";
        case CommandType::WeatherHome: return "WeatherHome";
        case CommandType::WeatherCampus: return "WeatherCampus";
        case CommandType::WeatherHelp: return "WeatherHelp";
        case CommandType::Screenshot: return "Screenshot";
        case CommandType::PlaySong: return "PlaySong";
        case CommandType::Volume: return "Volume";
        case CommandType::Brightness: return "Brightness";
        case CommandType::WifiOn: return "WifiOn";
        case CommandType::WifiOff: return "WifiOff";
    }
    return "Unknown";
}

struct Command
{
    CommandType type = CommandType::Unknown;
    int parameter = -1;
    std::string query;
    bool requiresConfirmation = false;
};

struct CommandResult
{
    bool success = false;
    std::string message;
    bool confirmationRequired = false;
    CommandType commandType = CommandType::Unknown;
};

} // namespace Yuki::System
