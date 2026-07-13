# Yuki - Voice-Based AI Personal Assistant (My istri) belum siap njir
*Project iseng project mandiri*

Yuki adalah proyek pengembangan asisten AI personal berbasis suara yang dirancang untuk berinteraksi secara verbal (tanpa input keyboard) dan memiliki kemampuan komputasi emosional untuk menjadi teman cerita atau tempat *curhat* bagi penggunanya.

---

## 🌟 Fitur Utama

- **Interaksi Berbasis Suara (Push-to-Talk)**: Berjalan sebagai *system tray app* — klik icon untuk memunculkan jendela kecil, tekan-tahan tombol mic untuk bicara. Menggunakan Whisper (`whisper.cpp`, model `base`) untuk Speech-to-Text.
- **Dual Conversation Mode**:
  - **Mode Perintah** (default) — command dikenali lewat `IntentDetector` dan dieksekusi langsung (buka aplikasi, kunci layar, atur brightness, dll) tanpa memanggil AI, hemat compute.
  - **Mode Curhat** — dipicu dengan kata kunci tertentu ("aku mau curhat", "aku capek", dll), percakapan dikirim ke Ollama dengan persona custom (yandere, empatik, suportif) yang bisa diatur lewat `config.json` tanpa recompile.
- **Voice Level Meter**: Indikator visual real-time (reuse RMS dari VAD) saat menekan tombol push-to-talk, biar tahu suara sudah tertangkap dengan baik.
- **Speaker Verification (MFCC + Cosine Similarity)**: Verifikasi suara berbasis DSP murni (bukan deep learning) untuk memastikan hanya suara yang sudah di-*enroll* yang bisa memicu command/curhat.
- **Sistem Memori Jangka Panjang**: Integrasi SQLite lokal (`memory.db`) untuk menyimpan riwayat percakapan, ditampilkan sebagai chat log di jendela push-to-talk.
- **Ollama Integration (Lokal AI)**: Model AI jalan lokal (`gemma3:4b`), privasi percakapan terjaga tanpa API eksternal.
- **Normalisasi Teks yang Robust**: Pipeline normalisasi hasil transkrip (typo correction, word-boundary-aware replacement, fuzzy matching untuk trigger/exit phrase) untuk menoleransi kesalahan dengar STT.
- **Sistem Kontrol Windows & Tray Icon**: Berjalan efisien di *background* lewat `Shell_NotifyIcon`, akses cepat lewat *system tray*.

---

## 📂 Struktur Direktori Proyek

Proyek ini menggunakan **C++17** dengan struktur modular sebagai berikut:

```text
Yuki/
├── CMakeLists.txt         # Konfigurasi build sistem CMake
├── .gitignore             # File pemisah untuk mengabaikan build & database lokal
├── memory.db              # Database SQLite untuk memori jangka panjang AI
├── config/                # config.json — model path, trigger/exit phrase, persona prompt, threshold
├── assets/                # Aset suara, ikon, dan file media lainnya
├── prompts/               # Kumpulan system prompt (karakter & gaya bicara AI)
├── database/               # Skema SQL dan konfigurasi database
├── models/                 # Model Whisper (ggml) & model lain jika diperlukan
├── third_party/            # Library pihak ketiga (whisper.cpp, SQLite, JSON parser, dll)
├── tests/                  # Unit testing untuk setiap komponen kode
└── src/                    # Source code utama aplikasi
    ├── main.cpp             # Entry point aplikasi
    ├── core/                # Logika utama aplikasi (Application lifecycle)
    ├── ai/                  # Komunikasi dengan backend AI (Ollama Client)
    ├── audio/               # MFCC extractor & Speaker Verifier
    ├── chat/                # ChatController — dual mode logic (Perintah/Curhat)
    ├── voice/               # Pengolahan suara (Whisper Recognizer, VAD, Resampler)
    ├── memory/              # Manajemen memori dan interaksi database SQLite
    ├── system/               # Integrasi kontrol OS Windows (Command, WindowsController)
    ├── tray/                 # Manajemen ikon aplikasi di system tray Windows
    ├── ui/                  # TrayWindow — jendela push-to-talk, chat log, voice level meter
    └── utils/               # TextNormalizer, Utf8Sanitizer, HttpClient, JSON, Logger
```

---

## 🚀 Alur Kerja Aplikasi (Workflow)

1. **Push-to-Talk**: User menekan-tahan tombol mic di jendela tray → `voice/` merekam audio via VAD (RMS-based, hysteresis threshold).
2. **Speaker Verification** *(opsional, jika diaktifkan)*: `audio/` memverifikasi suara cocok dengan hasil *enrollment* sebelum lanjut ke transkripsi.
3. **Speech-to-Text**: Audio diproses Whisper (`base` model), hasil transkrip dinormalisasi (`TextNormalizer`) untuk menoleransi kesalahan dengar.
4. **Mode Routing**: `ChatController` menentukan mode aktif:
   - Intent dikenal → eksekusi langsung lewat `system/` (skip AI).
   - Trigger phrase cocok / sudah dalam mode Curhat → lanjut ke AI.
   - Tidak match apapun di mode Perintah → balasan lokal, skip AI (hemat compute).
5. **Context Retrieval**: `memory/` mengambil riwayat percakapan relevan dari `memory.db`.
6. **AI Processing**: Teks + memori + persona prompt dikirim ke `ai/` (Ollama lokal) untuk respons empatik/kontekstual.
7. **Memory Update**: Respons baru disimpan kembali ke `memory.db`.
8. **Voice Output**: Respons diucapkan lewat TTS dan ditampilkan di chat log jendela tray.

---

## 🛠️ Prasyarat & Teknologi

- **Bahasa Pemrograman**: C++17, RAII & smart pointer di seluruh codebase
- **Build System**: CMake
- **Speech-to-Text**: whisper.cpp (`ggml-base.bin`)
- **Database**: SQLite3
- **AI Backend**: [Ollama](https://ollama.com) (`gemma3:4b`, lokal)
- **UI**: Win32 API native (tray icon + jendela push-to-talk, tanpa framework GUI berat)
- **OS Target**: Windows

---

## 🔧 Pengembangan Lebih Lanjut (Roadmap)

- [x] ~~Integrasi *Wake Word*~~ — sudah diimplementasikan, saat ini dinonaktifkan secara default (diganti push-to-talk), logic tetap tersimpan untuk diaktifkan kembali nanti.
- [x] ~~Peningkatan kecepatan respon (*Low Latency*) audio~~ — buffer reuse, VAD hysteresis, resampler linear interpolation.
- [ ] Audit & perbaikan intent matching yang masih rawan false-positive (OpenVSCode, OpenExplorer, Screenshot, Lock — kata kunci terlalu umum).
- [ ] Tuning threshold Speaker Verification berdasarkan pengujian kondisi ruangan asli (bukan cuma unit test sintetis).
- [ ] Evaluasi apakah perlu upgrade model Whisper lebih lanjut (`small`) jika akurasi `base` masih kurang untuk kosakata tertentu.
- [ ] Implementasi analisis sentimen suara untuk mendeteksi emosi pengguna secara real-time.
- [ ] Custom tray icon (masih pakai placeholder).

---

## 🐛 Known Issues

- Beberapa intent (`OpenVSCode`, `OpenExplorer`, `Screenshot`, `Lock`) menggunakan keyword yang cukup umum dan berpotensi false-positive — belum diperbaiki, prioritas rendah.
- Confidence score dari Whisper (`no_speech_prob`) tidak selalu mencerminkan akurasi transkrip — bisa tinggi walau hasil transkrip salah/gibberish.
