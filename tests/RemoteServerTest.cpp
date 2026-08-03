#include "../src/remote/RemoteServer.h"
#include "../src/utils/HttpClient.h"
#include "../src/utils/Logger.h"

#include <iostream>
#include <mutex>
#include <string>

static int testsPassed = 0;
static int testsFailed = 0;

static void Test(const std::string& name, bool condition)
{
    if (condition) { std::cout << "[PASS] " << name << "\n"; ++testsPassed; }
    else { std::cout << "[FAIL] " << name << "\n"; ++testsFailed; }
}

int main()
{
    std::cout << "RemoteServer Tests\n";

    using Yuki::Remote::RemoteServer;

    std::mutex mtx;
    std::string received;
    bool called = false;

    auto handler = [&](const std::string& text) {
        std::lock_guard<std::mutex> lock(mtx);
        called = true;
        received = text;
        return "Halo dari server!";
    };

    RemoteServer server(0, "", handler);
    Test("Server start", server.Start());

    int port = server.GetPort();
    Test("Server port assigned", port > 0);
    std::string base = "http://127.0.0.1:" + std::to_string(port);

    Yuki::Utils::HttpClient client;

    // GET /api/health
    auto health = client.Get(base + "/api/health");
    Test("GET /api/health -> 200",
         health.statusCode == 200);
    Test("GET /api/health body ok",
         health.body.find("\"ok\":true") != std::string::npos);

    // POST /api/say
    auto say = client.Post(base + "/api/say", R"({"text":"buka vscode"})");
    Test("POST /api/say -> 200", say.statusCode == 200);
    {
        std::lock_guard<std::mutex> lock(mtx);
        Test("Handler dipanggil", called);
        Test("Teks diterima handler", received == "buka vscode");
    }
    Test("Balasan handler ada di body",
         say.body.find("Halo dari server!") != std::string::npos);

    // POST /api/say kosong -> 400
    auto empty = client.Post(base + "/api/say", R"({"text":""})");
    Test("POST /api/say kosong -> 400", empty.statusCode == 400);

    // Path tak dikenal -> 404
    auto nf = client.Get(base + "/api/nope");
    Test("GET /api/nope -> 404", nf.statusCode == 404);

    server.Stop();
    Test("Server stop", !server.IsRunning());

    // ---- Token auth ----
    RemoteServer secured(0, "rahasia",
        [](const std::string&) { return "aman"; });
    Test("Secured server start", secured.Start());

    std::string sbase = "http://127.0.0.1:" + std::to_string(secured.GetPort());

    auto unauth = client.Post(sbase + "/api/say", R"({"text":"x"})");
    Test("Tanpa token -> 401", unauth.statusCode == 401);

    Yuki::Utils::HttpConfig cfg;
    cfg.headers["X-Yuki-Token"] = "rahasia";
    Yuki::Utils::HttpClient authed(cfg);

    auto auth = authed.Post(sbase + "/api/say", R"({"text":"x"})");
    Test("Dengan token -> 200", auth.statusCode == 200);

    secured.Stop();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";
    std::cout << "Total:  " << (testsPassed + testsFailed) << "\n";

    return testsFailed > 0 ? 1 : 0;
}
