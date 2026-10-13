# Cleaner Windows Server 2008 R2 (High-Performance Win32 Edition)

![Platform](https://img.shields.io/badge/Platform-Windows%20Server%202008%20R2%2B-0078D6?style=flat-square)
![Architecture](https://img.shields.io/badge/Arch-Native%20x64%20(Win32%20C%2B%2B)-success?style=flat-square)
![Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(.NET%20Not%20Required)-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)

**Cleaner Windows Server 2008** is a mini, high-performance desktop utility (*Native Win32 x64*) engineered specifically for **Windows Server 2008 R2** (and compatible with Windows Server 2012/2016/2019/2022 and Windows 7/10/11).

It empowers Server Administrators to safely scan (**Scan per Drive**) and clean (**Cleaner**) server junk, temporary files, and log files that typically swell to tens of gigabytes (such as `C:\inetpub\logs\LogFiles` and `C:\Windows\Logs\CBS`) **without crawling unnecessary directories across the entire drive**.

---

## Why Native C++ / Win32 API? (High Performance)

On **Windows Server 2008 R2**, directories like `C:\inetpub\logs\LogFiles` or `C:\Windows\Temp` often accumulate **hundreds of thousands to millions of small files** totaling dozens of gigabytes. Heavy framework-based tools often freeze, hang, or exhaust system memory when loading large file lists into a data grid.

This application is built with **Native C++17 & Pure Win32 API (`_WIN32_WINNT = 0x0601`)**:
1. **Zero Runtime Dependency**: Uses only native system DLLs built into Windows Server 2008 R2 (`kernel32.dll`, `user32.dll`, `comctl32.dll`, `shell32.dll`, `msvcrt.dll`). No new .NET Framework, Java, or external runtime installation is required. Standalone executable size is only **~300 KB**.
2. **Kernel Large-Fetch Enumeration (`FindFirstFileExW`)**: Leverages `FindExInfoBasic` (bypasses legacy 8.3 short name queries) and `FIND_FIRST_EX_LARGE_FETCH` to query NTFS directory contents **3x–10x faster** with minimal disk I/O overhead.
3. **Virtual Data Grid (`LVS_OWNERDATA`)**: Employs a Win32 *Virtual ListView* backed by an in-memory data model, allowing it to display **500,000+ file rows** instantly with negligible memory footprint and smooth 60 FPS scrolling without UI lag.
4. **Native 64-bit (`x86_64`)**: Eliminates *WOW64 File System Redirection* pitfalls, enabling direct and accurate access to critical directories such as `C:\Windows\System32\LogFiles\HTTPERR` and `C:\Windows\Logs\CBS`.

---

## Key Features

- **Scan per Drive**: Detects and lists all available drives on the server (`C:\`, `D:\`, `E:\`, etc.) along with Volume Label, File System (`NTFS`/`FAT32`), Total Capacity, and **Free Space**.
- **Targeted Path Scanning (Fast & Safe)**: Never performs a slow full disk crawl; instead, it directly targets designated locations where server logs and junk files accumulate.
- **Preview in Table Grid Before Deletion**: Clicking **`SCAN`** parses and displays all matching junk/unused files in the **Table Grid** without deleting anything.
- **Safety Status Classification (`SAFE` vs `WARNING`)**:
  - **`SAFE` (Green)**: Archived logs, cache, or unused temp files (e.g., IIS/CBS logs older than 24 hours or temp files older than 1 hour) that are **100% safe to delete** (checked automatically).
  - **`WARNING` (Yellow/Orange)**: Recent log or temporary files modified today (<24 hours for logs / <1 hour for temp files) or recent dump files that may still be in use by running services (unchecked by default, with one-click selection via **`Select SAFE + WARNING`**).
- **Execution via `CLEANER`**:
  - Permanently removes all checked files from the Table Grid after user confirmation.
  - Files actively locked by Windows or IIS are skipped safely (**`LOCKED / SKIPPED`**) without interrupting the cleanup routine.
  - Automatically refreshes the **Free Space** statistics across all drives upon completion.
- **Administrator Utilities**:
  - **Click Column Headers to Sort** (defaults to largest file size first).
  - **Double-Click Any Row** to reveal and select the file directly in *Windows Explorer*.
  - **`+ Custom Folder...`** to add custom application log or temp folders (e.g., `D:\AppLogs`).
  - **`Export CSV`** to export scan reports for audit and compliance.

---

## Targeted Path Locations (Per Drive)

When you select target drive(s) and click **`SCAN`**, the engine resolves and scans the following locations (if present on that drive):

| Category | Targeted Path Pattern (`<Drive>:\...`) | Default Safety Level | Description |
| :--- | :--- | :--- | :--- |
| **IIS Web Server Logs** | `<Drive>:\inetpub\logs\LogFiles`<br>`<Drive>:\inetpub\logs\FailedReqLogFiles`<br>`<Drive>:\IISLogs`, `<Drive>:\Logs\IIS` | `SAFE` (>24h old)<br>`WARNING` (<24h old) | HTTP/FTP/SMTP IIS access logs (`W3SVC*`) that often consume dozens of GB. |
| **Windows & Drive Temp** | `<Drive>:\Windows\Temp`<br>`<Drive>:\Temp`, `<Drive>:\tmp` | `SAFE` (>1h old)<br>`WARNING` (<1h old) | Windows system and application temporary files. |
| **User Profiles Temp** | `<Drive>:\Users\*\AppData\Local\Temp`<br>`<Drive>:\Users\*\...\INetCache` | `SAFE` (>1h old)<br>`WARNING` (<1h old) | Automatically scans temp directories for all user and service accounts on the server. |
| **Windows CBS & Setup Logs** | `<Drive>:\Windows\Logs\CBS`<br>`<Drive>:\Windows\Logs\DISM`<br>`<Drive>:\Windows\Panther` | `SAFE` (`CbsPersist*` / >24h)<br>`WARNING` (Active `CBS.log`) | Resolves the notorious `CBS.log` / `CbsPersist` storage exhaustion on Server 2008 R2. |
| **HTTP.sys & System32 Logs** | `<Drive>:\Windows\System32\LogFiles\HTTPERR`<br>`<Drive>:\Windows\System32\LogFiles\W3SVC*`<br>`<Drive>:\PerfLogs` | `SAFE` (>24h old)<br>`WARNING` (<24h old) | HTTP.sys kernel error logs (`httperr*.log`) and system service logs. |
| **Windows Error Reporting** | `<Drive>:\ProgramData\Microsoft\Windows\WER`<br>`<Drive>:\Users\*\AppData\Local\Microsoft\Windows\WER` | `SAFE` | Application crash report archives (`ReportArchive` & `ReportQueue`). |
| **Memory & Crash Dumps** | `<Drive>:\Windows\MEMORY.DMP`<br>`<Drive>:\Windows\Minidump`<br>`<Drive>:\Users\*\AppData\Local\CrashDumps` | `SAFE` (>7d old)<br>`WARNING` (<7d old) | Crash memory dumps that can consume hundreds of megabytes to gigabytes. |
| **Windows Update Cache** | `<Drive>:\Windows\SoftwareDistribution\Download` | `SAFE` (>48h old)<br>`WARNING` (<48h old) | Downloaded Windows Update installation packages. |
| **IIS & ASP.NET Cache** | `<Drive>:\inetpub\temp\IIS Temporary Compressed Files`<br>`...\Framework*\v*\Temporary ASP.NET Files` | `SAFE` (>24h old)<br>`WARNING` (<24h old) | Static/dynamic compression cache and temporary ASP.NET compilations. |
| **Recycle Bin** | `<Drive>:\$Recycle.Bin`<br>`<Drive>:\RECYCLER` | `SAFE` | Files deleted to the Recycle Bin on each drive that still occupy disk storage. |
| **Prefetch & MSI Temp** | `<Drive>:\Windows\Prefetch`<br>`<Drive>:\Config.Msi` | `SAFE` | Application prefetch cache and installer rollback temporary data. |

> **Safety Measures**:
> - The scanner **never follows** NTFS Symbolic Links or Junctions (`FILE_ATTRIBUTE_REPARSE_POINT`), preventing escape from target directories.
> - The cleaner only removes files within the targeted directories and **never deletes** the root target folders (e.g., `C:\Windows\Temp` or `C:\inetpub\logs\LogFiles`).

---

## How to Use

1. Run **`CleanerWinServer2008.exe`** (Running as Administrator is recommended, or click **`Run as Admin`** inside the app to ensure unrestricted access to system directories).
2. Under **1. Select Server Drives**, check the drive(s) you wish to inspect (`C:\`, `D:\`, etc.).
3. Under **2. Targeted Server Junk & Log Locations**, verify that the desired categories are enabled.
4. Click **`SCAN`**.
5. All detected files will be populated into the **Table Grid** with:
   - `[x] Safety Status` (`SAFE` or `WARNING`)
   - `Drive`, `Category`, `Size`, `Last Modified`, `Full File Path`, and `Safety Info / Reason`.
6. Files marked **`SAFE`** are automatically checked. You can use **`Select SAFE Only`**, **`Select SAFE + WARNING`**, or toggle rows manually.
7. Click **`CLEANER (Delete)`** to purge all checked files.

---

## Building from Source

This project compiles cleanly with **MinGW-w64 (GCC)** on Windows.

### Using `build.bat` (Windows Command Prompt / PowerShell)
```cmd
build.bat
```

### Using `Makefile`
```bash
make
```

### Directory Structure
```text
cleanerwindowsserver2008/
├── assets/
│   └── app.ico              # Multi-resolution application icon
├── src/
│   ├── app.manifest         # Win32 manifest (Common Controls v6 & Server 2008 R2 compatibility)
│   ├── main.cpp             # Win32 GUI, Virtual ListView Grid, & Worker Threads
│   ├── resource.h           # Resource and control identifier definitions
│   ├── resource.rc          # Resource script (Icon, Manifest, VersionInfo Copyright)
│   ├── scanner.cpp          # Drive enumeration & targeted path resolver
│   └── scanner.h            # Data structures & safety classification headers
├── build.bat                # Build script for MinGW-w64 g++
├── Makefile                 # Standard Makefile
├── CleanerWinServer2008.exe # Prebuilt x64 standalone executable
├── LICENSE                  # MIT Open Source License
└── README.md                # Project documentation
```

---

## Copyright & License

Designed and developed by:

- **Author**: **Abraham Situmorang**
- **Organization**: **Nanoquity 2026**
- **Website**: **[www.nanoquity.com](https://www.nanoquity.com)**
- **License**: [MIT License](LICENSE)
