#include "Application.h"
#include "../ui/MainWindow.h"
#include "../utils/Logger.h"

Application::Application()
    : m_mainWindow(nullptr)
    , m_isInitialized(false) {
}

Application::~Application() {
    // RAII safety net: jika Run() belum sempat memanggil Shutdown() secara
    // normal (mis. karena early-return atau exception), destructor tetap
    // menjamin sumber daya dibersihkan.
    Shutdown();
}

int Application::Run() {
    if (!Init()) {
        Logger::Error("Application: inisialisasi gagal, aplikasi dihentikan.");
        return -1;
    }

    m_mainWindow->Show();

    // Message loop didelegasikan ke MainWindow karena berkaitan langsung
    // dengan siklus hidup window itu sendiri (SRP).
    const int exitCode = m_mainWindow->RunMessageLoop();

    Shutdown();

    return exitCode;
}

bool Application::Init() {
    if (m_isInitialized) {
        return true; // Idempotent: sudah diinisialisasi sebelumnya.
    }

    // Application hanya bertanggung jawab mengorkestrasi lifecycle;
    // pembuatan window sesungguhnya didelegasikan ke MainWindow.
    m_mainWindow = std::make_unique<MainWindow>();

    if (!m_mainWindow->Create()) {
        Logger::Error("Application: gagal membuat MainWindow.");
        m_mainWindow.reset();
        return false;
    }

    Logger::Info("Application: inisialisasi berhasil.");
    m_isInitialized = true;
    return true;
}

void Application::Shutdown() {
    if (!m_isInitialized) {
        return; // Idempotent: tidak ada yang perlu dibersihkan.
    }

    if (m_mainWindow) {
        m_mainWindow->Destroy();
        m_mainWindow.reset();
    }

    Logger::Info("Application: shutdown selesai.");
    m_isInitialized = false;
}