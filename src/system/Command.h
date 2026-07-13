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
    OpenSteam,
    OpenExplorer,
    OpenYoutubeMusic,
    OpenGithub,
    Screenshot,
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
        case CommandType::OpenSteam: return "OpenSteam";
        case CommandType::OpenExplorer: return "OpenExplorer";
        case CommandType::OpenYoutubeMusic: return "OpenYoutubeMusic";
        case CommandType::OpenGithub: return "OpenGithub";
        case CommandType::Screenshot: return "Screenshot";
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
