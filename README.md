# AI Copilot Logger (X-Plane)

Plugin C++ yang merekam state pesawat + input pilot ke CSV (10 Hz).
Hasil: `X-Plane/Output/copilot_logs/flight_<ICAO>_<waktu>.csv`

## Build lewat GitHub
1. Buat repo baru, upload semua isi folder ini (termasuk `.github`).
2. Tab **Actions** -> workflow `build` jalan otomatis.
3. Selesai, download artifact `AICopilotLogger-semua-platform`.

## Pasang
Salin folder `AICopilotLogger` ke `X-Plane/Resources/plugins/`:

    AICopilotLogger/
      win_x64/AICopilotLogger.xpl
      profiles/standard.txt
      profiles/B738.txt

## Tambah pesawat
Buat `profiles/<ICAO>.txt` (A320, A339, B77W, C208, ...). Tidak perlu compile ulang.
