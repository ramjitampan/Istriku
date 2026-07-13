#pragma once
#include "Command.h"
#include <string>
#include <string_view>
#include <vector>

namespace Yuki::System {

class IntentDetector
{
public:
    IntentDetector() = default;
    ~IntentDetector() = default;

    IntentDetector(const IntentDetector&) = delete;
    IntentDetector& operator=(const IntentDetector&) = delete;

    Command Detect(const std::string& message);

private:
    static std::string ToLower(const std::string& str);
    static bool Contains(const std::string& str, const std::string& keyword);
    static int ExtractNumber(const std::string& message);
    static int ParseIndonesianNumber(const std::string& message);

    Command DetectSystem(const std::string& msg);
    Command DetectApplications(const std::string& msg);
    Command DetectWeb(const std::string& msg);
    Command DetectScreenshot(const std::string& msg);
    Command DetectVolume(const std::string& msg);
    Command DetectBrightness(const std::string& msg);
    Command DetectWifi(const std::string& msg);
    Command DetectConfirmation(const std::string& msg);

    static std::vector<std::string> m_confirmationWords;
    static std::vector<std::string> m_rejectionWords;
};

} // namespace Yuki::System