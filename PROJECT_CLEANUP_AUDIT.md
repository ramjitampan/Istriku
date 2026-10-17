# Yukihana Stage 4 Cleanup Audit

Tanggal: 17 Oktober 2026
Scope: `D:\AI_Asisten\Yuki` saja. `D:\AI_Asisten\mobileYuki` tidak diubah.

## Backup dan database

- Snapshot pre-cleanup: `backup_stage4_precleanup_20261017/` (termasuk `.git`, konfigurasi, eksperimen, artefak build, dan database).
- SHA-256 `memory.db` sebelum pekerjaan: `4ECA8104C00D676EA11A0B29DB1A09322F3C1D504F424491D5D25F1BACC881F7`.
- SHA-256 `memory.db` pada sumber dan backup tetap sama setelah cleanup.
- `PRAGMA integrity_check` menghasilkan `ok` untuk `memory.db`, `build/memory.db`, `build/bin/memory.db`, dan salinan dalam backup. Dua database di folder `build` memiliki hash berbeda, jadi keduanya dipertahankan.

## Klasifikasi audit

### Aktif dan diperlukan

- `src/main.cpp`, `src/ai`, `src/chat`, `src/config`, `src/memory`, `src/remote`, `src/system`, `src/ui`, dan `src/utils`: dipakai target aplikasi `Yuki` atau alur aktifnya.
- `config/config.json`: konfigurasi runtime yang dilacak Git. Nilai remote saat audit: aktif, port 8123, token kosong; nilainya tidak diubah pada tahap ini.
- `third_party/sqlite` dan `third_party/nlohmann`: dipakai kode SQLite dan JSON.
- `build/`: direktori build utama dipertahankan, termasuk dua salinan database di dalamnya.

### Legacy tetapi masih memiliki referensi

- `src/voice/TextToSpeech.*`: tidak ditautkan ke target aplikasi utama; masih menjadi source target `TextToSpeechTest` dan dipakai oleh source `VoiceSession`. Tidak dihapus.
- `src/voice/WhisperRecognizer.*`, `VoiceSession.*`, `WakeWordDetector.*`, dan `src/audio/*`: tidak masuk jalur runtime utama. Source/config/test masih merujuk pada komponen tersebut; sebagian target tes audio masih aktif. Tidak dihapus.
- `third_party/whisper.cpp`: source dan model `ggml-tiny.bin` (~74 MiB) serta `ggml-base.bin` (~141 MiB) masih dirujuk config/source/test dormant. Tidak dihapus.
- `src/core/Application.cpp` dan `src/ui/MainWindow.cpp`: masih tercantum pada target `Yuki`, walau entry point sekarang menggunakan tray window. Dipertahankan.

### Eksperimen yang dipertahankan

- `training/*.py`, `training/Yukihana_QLoRA_Colab.ipynb`, requirements, dan dua dataset JSONL berukuran sekitar 105 KB total. Ini jelas artefak eksperimen, tetapi status pemakaiannya belum cukup untuk penghapusan.
- Tidak ditemukan direktori output/checkpoint training saat audit. Model Ollama yang dipasang di sistem berada di luar scope dan tidak disentuh.

### Kandidat cleanup yang dihapus

- `build_commandcheck/`, `build_weathercheck/`, `build_elearning_check/`: direktori CMake sementara dari build verifikasi sebelumnya; masing-masing berisi cache/build output, tanpa `memory.db` atau source. Dihapus setelah backup dan build utama berhasil.
- `debug.log`: satu baris diagnostik Crashpad eksternal, tidak dirujuk source/config/build. Dihapus.
- `.gitignore` kini mengecualikan `build_*/`, `backup_*.zip`, `backup_*/`, dan `debug.log` agar artefak lokal tidak masuk commit berikutnya.

## Dependensi dan perubahan build

- Tidak ada dependensi vendor atau source legacy yang dihapus.
- Build semua target sebelumnya gagal karena beberapa target tes yang mengompilasi `WindowsController.cpp` tidak menyertakan `third_party`/`src/utils/Json.cpp`. CMake target tes terkait diperbaiki dengan include/source yang diperlukan; target aplikasi dan perilaku runtime tidak berubah.
- Fixture `ModeRoutingTest` diganti menjadi kalimat yang tidak dikenali. Fixture lama menyebut cuaca dan memang cocok dengan intent `WeatherHelp`, sehingga assertion “unknown” salah.

## Dokumentasi dan verifikasi

- `README.md` diperbarui agar menjelaskan pemisahan mode eksplisit, build/run Windows 11, serta kontrak remote.
- Build semua target CMake berhasil dengan `cmake --build build --parallel 4`.
- Tes yang dipilih setelah cleanup lulus: intent (22), routing mode (16), remote/auth (14), riwayat prompt (8), persona (11), isolasi perintah AI (11), transisi mode (51), config (41), UTF-8 (49), normalisasi wake word (26), audio config (13), resampler (12), MFCC (10), speaker verifier (7), normalisasi recognition (52), confidence filter (15), command recording config (11), initial prompt (12), dan wake word (51). Extractor, retriever, prompt builder, memory rules, dan memory storage juga keluar dengan kode 0.
- `RemoteServerTest` menguji `/api/health`, `/api/say`, 401 tanpa token, dan 200 dengan token pada port lokal sementara.
- Kontrak Flutter diperiksa baca-saja: path `/api/health`, `/api/say`, body `text`, field balasan `reply`, dan header `X-Yuki-Token` cocok. Tes Flutter tidak dijalankan dan tidak ada file `mobileYuki` yang diubah.
- Tes TTS dan tes Windows yang dapat menghasilkan suara atau mengubah state sistem tidak dijalankan. Seluruh target tes tetap berhasil dibangun.

## Estimasi ruang

- Artefak build sementara dan log yang dihapus: **103,228,663 byte**, sekitar **98.4 MiB**.
- Backup pre-cleanup berukuran sekitar **8.79 GiB** dan sengaja dipertahankan. Karena backup lengkap tersimpan di dalam workspace, penggunaan disk keseluruhan naik sekitar 8.69 GiB bersih meski artefak sementara sudah dibersihkan.

## Hal yang masih perlu ditinjau

- `RemoteServer` berjalan tanpa autentikasi bila token kosong. Saat audit, konfigurasi lokal memang memakai token kosong; setting dan mekanisme autentikasi tidak diubah.
- TTS, Whisper, dan voice session tetap dormant/legacy. Penghapusan memerlukan keputusan terpisah karena masih dirujuk tests, konfigurasi, atau komponen dormant.
- Pengujian integrasi langsung dengan Ollama dan Flutter pada perangkat tidak dilakukan.
