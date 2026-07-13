#pragma once

#include "Config.h"

#include <memory>
#include <string>

namespace Yuki::Config {

class ConfigManager final
{
public:
    ConfigManager();
    explicit ConfigManager(std::string configPath);

    bool Load();
    void Reload();

    const Config& Get() const noexcept;

private:
    bool LoadFromFile();
    void ApplyDefaults();
    void LogConfig() const;

    std::string m_configPath;
    Config m_config;
};

} // namespace Yuki::Config
