# Yukihana Training Workspace

Workspace ini hanya untuk supervised fine-tuning Yukihana menggunakan QLoRA.
Runtime utama tetap `Yukihana.exe` berbasis C++17 dan tidak bergantung pada Python.

## Dataset

Dataset memakai JSON Lines conversational. Setiap baris adalah satu percakapan:

```json
{"messages":[{"role":"user","content":"Halo"},{"role":"assistant","content":"Halo, sayang."}]}
```

Role yang diterima adalah `system`, `user`, `assistant`, dan `tool`. Setiap record
harus berakhir dengan respons `assistant`. Validation set harus terpisah dan tidak
boleh menduplikasi training set.

Validasi dataset tanpa mengunduh model:

```powershell
cd training
python prepare_dataset.py
```

## Environment

Gunakan virtual environment terpisah. Instalasi dependency dan model download
belum perlu dilakukan pada tahap persiapan dataset ini.

```powershell
cd training
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## Kandidat Base Model

- `Qwen/Qwen2.5-3B-Instruct`: pilihan awal karena ringan, memiliki chat template,
  dan kuat untuk instruction following serta coding. Periksa lisensinya sebelum distribusi.
- `meta-llama/Llama-3.2-3B-Instruct`: alternatif 3B yang matang; akses dan distribusi
  mengikuti lisensi Llama 3.2.
- `google/gemma-3-4b-it`: kandidat 4B multilingual, tetapi gated dan wajib menerima
  lisensi Gemma sebelum model dapat diunduh.

Mulai eksperimen dengan Qwen 3B. Lakukan perbandingan kecil memakai validation set
yang sama sebelum memilih model final.

## Perkiraan VRAM QLoRA

Angka berikut adalah perkiraan, bukan jaminan. Panjang konteks, batch size,
checkpointing, optimizer, dan arsitektur model memengaruhi penggunaan aktual.

| Model | Minimum praktis | Disarankan |
| --- | ---: | ---: |
| 3B, 4-bit QLoRA, sequence 2048, batch 1 | 6-8 GB | 10-12 GB |
| 4B, 4-bit QLoRA, sequence 2048, batch 1 | 8-10 GB | 12-16 GB |

Jika kehabisan VRAM, kurangi `--max-length`, pertahankan batch size 1, dan gunakan
gradient accumulation. GPU NVIDIA Pascal atau lebih baru mendukung NF4/FP4 melalui
bitsandbytes, tetapi BF16 memerlukan GPU yang lebih baru; script otomatis memakai FP16
jika BF16 tidak tersedia.

## Training Nanti

Perintah ini disiapkan untuk tahap berikutnya. Jangan dijalankan sebelum dataset
ditinjau dan environment GPU siap.

```powershell
cd training
python train_qlora.py `
  --model Qwen/Qwen2.5-3B-Instruct `
  --output-dir outputs/yukihana-qwen2.5-3b `
  --epochs 3 `
  --max-length 2048 `
  --batch-size 1 `
  --gradient-accumulation 8
```

Evaluasi kualitatif setelah training:

```powershell
python evaluate.py --adapter outputs/yukihana-qwen2.5-3b
```

Upload sengaja membutuhkan konfirmasi eksplisit dan tidak dilakukan otomatis:

```powershell
$env:HF_TOKEN = "hf_token"
python push_to_hub.py --model-dir outputs/yukihana-qwen2.5-3b `
  --repo-id username/Yukihana-3B-QLoRA --private --yes
```

## Catatan Kualitas

Dataset awal ini hanya template untuk menguji format dan cakupan perilaku. Dua puluh
record belum cukup untuk menghasilkan model produksi. Dataset nyata perlu lebih banyak
variasi, contoh negatif, bahasa natural pengguna, tool schema yang stabil, dan review
manual untuk mencegah respons repetitif atau perilaku approval yang tidak konsisten.
