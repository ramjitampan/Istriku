#include "ConfigManager.h"
#include "../utils/Logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

namespace Yuki::Config {

ConfigManager::ConfigManager()
    : m_configPath("config/config.json")
{
}

ConfigManager::ConfigManager(std::string configPath)
    : m_configPath(std::move(configPath))
{
}

bool ConfigManager::Load()
{
    Logger::Info("Loading configuration...");

    ApplyDefaults();

    if (!LoadFromFile())
    {
        Logger::Warning("Configuration file not found or invalid. Using defaults.");
        LogConfig();
        return false;
    }

    Logger::Info("Configuration loaded.");
    LogConfig();
    return true;
}

void ConfigManager::Reload()
{
    Load();
}

const Config& ConfigManager::Get() const noexcept
{
    return m_config;
}

bool ConfigManager::LoadFromFile()
{
    std::ifstream file(m_configPath);
    if (!file.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(buffer.str());
    }
    catch (const nlohmann::json::parse_error&)
    {
        return false;
    }

    // Voice
    if (root.contains("voice") && root["voice"].is_object())
    {
        auto& v = root["voice"];
        if (v.contains("enabled") && v["enabled"].is_boolean())
            m_config.voice.enabled = v["enabled"].get<bool>();
        if (v.contains("model") && v["model"].is_string())
            m_config.voice.model = v["model"].get<std::string>();
        if (v.contains("language") && v["language"].is_string())
            m_config.voice.language = v["language"].get<std::string>();
        if (v.contains("threads") && v["threads"].is_number_integer())
            m_config.voice.threads = v["threads"].get<int>();
        if (v.contains("vad_threshold") && v["vad_threshold"].is_number())
            m_config.voice.vad_threshold = v["vad_threshold"].get<float>();
        if (v.contains("wake_word") && v["wake_word"].is_string())
            m_config.voice.wake_word = v["wake_word"].get<std::string>();
        if (v.contains("sample_rate") && v["sample_rate"].is_number_integer())
            m_config.voice.sample_rate = v["sample_rate"].get<int>();
        if (v.contains("silence_timeout_ms") && v["silence_timeout_ms"].is_number_integer())
            m_config.voice.silence_timeout_ms = v["silence_timeout_ms"].get<int>();
        if (v.contains("speech_start_threshold") && v["speech_start_threshold"].is_number())
            m_config.voice.speech_start_threshold = v["speech_start_threshold"].get<float>();
        if (v.contains("speech_stop_threshold") && v["speech_stop_threshold"].is_number())
            m_config.voice.speech_stop_threshold = v["speech_stop_threshold"].get<float>();
        if (v.contains("minimum_command_duration_ms") && v["minimum_command_duration_ms"].is_number_integer())
            m_config.voice.minimum_command_duration_ms = v["minimum_command_duration_ms"].get<int>();
        if (v.contains("initial_prompt") && v["initial_prompt"].is_string())
            m_config.voice.initial_prompt = v["initial_prompt"].get<std::string>();
        if (v.contains("confidence_threshold") && v["confidence_threshold"].is_number())
            m_config.voice.confidence_threshold = v["confidence_threshold"].get<float>();
        if (v.contains("speaker_verification_enabled") && v["speaker_verification_enabled"].is_boolean())
            m_config.voice.speaker_verification_enabled = v["speaker_verification_enabled"].get<bool>();
        if (v.contains("similarity_threshold") && v["similarity_threshold"].is_number())
            m_config.voice.similarity_threshold = v["similarity_threshold"].get<float>();
        if (v.contains("curhat_trigger_phrases") && v["curhat_trigger_phrases"].is_array())
            m_config.voice.curhat_trigger_phrases = v["curhat_trigger_phrases"].get<std::vector<std::string>>();
        if (v.contains("curhat_exit_phrases") && v["curhat_exit_phrases"].is_array())
            m_config.voice.curhat_exit_phrases = v["curhat_exit_phrases"].get<std::vector<std::string>>();
    }

    // TTS
    if (root.contains("tts") && root["tts"].is_object())
    {
        auto& t = root["tts"];
        if (t.contains("enabled") && t["enabled"].is_boolean())
            m_config.tts.enabled = t["enabled"].get<bool>();
        if (t.contains("volume") && t["volume"].is_number_integer())
            m_config.tts.volume = t["volume"].get<int>();
        if (t.contains("rate") && t["rate"].is_number_integer())
            m_config.tts.rate = t["rate"].get<int>();
    }

    // Ollama
    if (root.contains("ollama") && root["ollama"].is_object())
    {
        auto& o = root["ollama"];
        if (o.contains("host") && o["host"].is_string())
            m_config.ollama.host = o["host"].get<std::string>();
        if (o.contains("model") && o["model"].is_string())
            m_config.ollama.model = o["model"].get<std::string>();
    }

    // Memory
    if (root.contains("memory") && root["memory"].is_object())
    {
        auto& m = root["memory"];
        if (m.contains("database") && m["database"].is_string())
            m_config.memory.database = m["database"].get<std::string>();
    }

    // System
    if (root.contains("system") && root["system"].is_object())
    {
        auto& s = root["system"];
        if (s.contains("confirm_shutdown") && s["confirm_shutdown"].is_boolean())
            m_config.system.confirm_shutdown = s["confirm_shutdown"].get<bool>();
        if (s.contains("confirm_restart") && s["confirm_restart"].is_boolean())
            m_config.system.confirm_restart = s["confirm_restart"].get<bool>();
        if (s.contains("confirm_sleep") && s["confirm_sleep"].is_boolean())
            m_config.system.confirm_sleep = s["confirm_sleep"].get<bool>();
    }

    // App
    if (root.contains("app") && root["app"].is_object())
    {
        auto& a = root["app"];
        if (a.contains("ui_mode") && a["ui_mode"].is_string())
            m_config.app.ui_mode = a["ui_mode"].get<std::string>();
    }

    // Persona prompt
    if (root.contains("persona_prompt") && root["persona_prompt"].is_string())
        m_config.persona_prompt = root["persona_prompt"].get<std::string>();

    return true;
}

void ConfigManager::ApplyDefaults()
{
    m_config = Config{};
}

void ConfigManager::LogConfig() const
{
    if (m_config.voice.enabled)
        Logger::Info("Voice enabled");
    else
        Logger::Info("Voice disabled");

    Logger::Info("Ollama model: " + m_config.ollama.model);
    Logger::Info("Database: " + m_config.memory.database);
}

} // namespace Yuki::Config
