#pragma once

#include <string>

// Logger
//
// Single Responsibility: menyediakan mekanisme logging sederhana dan seragam
// untuk seluruh komponen aplikasi Yuki.
//
// Class lain (Application, MainWindow, dst.) TIDAK boleh menulis langsung ke
// std::cout/std::cerr; semua output diagnostik harus melalui Logger agar
// format dan tujuan output mudah diubah di kemudian hari (mis. ke file log)
// tanpa menyentuh kode class lain.
class Logger {
public:
    // Mencatat pesan informatif (alur normal aplikasi).
    static void Info(const std::string& message);

    // Mencatat pesan peringatan (kondisi tidak ideal namun belum fatal).
    static void Warning(const std::string& message);

    // Mencatat pesan error (kegagalan yang perlu diketahui pengguna/developer).
    static void Error(const std::string& message);

private:
    // Logger hanya berisi fungsi static, tidak perlu diinstansiasi.
    Logger() = delete;
};