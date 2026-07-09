#pragma once

#include <memory>

// Forward declaration to avoid pulling Windows/UI headers into Application.h.
// Application only needs to know that MainWindow exists, not its internals.
class MainWindow;

// Application
//
// Single Responsibility: mengatur lifecycle aplikasi Yuki secara keseluruhan
// (inisialisasi, menjalankan loop utama, dan pembersihan sumber daya).
//
// Application TIDAK bertanggung jawab untuk menggambar UI, memproses AI,
// atau berinteraksi langsung dengan Windows API. Tugas tersebut didelegasikan
// ke komponen lain (mis. MainWindow) agar setiap kelas tetap fokus pada
// satu tanggung jawab (SRP).
//
// Pemanggil (main.cpp) hanya perlu tahu Run(); seluruh urutan
// Init -> MainWindow -> Message Loop -> Shutdown disembunyikan di dalamnya,
// sehingga main.cpp tidak mengetahui detail lifecycle sama sekali.
class Application {
public:
    Application();
    ~Application();

    // Menjalankan seluruh siklus hidup aplikasi: inisialisasi, menampilkan
    // MainWindow, menjalankan message loop, lalu membersihkan sumber daya.
    // Mengembalikan kode keluar aplikasi.
    int Run();

private:
    // Menginisialisasi seluruh komponen yang dibutuhkan sebelum aplikasi
    // berjalan (mis. membuat MainWindow). Mengembalikan false jika inisialisasi gagal.
    bool Init();

    // Membersihkan sumber daya sebelum aplikasi ditutup.
    // Aman dipanggil lebih dari sekali (idempotent).
    void Shutdown();

    // Kepemilikan MainWindow dipegang oleh Application melalui unique_ptr,
    // sehingga lifecycle window mengikuti lifecycle aplikasi (RAII).
    std::unique_ptr<MainWindow> m_mainWindow;

    bool m_isInitialized;
};