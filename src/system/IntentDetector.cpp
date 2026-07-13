#include "IntentDetector.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <vector>

namespace Yuki::System {

std::vector<std::string> IntentDetector::m_confirmationWords = {
    "ya", "iya", "yakin", "lanjutkan", "ya betul", "yes", "y", "oke",
    "ok", "setuju", "benar", "tentu", "gas"
};

std::vector<std::string> IntentDetector::m_rejectionWords = {
    "tidak", "nggak", "kaga", "gak", "batalkan", "batal", "jangan",
    "no", "n", "cancel", "ga jadi", "enggak"
};

Command IntentDetector::Detect(const std::string& message)
{
    std::string msg = ToLower(message);

    Command result = DetectConfirmation(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectSystem(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectApplications(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectWeb(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectScreenshot(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectVolume(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectBrightness(msg);
    if (result.type != CommandType::Unknown)
        return result;

    result = DetectWifi(msg);
    if (result.type != CommandType::Unknown)
        return result;

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectConfirmation(const std::string& msg)
{
    for (const auto& word : m_confirmationWords)
    {
        if (msg == word || msg.find(word) != std::string::npos)
        {
            return {CommandType::Unknown, -1, false};
        }
    }

    for (const auto& word : m_rejectionWords)
    {
        if (msg.find(word) != std::string::npos)
        {
            return {CommandType::Unknown, -1, false};
        }
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectSystem(const std::string& msg)
{
    // "tidur yang nyenyak sayang" → Shutdown (checked before "tidur" → Sleep)
    if ((Contains(msg, "nyenyak") && Contains(msg, "tidur")) ||
        (Contains(msg, "tidur") && Contains(msg, "sayang")))
    {
        return {CommandType::Shutdown, -1, true};
    }

    if (Contains(msg, "matikan laptop") ||
        Contains(msg, "matikan komputer") ||
        Contains(msg, "shutdown") ||
        Contains(msg, "shut down") ||
        (Contains(msg, "matikan") && Contains(msg, "laptop")))
    {
        return {CommandType::Shutdown, -1, true};
    }

    if (Contains(msg, "restart") ||
        Contains(msg, "reboot") ||
        (Contains(msg, "mulai") && Contains(msg, "ulang")) ||
        Contains(msg, "restart ulang"))
    {
        return {CommandType::Restart, -1, true};
    }

    if (Contains(msg, "sleep") ||
        Contains(msg, "tidur") ||
        Contains(msg, "mode tidur"))
    {
        return {CommandType::Sleep, -1, true};
    }

    if (Contains(msg, "kunci laptop") ||
        Contains(msg, "lock") ||
        Contains(msg, "kunci komputer") ||
        Contains(msg, "kuncikan") ||
        Contains(msg, "kunci"))
    {
        return {CommandType::Lock, -1, false};
    }

    if (Contains(msg, "log out") ||
        Contains(msg, "logout") ||
        Contains(msg, "keluar") ||
        Contains(msg, "sign out"))
    {
        return {CommandType::Logout, -1, false};
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectApplications(const std::string& msg)
{
    if (Contains(msg, "buka vscode") ||
        Contains(msg, "buka visual studio code") ||
        Contains(msg, "vscode") ||
        Contains(msg, "code"))
    {
        return {CommandType::OpenVSCode, -1, false};
    }

    if (Contains(msg, "buka steam") ||
        Contains(msg, "steam"))
    {
        return {CommandType::OpenSteam, -1, false};
    }

    if (Contains(msg, "buka folder") ||
        Contains(msg, "buka file explorer") ||
        Contains(msg, "explorer") ||
        Contains(msg, "buka direktori"))
    {
        return {CommandType::OpenExplorer, -1, false};
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectWeb(const std::string& msg)
{
    if (Contains(msg, "buka youtube music") ||
        Contains(msg, "youtube music") ||
        Contains(msg, "yt music"))
    {
        return {CommandType::OpenYoutubeMusic, -1, false};
    }

    // Looser match: "buka"/"bukakan" + any of youtube/yutub/music/musik
    // Menangkap varian seperti "buka youtube", "bukakan youtube",
    // "bukakan untuk music" (whisper misrecognizes "youtube" as "untuk")
    if ((Contains(msg, "buka") || Contains(msg, "bukakan")) &&
        (Contains(msg, "youtube") || Contains(msg, "yutub") ||
         Contains(msg, "music") || Contains(msg, "musik")))
    {
        return {CommandType::OpenYoutubeMusic, -1, false};
    }

    if (Contains(msg, "buka github") ||
        Contains(msg, "github"))
    {
        return {CommandType::OpenGithub, -1, false};
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectScreenshot(const std::string& msg)
{
    if (Contains(msg, "screenshot") ||
        Contains(msg, "screen shot") ||
        Contains(msg, "tangkap layar") ||
        msg == "ss")
    {
        return {CommandType::Screenshot, -1, false};
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectVolume(const std::string& msg)
{
    if (Contains(msg, "mute"))
    {
        return {CommandType::Volume, 0, false};
    }

    if (Contains(msg, "unmute") ||
        (Contains(msg, "buka") && Contains(msg, "mute")) ||
        Contains(msg, "hidupkan suara"))
    {
        return {CommandType::Volume, 50, false};
    }

    if (Contains(msg, "volume naik") ||
        Contains(msg, "naikkan volume") ||
        Contains(msg, "volume up") ||
        Contains(msg, "volume besar") ||
        (Contains(msg, "volume") && Contains(msg, "naik")))
    {
        return {CommandType::Volume, -1, false};
    }

    if (Contains(msg, "volume turun") ||
        Contains(msg, "turunkan volume") ||
        Contains(msg, "volume down") ||
        Contains(msg, "volume kecil") ||
        (Contains(msg, "volume") && Contains(msg, "turun")))
    {
        return {CommandType::Volume, -2, false};
    }

    int number = ParseIndonesianNumber(msg);
    if (number >= 0 && number <= 100)
    {
        if (Contains(msg, "volume") || Contains(msg, "vol"))
        {
            return {CommandType::Volume, number, false};
        }
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectBrightness(const std::string& msg)
{
    if (msg == "terang sekali" ||
        msg == "brightness penuh" ||
        msg == "brightness maksimum" ||
        msg == "layar terang sekali")
    {
        return {CommandType::Brightness, 100, false};
    }

    if (Contains(msg, "terangkan sedikit") ||
        Contains(msg, "terang dikit"))
    {
        return {CommandType::Brightness, -1, false};
    }

    // Check for number FIRST — jika ada angka + keyword brightness, pake absolut
    int number = ParseIndonesianNumber(msg);
    if (number >= 0 && number <= 100)
    {
        if (Contains(msg, "brightness") ||
            Contains(msg, "kecerahan") ||
            Contains(msg, "terang") ||
            Contains(msg, "cahaya") ||
            Contains(msg, "layar") ||
            Contains(msg, "brig") ||
            Contains(msg, "persen"))
        {
            return {CommandType::Brightness, number, false};
        }
    }

    // Brightness UP variants
    if (Contains(msg, "naikkan brightness") ||
        Contains(msg, "naik brightness") ||
        Contains(msg, "naikin brightness") ||
        Contains(msg, "brightness naik") ||
        Contains(msg, "brightness up") ||
        Contains(msg, "terangkan") ||
        Contains(msg, "terangi") ||
        Contains(msg, "lebih terang") ||
        Contains(msg, "layar terang") ||
        Contains(msg, "terangin") ||
        (Contains(msg, "brightness") && Contains(msg, "naik")))
    {
        return {CommandType::Brightness, -2, false};
    }

    // Brightness DOWN variants
    if (Contains(msg, "turunkan brightness") ||
        Contains(msg, "turun brightness") ||
        Contains(msg, "turunin brightness") ||
        Contains(msg, "brightness turun") ||
        Contains(msg, "brightness down") ||
        Contains(msg, "redupkan") ||
        Contains(msg, "redup") ||
        Contains(msg, "gelapkan") ||
        Contains(msg, "gelap") ||
        Contains(msg, "lebih redup") ||
        Contains(msg, "brightness kecil") ||
        Contains(msg, "layar redup") ||
        Contains(msg, "redup") ||
        (Contains(msg, "brightness") && Contains(msg, "turun")))
    {
        return {CommandType::Brightness, -3, false};
    }

    return {CommandType::Unknown, -1, false};
}

Command IntentDetector::DetectWifi(const std::string& msg)
{
    if (Contains(msg, "wifi hidupkan") ||
        Contains(msg, "hidupkan wifi") ||
        Contains(msg, "nyalakan wifi") ||
        Contains(msg, "aktifkan wifi") ||
        Contains(msg, "wifi on") ||
        (Contains(msg, "wifi") && (Contains(msg, "hidup") || Contains(msg, "nyala"))))
    {
        return {CommandType::WifiOn, -1, false};
    }

    if (Contains(msg, "wifi mati") ||
        Contains(msg, "matikan wifi") ||
        Contains(msg, "nonaktifkan wifi") ||
        Contains(msg, "wifi off") ||
        (Contains(msg, "wifi") && Contains(msg, "mati")))
    {
        return {CommandType::WifiOff, -1, false};
    }

    return {CommandType::Unknown, -1, false};
}

// ===================== HELPERS =====================

std::string IntentDetector::ToLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool IntentDetector::Contains(const std::string& str, const std::string& keyword)
{
    return str.find(keyword) != std::string::npos;
}

int IntentDetector::ExtractNumber(const std::string& message)
{
    std::regex pattern("(\\d+)");
    std::smatch match;

    if (std::regex_search(message, match, pattern))
    {
        return std::stoi(match[1].str());
    }

    return -1;
}

int IntentDetector::ParseIndonesianNumber(const std::string& message)
{
    std::string msg = ToLower(message);

    // Try standard digit extraction first (e.g., "50", "70")
    std::regex digitPattern("(\\d+)");
    std::smatch match;
    if (std::regex_search(msg, match, digitPattern))
    {
        return std::stoi(match[1].str());
    }

    // "seratus" → 100
    if (msg.find("seratus") != std::string::npos)
        return 100;

    // "nol" → 0
    if (msg.find("nol") != std::string::npos)
        return 0;

    // "sepuluh" → 10
    if (msg.find("sepuluh") != std::string::npos)
        return 10;

    // "sebelas" → 11
    if (msg.find("sebelas") != std::string::npos)
        return 11;

    // Common Whisper misrecognition: "elima puluh" → 50
    if (msg.find("elima puluh") != std::string::npos)
        return 50;

    // Check "XX belas" pattern (12-19)
    static const char* digitNames[] = {
        "", "satu", "dua", "tiga", "empat", "lima", "enam", "tujuh", "delapan", "sembilan"
    };

    for (int i = 2; i <= 9; i++)
    {
        std::string pattern = std::string(digitNames[i]) + " belas";
        if (msg.find(pattern) != std::string::npos)
            return 10 + i;
    }

    // Check "XX puluh" pattern (20-90)
    for (int i = 2; i <= 9; i++)
    {
        std::string pattern = std::string(digitNames[i]) + " puluh";
        if (msg.find(pattern) != std::string::npos)
            return i * 10;
    }

    // Single digit words (checked after compounds to avoid partial match)
    if (msg.find("satu") != std::string::npos) return 1;
    if (msg.find("dua") != std::string::npos) return 2;
    if (msg.find("tiga") != std::string::npos) return 3;
    if (msg.find("empat") != std::string::npos) return 4;
    if (msg.find("lima") != std::string::npos) return 5;
    if (msg.find("enam") != std::string::npos) return 6;
    if (msg.find("tujuh") != std::string::npos) return 7;
    if (msg.find("delapan") != std::string::npos) return 8;
    if (msg.find("sembilan") != std::string::npos) return 9;

    return -1;
}

} // namespace Yuki::System
