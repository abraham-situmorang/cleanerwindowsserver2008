# Cleaner Windows Server 2008 R2 (High-Performance Win32 Edition)

![Platform](https://img.shields.io/badge/Platform-Windows%20Server%202008%20R2%2B-0078D6?style=flat-square)
![Architecture](https://img.shields.io/badge/Arch-Native%20x64%20(Win32%20C%2B%2B)-success?style=flat-square)
![Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(.NET%20Not%20Required)-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)

**Cleaner Windows Server 2008** adalah aplikasi desktop mini berkinerja tinggi (*High-Performance Native Win32 x64*) yang dirancang khusus untuk **Windows Server 2008 R2** (serta kompatibel dengan Windows Server 2012/2016/2019/2022 dan Windows 7/10/11).

Program ini memudahkan Administrator Server untuk memindai (**Scan per Drive**) dan membersihkan (**Cleaner**) file sampah, temporary files, serta log server yang sering membengkak hingga puluhan Gigabyte (seperti `C:\inetpub\logs\LogFiles` dan `C:\Windows\Logs\CBS`) **tanpa perlu memindai seluruh isi hardisk yang tidak perlu**.

---

## Mengapa Menggunakan Arsitektur Native C++ / Win32 API? (High Performance)

Pada **Windows Server 2008 R2**, folder seperti `C:\inetpub\logs\LogFiles` atau `C:\Windows\Temp` sering kali berisi **ratusan ribu hingga jutaan file kecil** berukuran total puluhan GB. Aplikasi berbasis framework berat sering mengalami *hang/freeze* atau kehabisan RAM saat memuat ratusan ribu baris ke dalam tabel.

Program ini dibangun menggunakan **Native C++17 & Pure Win32 API (`_WIN32_WINNT = 0x0601`)**:
1. **Zero Framework Dependency**: Hanya menggunakan DLL bawaan asli Windows Server 2008 R2 (`kernel32.dll`, `user32.dll`, `comctl32.dll`, `shell32.dll`, `msvcrt.dll`). Tidak membutuhkan instalasi .NET Framework baru, Java, atau runtime tambahan. Ukuran file `.exe` mandiri hanya **~300 KB**.
2. **Kernel Large-Fetch Enumeration (`FindFirstFileExW`)**: Menggunakan flag `FindExInfoBasic` (melewati query nama 8.3 legacy) dan `FIND_FIRST_EX_LARGE_FETCH` untuk membaca direktori NTFS hingga **3x–10x lebih cepat** dengan beban I/O disk minimal.
3. **Virtual Data Grid (`LVS_OWNERDATA`)**: Menggunakan komponen *Virtual ListView* Win32 sehingga mampu menampilkan **500.000+ baris file** di tabel secara instan dengan penggunaan memori sangat kecil dan *scrolling* 60 FPS tanpa *lag*.
4. **Native 64-bit (`x86_64`)**: Bebas dari *WOW64 File System Redirection*, sehingga dapat mengakses `C:\Windows\System32\LogFiles\HTTPERR` dan `C:\Windows\Logs\CBS` secara langsung dan akurat.

---

## Fitur Utama

- **Scan per Drive**: Menampilkan semua Drive yang terpasang di server (`C:\`, `D:\`, `E:\`, dst.) lengkap dengan informasi Label, File System (`NTFS`/`FAT32`), Total Kapasitas, dan **Sisa Ruang Kosong (`Free Space`)**.
- **Targeted Path Scanning (Cepat & Aman)**: Tidak melakukan *crawl* ke seluruh folder aplikasi/database di drive, melainkan langsung menuju daftar path umum tempat berkumpulnya file sampah dan log server.
- **Preview di Table Grid Tanpa Menghapus Dulu**: Ketika tombol **`SCAN`** diklik, program hanya mendata dan menampilkan seluruh file sampah/tidak terpakai ke dalam **Table Grid** beserta ukurannya.
- **Indikator Keamanan (`SAFE` vs `WARNING`)**:
  - **`SAFE` (Hijau)**: File sampah, cache, atau log lama (misal log IIS/CBS yang berusia > 24 jam atau file temp > 1 jam) yang **100% aman dihapus** (otomatis tercentang).
  - **`WARNING` (Kuning/Oranye)**: File log atau temporary yang baru saja dimodifikasi hari ini (< 24 jam untuk log / < 1 jam untuk temp) atau file dump terbaru yang mungkin masih digunakan oleh layanan aktif (tidak dicentang secara default, namun dapat dipilih manual atau melalui tombol **`Select SAFE + WARNING`**).
- **Eksekusi Penghapusan Saat Klik `CLEANER`**:
  - Menghapus seluruh file yang tercentang di Table Grid setelah konfirmasi.
  - File yang sedang dikunci (*locked*) oleh proses Windows atau IIS aktif akan otomatis dilewati dengan aman (**`LOCKED / SKIPPED`**) tanpa menghentikan proses pembersihan.
  - Otomatis memperbarui informasi **Free Space** pada daftar Drive setelah pembersihan selesai.
- **Fitur Tambahan Administrator**:
  - **Klik Header Kolom untuk Sorting** (secara default diurutkan dari ukuran file terbesar ke terkecil).
  - **Double-Click Baris** untuk membuka lokasi file langsung di *Windows Explorer*.
  - **`+ Custom Folder...`** untuk menambahkan folder log aplikasi kustom (misal `D:\AppLogs`).
  - **`Export CSV`** untuk menyimpan laporan daftar file sampah sebelum/sesudah pembersihan.

---

## Daftar Path Umum yang Dipindai (Per Drive)

Saat Anda memilih Drive (misalnya `C:\` atau `D:\`) dan menekan **`SCAN`**, program akan memindai lokasi-lokasi berikut (jika tersedia di Drive tersebut):

| Kategori | Path Umum (`<Drive>:\...`) | Status Keamanan Default | Keterangan |
| :--- | :--- | :--- | :--- |
| **IIS Web Server Logs** | `<Drive>:\inetpub\logs\LogFiles`<br>`<Drive>:\inetpub\logs\FailedReqLogFiles`<br>`<Drive>:\IISLogs`, `<Drive>:\Logs\IIS` | `SAFE` (>24 jam)<br>`WARNING` (<24 jam) | Log akses HTTP/FTP/SMTP IIS (`W3SVC*`) yang sering membengkak puluhan GB. |
| **Windows & Drive Temp** | `<Drive>:\Windows\Temp`<br>`<Drive>:\Temp`, `<Drive>:\tmp` | `SAFE` (>1 jam)<br>`WARNING` (<1 jam) | File temporary sistem Windows dan aplikasi server. |
| **User Profiles Temp** | `<Drive>:\Users\*\AppData\Local\Temp`<br>`<Drive>:\Users\*\...\INetCache` | `SAFE` (>1 jam)<br>`WARNING` (<1 jam) | Otomatis memindai folder `Temp` milik seluruh akun User/Administrator di server. |
| **Windows CBS & Setup Logs** | `<Drive>:\Windows\Logs\CBS`<br>`<Drive>:\Windows\Logs\DISM`<br>`<Drive>:\Windows\Panther` | `SAFE` (`CbsPersist*` / >24j)<br>`WARNING` (`CBS.log` aktif) | Mengatasi masalah klasik `CBS.log` / `CbsPersist` yang memenuhi drive `C:` di Server 2008 R2. |
| **HTTP.sys & System32 Logs** | `<Drive>:\Windows\System32\LogFiles\HTTPERR`<br>`<Drive>:\Windows\System32\LogFiles\W3SVC*`<br>`<Drive>:\PerfLogs` | `SAFE` (>24 jam)<br>`WARNING` (<24 jam) | Log error kernel HTTP.sys (`httperr*.log`) dan log layanan sistem. |
| **Windows Error Reporting** | `<Drive>:\ProgramData\Microsoft\Windows\WER`<br>`<Drive>:\Users\*\AppData\Local\Microsoft\Windows\WER` | `SAFE` | Laporan crash aplikasi (`ReportArchive` & `ReportQueue`). |
| **Memory & Crash Dumps** | `<Drive>:\Windows\MEMORY.DMP`<br>`<Drive>:\Windows\Minidump`<br>`<Drive>:\Users\*\AppData\Local\CrashDumps` | `SAFE` (>7 hari)<br>`WARNING` (<7 hari) | File dump memori kernel/aplikasi yang dapat berukuran ratusan MB hingga beberapa GB. |
| **Windows Update Cache** | `<Drive>:\Windows\SoftwareDistribution\Download` | `SAFE` (>48 jam)<br>`WARNING` (<48 jam) | Paket instalasi Windows Update yang sudah selesai diunduh. |
| **IIS & ASP.NET Cache** | `<Drive>:\inetpub\temp\IIS Temporary Compressed Files`<br>`...\Framework*\v*\Temporary ASP.NET Files` | `SAFE` (>24 jam)<br>`WARNING` (<24 jam) | Cache kompresi statis/dinamis IIS dan hasil kompilasi sementara ASP.NET. |
| **Recycle Bin** | `<Drive>:\$Recycle.Bin`<br>`<Drive>:\RECYCLER` | `SAFE` | File yang sudah dihapus ke Recycle Bin di masing-masing Drive namun masih memakan ruang disk. |
| **Prefetch & MSI Temp** | `<Drive>:\Windows\Prefetch`<br>`<Drive>:\Config.Msi` | `SAFE` | Cache prefetch eksekusi aplikasi dan file rollback installer lama. |

> **Catatan Keamanan Ekstra**:
> - Program **tidak pernah** mengikuti *NTFS Symbolic Links / Junctions* (`FILE_ATTRIBUTE_REPARSE_POINT`) sehingga aman dari risiko keluar dari folder target.
> - Program hanya menghapus file di dalam folder target dan **tidak pernah** menghapus folder induknya (seperti `C:\Windows\Temp` atau `C:\inetpub\logs\LogFiles`).

---

## Cara Penggunaan

1. Jalankan **`CleanerWinServer2008.exe`** (Disarankan klik kanan -> **Run as administrator** atau klik tombol **`Run as Admin`** di dalam aplikasi agar memiliki akses penuh ke `C:\Windows\Temp`, `C:\Windows\Logs\CBS`, dan `C:\inetpub\logs\LogFiles`).
2. Pada panel **1. Pilih Drive Server**, centang Drive yang ingin dipindai (misal `C:\`, `D:\`, dst.).
3. Pada panel **2. Lokasi Path Umum**, pastikan kategori folder sampah/log yang diinginkan sudah tercentang.
4. Klik tombol **`SCAN`**.
5. Seluruh file yang ditemukan akan ditampilkan di **Table Grid** beserta kolom:
   - `[x] Safety Status` (`SAFE` atau `WARNING`)
   - `Drive`, `Category`, `Size`, `Last Modified`, `Full File Path`, dan `Safety Info / Keterangan`.
6. Secara otomatis, hanya file berstatus **`SAFE`** yang dicentang. Anda dapat menggunakan tombol **`Select SAFE Only`**, **`Select SAFE + WARNING`**, atau mencentang baris tertentu secara manual.
7. Klik tombol **`CLEANER (Delete)`** untuk menghapus seluruh file yang tercentang di Table Grid.

---

## Cara Build dari Source Code

Proyek ini dapat dikompilasi menggunakan **MinGW-w64 (GCC)** di Windows tanpa memerlukan instalasi IDE berat.

### Menggunakan `build.bat` (Windows CMD / PowerShell)
```bat
build.bat
```

### Menggunakan `Makefile`
```bash
make
```

### Struktur Direktori
```text
cleanerwindowsserver2008/
├── assets/
│   └── app.ico              # Ikon aplikasi multi-resolusi
├── src/
│   ├── app.manifest         # Manifest Win32 (Common Controls v6 & Win2008R2 compatibility)
│   ├── main.cpp             # GUI Win32, Virtual ListView Grid, & Worker Threads
│   ├── resource.h           # Definisi ID kontrol & resource
│   ├── resource.rc          # Resource script (Icon, Manifest, VersionInfo Copyright)
│   ├── scanner.cpp          # Engine enumerasi Drive & Targeted Path Resolver
│   └── scanner.h            # Header struktur data & klasifikasi keamanan
├── build.bat                # Script kompilasi instan (MinGW-w64 g++)
├── Makefile                 # Makefile standar
├── CleanerWinServer2008.exe # Binary x64 siap pakai
├── LICENSE                  # Lisensi MIT Open Source
└── README.md                # Dokumentasi repositori GitHub
```

---

## Copyright & Lisensi

Dirancang dan dikembangkan oleh:

- **Author**: **Abraham Situmorang**
- **Organization**: **Nanoquity 2026**
- **Website**: **[www.nanoquity.com](https://www.nanoquity.com)**
- **License**: [MIT License](LICENSE)
