#include "ai/OllamaClient.h"

#include "chat/ChatController.h"

#include "config/ConfigManager.h"

#include "memory/MemoryManager.h"

#include "system/IntentDetector.h"
#include "system/WindowsController.h"

#include "ui/TrayWindow.h"

#include "voice/VoiceSession.h"
#include "voice/WhisperRecognizer.h"

#include "utils/HttpClient.h"
#include "utils/Logger.h"
#include "utils/Utf8Sanitizer.h"
#include "utils/TextNormalizer.h"

#include <cstdlib>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include <windows.h>

using namespace Yuki;

static Voice::VoiceSession* g_voiceSession = nullptr;

static void SignalHandler(int)
{
    if (g_voiceSession)
        g_voiceSession->Stop();
    std::cout << "\nYuki berhenti.\n";
    std::exit(0);
}

static std::string ParseModeArg(int argc, char* argv[])
{
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::strcmp(argv[i], "--mode") == 0)
        {
            return argv[i + 1];
        }
    }
    return {};
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Config::ConfigManager configManager;
    configManager.Load();
    const auto& cfg = configManager.Get();

    // CLI arg overrides config
    std::string cliMode = ParseModeArg(argc, argv);
    std::string uiMode = cliMode.empty() ? cfg.app.ui_mode : cliMode;

    Utils::HttpClient httpClient;

    auto backend =
        std::make_unique<AI::OllamaClient>(httpClient, cfg.ollama);

    Memory::MemoryManager memoryManager(cfg.memory);

    Chat::ChatController chat(
        std::move(backend),
        memoryManager,
        &cfg);

    // Desktop command handling (pre-AI routing)
    System::WindowsController windowsController;
    System::IntentDetector intentDetector;

    if (uiMode == "console")
    {
        std::unique_ptr<Voice::WhisperRecognizer> recognizer;
        try
        {
            recognizer =
                std::make_unique<Voice::WhisperRecognizer>(cfg.voice);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[ERROR] " << e.what() << "\n";
            std::cerr << "Pastikan model ada di: " << cfg.voice.model << "\n";
            return 1;
        }

        Voice::VoiceSession voiceSession(
            std::move(recognizer),
            [&chat, &windowsController, &intentDetector](const std::string& text) -> std::string
            {
                if (text.empty())
                {
                    Logger::Info("[Voice] Empty after normalization, ignoring");
                    return {};
                }

                System::Command command = intentDetector.Detect(text);

                if (command.type != System::CommandType::Unknown)
                {
                    Logger::Info("[Voice] Intent: " + std::string(System::CommandTypeToString(command.type)));
                    System::CommandResult result = windowsController.Execute(command);

                    if (result.confirmationRequired)
                    {
                        return chat.SendMessage(text);
                    }

                    if (!result.success)
                    {
                        Logger::Error("[Voice] Desktop command failed: " + result.message);
                    }
                    else
                    {
                        Logger::Info("[Voice] Desktop command success: " + result.message);
                    }

                    return result.message;
                }

                Logger::Info("[Voice] Intent: None — sending to AI");
                std::string reply = chat.SendMessage(text);
                return reply;
            });

        g_voiceSession = &voiceSession;
        std::signal(SIGINT, SignalHandler);

        voiceSession.SetRate(cfg.tts.rate);
        voiceSession.SetVolume(cfg.tts.volume);
        voiceSession.SetListeningMode(Voice::ListeningMode::AlwaysOn);

        voiceSession.OnStateChanged(
            [](Voice::VoiceSessionState state)
            {
                switch (state)
                {
                    case Voice::VoiceSessionState::Idle:
                        std::cout << "\n[Yuki] idle\n"; break;
                    case Voice::VoiceSessionState::ListeningWakeWord:
                        std::cout << "\n[Yuki] Listening Wake Word...\n"; break;
                    case Voice::VoiceSessionState::RecordingCommand:
                        std::cout << "\n[Yuki] Recording Command...\n"; break;
                    case Voice::VoiceSessionState::Thinking:
                        std::cout << "\n[Yuki] Thinking...\n"; break;
                    case Voice::VoiceSessionState::Speaking:
                        std::cout << "\n[Yuki] Speaking...\n"; break;
                }
            });

        if (!voiceSession.Start())
        {
            std::cerr << "[Voice] Gagal buka microphone!\n";
            return 1;
        }

        std::cout << "\n===== Yuki AI =====\n";
        std::cout << "Mode: Voice (ucap 'Yuki...')\n";
        std::cout << "Ctrl+C untuk keluar.\n\n";

        while (true)
        {
            std::cin.get();
        }
    }
    else
    {
        // Tray mode
        TrayWindow trayWindow(cfg, chat, windowsController, intentDetector);

        if (!trayWindow.Create())
        {
            std::cerr << "[ERROR] Gagal membuat tray window.\n";
            return 1;
        }

        Logger::Info("Yuki tray mode started");
        return trayWindow.RunMessageLoop();
    }

    return 0;
}
