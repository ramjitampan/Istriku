#pragma once
#include "Command.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// COM audio headers (harus sebelum gdiplus fix biar PROPID gak conflict)
#include <mmdeviceapi.h>
#include <endpointvolume.h>

// MinGW gdiplus fix: define PROPID before including gdiplus
#ifndef PROPID
#define PROPID long
#endif
#include <gdiplus.h>

#include <string>

namespace Yuki::System {

class WindowsController
{
public:
    WindowsController() = default;
    ~WindowsController() = default;

    WindowsController(const WindowsController&) = delete;
    WindowsController& operator=(const WindowsController&) = delete;
    WindowsController(WindowsController&&) noexcept = default;
    WindowsController& operator=(WindowsController&&) noexcept = default;

    CommandResult Execute(const Command& command);
    int GetCurrentVolumePercent();

private:
    CommandResult ExecuteSystemCommand(const Command& command);
    CommandResult ExecuteOpenApplication(const Command& command);
    CommandResult ExecuteOpenWeb(const Command& command);
    CommandResult ExecuteScreenshot();
    CommandResult ExecuteVolume(const Command& command);
    CommandResult ExecuteBrightness(const Command& command);
    CommandResult ExecuteWifi(const Command& command);

    CommandResult SystemShutdown();
    CommandResult SystemRestart();
    CommandResult SystemSleep();
    CommandResult SystemLock();
    CommandResult SystemLogOut();

    CommandResult OpenVSCodeApp();
    CommandResult OpenSteamApp();
    CommandResult OpenExplorerApp();

    CommandResult OpenUrlAction(const std::string& url);

    static bool SaveBitmapToFile(HBITMAP hBitmap, const std::string& filePath);
    static void FillGammaRamp(int percent, WORD ramp[256*3]);

    static CommandResult SetBrightnessWmi(int percent);
    static CommandResult SetBrightnessGammaRamp(int percent);
    static CommandResult SetBrightnessGamma(int percent);

    static std::string GetProgramFilesPath();
    static std::string GetLocalAppDataPath();
    static bool IsLaptop();
};

} // namespace Yuki::System