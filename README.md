# Yuki - Voice-Based AI Personal Assistant (My istri) belum siap njir
'Project iseng project mandiri'

Yuki adalah proyek pengembangan asisten AI personal berbasis suara yang dirancang untuk berinteraksi secara verbal (tanpa input keyboard) dan memiliki kemampuan komputasi emosional untuk menjadi teman cerita atau tempat *curhat* bagi penggunanya.

---

## 🌟 Fitur Utama

- **Interaksi Berbasis Suara Penuh (Hands-Free)**: Menggunakan modul pengenal suara (*Voice Recognizer*) untuk menangkap perintah pengguna dan meresponsnya kembali menggunakan suara.
- **Pendengar yang Baik (Curhat-Friendly)**: Konfigurasi sistem prompt yang dioptimalkan agar AI memberikan respons yang empati, suportif, dan kontekstual layaknya seorang teman.
- **Sistem Memori Jangka Panjang**: Integrasi dengan database SQLite lokal untuk menyimpan riwayat percakapan, preferensi pengguna, dan memori masa lalu agar interaksi terasa lebih personal dari waktu ke waktu.
- **Ollama Integration (Lokal AI)**: Menjalankan model AI secara lokal untuk menjaga privasi data percakapan pengguna tanpa perlu bergantung pada koneksi internet atau API berbayar.
- **Sistem Kontrol Windows & Tray Icon**: Berjalan secara efisien di latar belakang sistem (*background process*) dengan akses cepat melalui menu *system tray*.

---

## 📂 Struktur Direktori Proyek

Proyek ini menggunakan bahasa pemrograman **C++** dengan struktur modular sebagai berikut:

```text
Yuki/
├── CMakeLists.txt         # Konfigurasi build sistem CMake
├── .gitignore             # File pemisah untuk mengabaikan build & database lokal
├── memory.db              # Database SQLite untuk memori jangka panjang AI
├── assets/                # Aset suara, ikon, dan file media lainnya
├── prompts/               # Kumpulan system prompt (karakter & gaya bicara AI)
├── database/              # Skema SQL dan konfigurasi database
├── models/                # Tempat menyimpan model AI (jika diperlukan)
├── third_party/           # Library pihak ketiga (misal: SQLite, JSON parser)
├── tests/                 # Unit testing untuk setiap komponen kode
└── src/                   # Source code utama aplikasi
    ├── main.cpp           # Entry point aplikasi
    ├── core/              # Logika utama aplikasi (Application lifecycle)
    ├── ai/                # Komunikasi dengan backend AI (Ollama Client)
    ├── chat/              # Manajemen percakapan dan alur logika teks
    ├── voice/             # Pengolahan suara (Speech-to-Text & Text-to-Speech)
    ├── memory/            # Manajemen memori dan interaksi database SQLite
    ├── system/            # Integrasi kontrol OS Windows
    ├── tray/              # Manajemen ikon aplikasi di system tray Windows
    ├── ui/                # Antarmuka grafis (GUI) aplikasi jika diperlukan
    └── utils/             # Fungsi pembantu (HTTP Client, JSON Parser)
```

---

## 🚀 Alur Kerja Aplikasi (Workflow)

1. **Voice Input**: Modul `voice/` mendengarkan suara pengguna melalui mikrofon, kemudian mengubahnya menjadi teks (*Speech-to-Text*).
2. **Context Retrieval**: Modul `memory/` mencari memori atau percakapan lama yang relevan dari `memory.db`.
3. **Prompt Injection**: Teks pengguna, memori lama, dan aturan karakter dari `prompts/` digabungkan menjadi satu kesatuan konteks.
4. **AI Processing**: Konteks tersebut dikirim ke modul `ai/` yang terhubung ke *Ollama Client* lokal untuk menghasilkan respons teks yang penuh empati.
5. **Memory Update**: Respons baru dari AI disimpan kembali ke dalam database untuk percakapan di masa mendatang.
6. **Voice Output**: Teks respons diubah kembali menjadi suara (*Text-to-Speech*) untuk diperdengarkan kepada pengguna.

---

## 🛠️ Prasyarat & Teknologi

- **Bahasa Pemrograman**: C++ (Standar C++17 atau lebih baru)
- **Build System**: CMake
- **Database**: SQLite3
- **AI Backend**: [Ollama](https://ollama.com) (Menjalankan model seperti Llama 3 atau Mistral secara lokal)
- **OS Target**: Windows (Menggunakan Windows API untuk kontrol sistem dan *tray icon*)

---

## 🔧 Pengembangan Lebih Lanjut (Roadmap)

- [ ] Integrasi *Wake Word* (Memicu AI hanya dengan memanggil namanya, misal: "Hey Yuki").
- [ ] Peningkatan kecepatan respon (*Low Latency*) audio.
- [ ] Implementasi analisis sentimen suara untuk mendeteksi emosi pengguna secara real-time.
