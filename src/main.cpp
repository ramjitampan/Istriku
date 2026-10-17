#include "ai/OllamaClient.h"
#include "ai/HuggingFaceClient.h"

#include "chat/ChatController.h"
#include "chat/CommandRouter.h"

#include "config/ConfigManager.h"

#include "memory/MemoryManager.h"

#include "system/IntentDetector.h"
#include "system/WindowsController.h"

#include "remote/RemoteServer.h"

#include "ui/TrayWindow.h"

#include "utils/HttpClient.h"
#include "utils/Logger.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <algorithm>

#include <windows.h>

using namespace Yuki;

static std::string ParseModeArg(int argc, char* argv[])
{
    for (int i = 0; i < argc - 1; ++i)
    {
        if (std::strcmp(argv[i], "--mode") == 0)
        {
            return argv[i + 1];
        }
    }
    return {};
}

static std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
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

    std::string backendName = cfg.ai.backend;
    std::transform(backendName.begin(), backendName.end(), backendName.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::unique_ptr<AI::AIBackend> backend;
    if (backendName == "huggingface")
    {
        const std::string token = AI::HuggingFaceClient::ReadTokenFromEnvironment(
            cfg.huggingface.token_env);
        if (!cfg.huggingface.model.empty() &&
            !cfg.huggingface.endpoint.empty() && !token.empty())
        {
            backend = std::make_unique<AI::HuggingFaceClient>(cfg.huggingface);
            Logger::Info("Using Hugging Face backend: " + cfg.huggingface.model);
        }
        else
        {
            Logger::Warning(
                "Hugging Face configuration or token is missing; falling back to Ollama.");
        }
    }
    else if (backendName != "ollama")
    {
        Logger::Warning("Unknown AI backend '" + cfg.ai.backend +
                        "'; falling back to Ollama.");
    }

    if (!backend)
        backend = std::make_unique<AI::OllamaClient>(httpClient, cfg.ollama);

    Memory::MemoryManager memoryManager(cfg.memory);

    Chat::ChatController chat(
        std::move(backend),
        memoryManager,
        &cfg);

    // Desktop command handling (pre-AI routing)
    System::WindowsController windowsController;
    System::IntentDetector intentDetector;

    windowsController.SetYoutubeApiKey(cfg.youtube_api_key);

    Chat::CommandRouter router(chat, windowsController, intentDetector, cfg);

    if (uiMode == "console")
    {
        std::cout << "\n===== Yuki AI =====\n";
        std::cout << "Mode aktif: Perintah\n";
        std::cout << "Gunakan /curhat dan /perintah untuk berpindah mode; /mode untuk melihat mode.\n";
        std::cout << "Ketik exit atau keluar untuk berhenti.\n\n";

        std::string line;
        while (true)
        {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line)) break;

            std::string text = Trim(line);
            if (text.empty()) continue;
            if (text == "exit" || text == "keluar" || text == "quit") break;

            const auto previousMode = router.GetMode();
            const std::string reply = router.HandleText(text);
            std::cout << "[Yuki] " << reply << "\n";
            if (router.GetMode() != previousMode)
            {
                std::cout << "[Mode aktif: "
                          << (router.GetMode() == Chat::ConversationMode::Curhat
                                  ? "Curhat" : "Perintah")
                          << "]\n";
            }
            std::cout << "\n";
        }

        std::cout << "Yuki berhenti.\n";
        return 0;
    }

    // Tray mode
    TrayWindow trayWindow(cfg, router);

    if (!trayWindow.Create())
    {
        std::cerr << "[ERROR] Gagal membuat tray window.\n";
        return 1;
    }

    // Remote server (kontrol dari HP lewat LAN)
    std::unique_ptr<Remote::RemoteServer> remoteServer;
    if (cfg.remote.enabled)
    {
        remoteServer = std::make_unique<Remote::RemoteServer>(
            cfg.remote.port, cfg.remote.token,
            [&router](const std::string& text) { return router.HandleText(text); });

        if (!remoteServer->Start())
            Logger::Error("Gagal start RemoteServer (port " +
                          std::to_string(cfg.remote.port) + ").");
    }

    Logger::Info("Yuki tray mode started");
    int exitCode = trayWindow.RunMessageLoop();

    if (remoteServer)
        remoteServer->Stop();

    return exitCode;
}
