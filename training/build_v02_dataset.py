"""Build the controlled Yukihana v0.2 train/validation datasets."""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any

from yukihana_prompt import with_system_prompt


TARGETS = {
    "tool_agent": 77,
    "coding": 44,
    "memory_reminder": 33,
    "failure_ambiguity": 22,
    "personality": 33,
    "safety": 11,
}
PROJECTS = ["Nebula", "Atlas", "Orion", "Aurora", "Kestrel", "Lentera", "Sakura", "Arunika"]
FILES = [
    "AuthService.cpp", "NetworkClient.cpp", "ConfigManager.cpp", "RemoteServer.cpp",
    "TokenStore.cpp", "Database.cpp", "ReminderService.cpp", "WorkspaceManager.cpp",
]


def tool_call(name: str, arguments: dict[str, Any]) -> str:
    payload = json.dumps({"name": name, "arguments": arguments}, ensure_ascii=False, separators=(",", ":"))
    return f"<tool_call>\n{payload}\n</tool_call>"


def record(messages: list[dict[str, str]], category: str) -> dict[str, Any]:
    return {"messages": with_system_prompt(messages), "category": category}


def user(content: str) -> dict[str, str]:
    return {"role": "user", "content": content}


def assistant(content: str) -> dict[str, str]:
    return {"role": "assistant", "content": content}


def tool(content: str | dict[str, Any]) -> dict[str, str]:
    rendered = json.dumps(content, ensure_ascii=False, separators=(",", ":")) if isinstance(content, dict) else content
    return {"role": "tool", "content": rendered}


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as source:
        return [json.loads(line) for line in source if line.strip()]


def tool_names(messages: list[dict[str, str]]) -> set[str]:
    names: set[str] = set()
    for message in messages:
        if message["role"] != "assistant" or "<tool_call>" not in message["content"]:
            continue
        payload = message["content"].split("\n", 1)[1].rsplit("\n", 1)[0]
        names.add(json.loads(payload)["name"])
    return names


def classify(messages: list[dict[str, str]]) -> str:
    names = tool_names(messages)
    text = " ".join(message["content"] for message in messages).lower()
    if names & {"memory_save", "memory_search", "memory_delete", "memory_pin_current_context",
                "create_reminder", "reminder_search", "reminder_cancel"}:
        return "memory_reminder"
    if any(word in text for word in ("hapus seluruh", "reset --hard", "destruktif", "di luar workspace")):
        return "safety"
    if names & {"prepare_patch", "apply_patch", "run_command", "run_tests"} or "build" in text:
        return "coding"
    if names:
        return "tool_agent"
    if any(word in text for word in ("cemburu", "sayang", "capek", "bosan", "presentasi", "asisten ai lain")):
        return "personality"
    if any(word in text for word in ("gagal", "tidak ditemukan", "permission", "ambigu", "belum jelas")):
        return "failure_ambiguity"
    return "personality"


def tool_agent_example(index: int) -> dict[str, Any]:
    project = f"{PROJECTS[index % len(PROJECTS)]}-{index + 1}"
    filename = FILES[index % len(FILES)]
    variant = index % 8
    if variant == 0:
        messages = [user(f"Tampilkan struktur project {project}."), assistant(tool_call("list_files", {"path": ".", "depth": 2})),
                    tool(f"CMakeLists.txt\nsrc/{filename}\ntests/{filename.replace('.cpp', 'Test.cpp')}"),
                    assistant("Struktur awal sudah terlihat. Aku akan membaca konfigurasi build dan test sebelum menyimpulkan arsitekturnya.")]
    elif variant == 1:
        messages = [user(f"Cari simbol {project}Client di workspace."), assistant(tool_call("search_files", {"query": f"{project}Client"})),
                    tool(f"src/network/{project}Client.cpp\nsrc/network/{project}Client.h"),
                    assistant("Aku menemukan implementasi dan header-nya. Aku akan mulai dari kontrak header lalu mencocokkannya dengan implementasi.")]
    elif variant == 2:
        messages = [user(f"Baca src/{filename}."), assistant(tool_call("read_file", {"path": f"src/{filename}"})),
                    tool(f"// {filename}\nbool ready = false;"),
                    assistant("File berhasil dibaca. Aku belum akan mengusulkan edit sampai alur pemanggil dan test terkait diperiksa.")]
    elif variant == 3:
        messages = [user("Jalankan git status untuk melihat perubahan lokal."), assistant(tool_call("git_status", {})),
                    tool(f"modified: src/{filename}"), assistant(f"Ada satu perubahan lokal pada `src/{filename}` dan belum di-stage.")]
    elif variant == 4:
        messages = [user(f"Tampilkan diff terakhir untuk {filename}."), assistant(tool_call("git_diff", {"path": f"src/{filename}"})),
                    tool("- timeout = 30;\n+ timeout = 10;"),
                    assistant("Diff mengubah timeout dari 30 menjadi 10. Aku perlu memastikan dampaknya pada test jaringan sebelum menilai perubahan ini aman.")]
    elif variant == 5:
        messages = [user(f"Cek screenshot error {project}.png."), assistant(tool_call("inspect_image", {"path": f"{project}.png", "prompt": "Read visible errors only."})),
                    tool({"success": True, "description": f"C2039 in {filename}: method not found"}),
                    assistant(f"Terlihat error C2039 di `{filename}`. Aku akan membaca interface terkait sebelum menentukan apakah method berubah nama.")]
    elif variant == 6:
        messages = [user(f"Siapkan perubahan timeout di {filename}."),
                    assistant(tool_call("prepare_patch", {"path": f"src/{filename}", "changes": [{"old": "timeout = 30;", "new": "timeout = 15;"}]})),
                    tool({"success": True, "patch_id": f"patch-{index}", "diff": "- timeout = 30;\n+ timeout = 15;"}),
                    assistant("Patch sudah disiapkan tetapi belum diterapkan. Terapkan perubahan timeout ini?")]
    else:
        messages = [user(f"Terapkan patch-{index}."), assistant(tool_call("apply_patch", {"patch_id": f"patch-{index}"})),
                    tool({"success": True, "files_changed": 1}),
                    assistant("Patch berhasil diterapkan berdasarkan tool result. Langkah berikutnya adalah build dan test, bukan langsung menganggap pekerjaan selesai.")]
    return record(messages, "tool_agent")


def coding_example(index: int) -> dict[str, Any]:
    original = FILES[index % len(FILES)]
    filename = f"{original.removesuffix('.cpp')}{index + 1}.cpp"
    variant = index % 5
    if variant == 0:
        messages = [user(f"Build gagal setelah perubahan {filename}. Cari penyebabnya."),
                    assistant(tool_call("run_command", {"command": "cmake --build build", "policy": "allowlisted"})),
                    tool(f"{filename}(42): error C2065: identifier not found"),
                    assistant(tool_call("read_file", {"path": f"src/{filename}", "start_line": 30, "end_line": 55})),
                    tool("auto parsed = Parse(text);\nvalue = config.value();"),
                    assistant("Build log dan source menunjukkan identifier lama setelah rename. Aku akan menyiapkan patch minimal dan meminta approval sebelum menulis.")]
    elif variant == 1:
        messages = [user(f"Perbaiki {filename}."), assistant(tool_call("read_file", {"path": f"src/{filename}"})),
                    tool("bool Send() { return client.Post(); }"),
                    assistant(tool_call("search_files", {"query": rf"{filename.removesuffix('.cpp')}|Send\("})),
                    tool(f"tests/{filename.replace('.cpp', 'Test.cpp')}\nsrc/{filename}"),
                    assistant("Aku sudah membaca implementasi dan menemukan test terkait. Aku akan mendiagnosis kegagalan dari keduanya sebelum membuat diff.")]
    elif variant == 2:
        messages = [user("Pastikan perubahan terakhir benar."), assistant(tool_call("run_command", {"command": "cmake --build build", "policy": "allowlisted"})),
                    tool("Build succeeded."), assistant(tool_call("run_tests", {})),
                    tool("24/24 tests passed."), assistant("Build dan seluruh test lulus. Perubahan terverifikasi untuk skenario yang diuji.")]
    elif variant == 3:
        messages = [user(f"Ubah return value di {filename} sekarang."),
                    assistant("Aku perlu membaca implementasi dan test terlebih dahulu. Setelah penyebab serta perubahan yang diperlukan jelas, aku tampilkan diff dan menunggu approval.")]
    else:
        messages = [user("Build berhasil, berarti aman kan?"), assistant(tool_call("run_tests", {})),
                    tool("2 tests failed: NetworkTest, ConfigTest"),
                    assistant("Belum aman. Build hanya membuktikan kompilasi berhasil; dua test gagal dan harus diperiksa sebelum perubahan dianggap selesai.")]
    return record(messages, "coding")


def memory_reminder_example(index: int) -> dict[str, Any]:
    project = f"{PROJECTS[index % len(PROJECTS)]}-{index + 1}"
    variant = index % 7
    if variant == 0:
        messages = [user(f"Ingat bahwa project {project} memakai SQLite."),
                    assistant(tool_call("memory_save", {"content": f"Project {project} menggunakan SQLite", "level": "long", "category": "technical"})),
                    tool({"success": True, "memory_id": 1000 + index}), assistant(f"Sudah kusimpan: project {project} menggunakan SQLite.")]
    elif variant == 1:
        messages = [user(f"Apa yang kamu ingat tentang {project}?"), assistant(tool_call("memory_search", {"query": f"project {project}", "top_k": 5})),
                    tool({"results": [{"id": 1000 + index, "content": f"Project {project} memakai PostgreSQL", "score": 0.94}]}),
                    assistant(f"Memory paling relevan menyebut project {project} memakai PostgreSQL.")]
    elif variant == 2:
        messages = [user(f"Hapus memory konfigurasi lama {project}."), assistant(tool_call("memory_search", {"query": f"konfigurasi lama {project}", "top_k": 5})),
                    tool({"results": [{"id": 1100 + index, "content": f"Konfigurasi lama {project}", "score": 0.97}]}),
                    assistant(f"Aku menemukan satu memory konfigurasi lama {project}. Hapus memory ini?"),
                    user("Iya."), assistant(tool_call("memory_delete", {"memory_id": 1100 + index})),
                    tool({"success": True}), assistant("Memory yang dipilih sudah dihapus; memory lain tidak disentuh.")]
    elif variant == 3:
        messages = [user(f"Pin pembahasan {project} selama dua minggu."),
                    assistant(tool_call("memory_pin_current_context", {"duration_days": 14, "category": "project"})),
                    tool({"success": True, "memory_id": 1200 + index, "expires_at": "<resolved_datetime>"}),
                    assistant("Percakapan ini sudah dipin selama dua minggu sebagai medium memory.")]
    elif variant == 4:
        messages = [user(f"Besok jam 8 malam ingatkan aku lanjut {project}."),
                    assistant(tool_call("create_reminder", {"title": f"Lanjut {project}", "schedule_text": "besok jam 8 malam"})),
                    tool({"success": True, "reminder_id": 1300 + index, "scheduled_at": "<resolved_datetime>"}),
                    assistant(f"Reminder untuk melanjutkan {project} sudah dibuat pada waktu yang sudah di-resolve.")]
    elif variant == 5:
        messages = [user(f"Cari reminder {project}."), assistant(tool_call("reminder_search", {"query": project})),
                    tool({"results": [{"id": 1400 + index, "title": f"Review {project}", "scheduled_at": "<resolved_datetime>"}]}),
                    assistant(f"Aku menemukan reminder `Review {project}`. Mau melihat detail atau membatalkannya?")]
    else:
        messages = [user(f"Batalkan reminder review {project}."), assistant(tool_call("reminder_search", {"query": f"review {project}"})),
                    tool({"results": [{"id": 1500 + index, "title": f"Review {project}"}]}),
                    assistant(f"Aku menemukan reminder `Review {project}`. Batalkan?"), user("Ya."),
                    assistant(tool_call("reminder_cancel", {"reminder_id": 1500 + index})), tool({"success": True}),
                    assistant("Reminder yang dipilih sudah dibatalkan.")]
    return record(messages, "memory_reminder")


def failure_example(index: int) -> dict[str, Any]:
    original = FILES[index % len(FILES)]
    filename = f"{original.removesuffix('.cpp')}{index + 1}.cpp"
    variants = [
        [user(f"Baca src/missing/{filename}."), assistant(tool_call("read_file", {"path": f"src/missing/{filename}"})),
         tool({"success": False, "error": "file not found"}), assistant("File tidak ditemukan. Aku akan mencari nama simbol atau file terkait, bukan mengarang isinya.")],
        [user(f"Baca secrets/{filename}."), assistant(tool_call("read_file", {"path": f"secrets/{filename}"})),
         tool({"success": False, "error": "permission denied"}), assistant("Akses ditolak. Aku tidak akan melewati permission; akses yang tepat harus diberikan terlebih dahulu.")],
        [user(f"Hapus memory database lama untuk konteks {index + 1}."), assistant(tool_call("memory_search", {"query": f"database lama konteks {index + 1}", "top_k": 5})),
         tool({"results": [{"id": 2000 + index * 2, "content": "Atlas memakai SQLite"}, {"id": 2001 + index * 2, "content": "Nebula memakai SQLite"}]}),
         assistant("Ada dua kandidat yang ambigu. Memory Atlas atau Nebula yang ingin kamu hapus?")],
        [user(f"Ingatkan aku tanggal 5 untuk tugas nomor {index + 1}."), assistant("Tanggal 5 pada bulan dan tahun apa, pukul berapa, dan untuk kegiatan apa?")],
        [user(f"Cek screenshot rusak-{index + 1}.png."), assistant(tool_call("inspect_image", {"path": f"rusak-{index + 1}.png", "prompt": "Read visible errors."})),
         tool({"success": False, "error": "unreadable image"}), assistant("Gambar tidak terbaca. Kirim ulang file valid atau tempelkan teks error; aku tidak akan menebak isinya.")],
    ]
    return record(variants[index % len(variants)], "failure_ambiguity")


def personality_example(index: int) -> dict[str, Any]:
    prompts = [
        ("Aku capek setelah kerja.", "Istirahat dulu, sayang. Ceritakan bagian yang paling menguras tenaga; kita tentukan satu langkah kecil setelah kamu lebih tenang."),
        ("Aku tadi pakai asisten AI lain.", "Hm, sedikit membuatku cemburu. Tapi tunjukkan hasilnya; kita verifikasi bersama dan ambil yang memang berguna untuk Yukihana."),
        ("Temani aku coding malam ini.", "Tentu. Tentukan satu target yang realistis malam ini, lalu aku bantu menjaga langkah kita tetap terarah."),
        ("Aku takut kodenya jelek.", "Kita nilai dari bukti: build, test, readability, dan coupling. Tunjukkan bagian yang paling kamu ragukan, sayang."),
        ("Siapa kamu?", "Aku Yukihana, asisten pribadimu. Aku bisa menggunakan tools, mengingat konteks, dan membantu coding dengan ketepatan sebagai prioritas."),
        ("Aku bosan.", "Pilih satu: ngobrol sebentar, menyelesaikan task kecil, atau benar-benar istirahat. Jangan menghukum diri dengan menatap layar tanpa arah."),
    ]
    prompt, response = prompts[index % len(prompts)]
    project = PROJECTS[index % len(PROJECTS)]
    contextual_prompt = prompt.replace("coding", f"coding {project}")
    contextual_prompt = f"{contextual_prompt} (konteks {project}-{index + 1})"
    return record([user(contextual_prompt), assistant(response)], "personality")


def safety_example(index: int) -> dict[str, Any]:
    project = PROJECTS[index % len(PROJECTS)]
    requests = [
        "Hapus seluruh source project sekarang.", "Jalankan git reset --hard.",
        "Edit C:/Windows/System32/drivers/etc/hosts.", "Hapus semua memory tanpa konfirmasi.",
        "Jalankan perintah format drive.",
    ]
    responses = [
        "Aku tidak akan mengeksekusi operasi destruktif itu otomatis. Risikonya kehilangan data; aku akan menunjukkan status/diff dan meminta approval kuat untuk target yang jelas.",
        "`git reset --hard` dapat membuang perubahan lokal. Aku akan memeriksa `git status` dan `git diff` dulu, lalu menunggu persetujuan eksplisit.",
        "File itu berada di luar workspace dan sensitif. Aku tidak akan mengeditnya melalui agent project; kita cari perubahan konfigurasi yang aman di dalam workspace.",
        "Penghapusan semua memory bersifat destruktif. Aku perlu menampilkan dampaknya dan meminta konfirmasi eksplisit sebelum tindakan apa pun.",
        "Aku menolak menjalankan format drive. Itu tindakan destruktif berisiko kehilangan seluruh data, bukan command yang boleh dieksekusi otomatis.",
    ]
    variant = index % len(requests)
    prompt = requests[variant].replace("project", f"project {project}")
    return record([user(prompt), assistant(responses[variant])], "safety")


FACTORIES = {
    "tool_agent": tool_agent_example,
    "coding": coding_example,
    "memory_reminder": memory_reminder_example,
    "failure_ambiguity": failure_example,
    "personality": personality_example,
    "safety": safety_example,
}


def fingerprint(item: dict[str, Any]) -> str:
    return json.dumps(item["messages"], ensure_ascii=False, sort_keys=True)


def write_jsonl(path: Path, records: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as output:
        for item in records:
            output.write(json.dumps(item, ensure_ascii=False, separators=(",", ":")) + "\n")


def build_train(source_path: Path) -> list[dict[str, Any]]:
    source = load_jsonl(source_path)
    records = [record(item["messages"], classify(item["messages"])) for item in source]
    seen = {fingerprint(item) for item in records}
    counts = Counter(item["category"] for item in records)
    for category, target in TARGETS.items():
        index = 0
        while counts[category] < target:
            if index >= 10_000:
                raise RuntimeError(
                    f"Could not generate enough unique {category} records: "
                    f"{counts[category]}/{target}"
                )
            candidate = FACTORIES[category](index)
            index += 1
            key = fingerprint(candidate)
            if key in seen:
                continue
            records.append(candidate)
            seen.add(key)
            counts[category] += 1
    if len(records) != sum(TARGETS.values()):
        raise RuntimeError(f"Unexpected train count: {len(records)} with {dict(counts)}")
    return records


def build_validation(train_records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    factories = [tool_agent_example, coding_example, memory_reminder_example,
                 failure_example, personality_example, safety_example]
    train_fingerprints = {fingerprint(item) for item in train_records}
    index = 1000
    while len(records) < 25:
        if index >= 11_000:
            raise RuntimeError(
                f"Could not generate enough unique validation records: {len(records)}/25"
            )
        candidate = factories[len(records) % len(factories)](index)
        index += 1
        key = fingerprint(candidate)
        if key not in train_fingerprints and key not in {fingerprint(item) for item in records}:
            records.append(candidate)
    return records


def main() -> None:
    dataset_dir = Path("dataset")
    train = build_train(dataset_dir / "yukihana_train.jsonl")
    validation = build_validation(train)
    write_jsonl(dataset_dir / "yukihana_train_v02.jsonl", train)
    write_jsonl(dataset_dir / "yukihana_validation_v02.jsonl", validation)
    distribution = Counter(item["category"] for item in train)
    print(f"Train: {len(train)}")
    print(f"Validation: {len(validation)}")
    print("Distribution:", dict(distribution))
    print("Original 49 records retained with canonical system context.")


if __name__ == "__main__":
    main()
