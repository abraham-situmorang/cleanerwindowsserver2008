/*
 * Cleaner Windows Server 2008 - High-Performance Scanner & Cleaner Engine
 * Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026
 * Website: https://www.nanoquity.com
 * License: MIT Open Source
 */

#include "scanner.h"
#include <stdio.h>
#include <wchar.h>

void InitCategories(std::vector<CategoryInfo>& categories) {
    categories.clear();
    const CategoryInfo defaults[] = {
        { CAT_IIS_LOGS,         L"IIS Web Server Logs",       L"<Drive>:\\inetpub\\logs\\LogFiles, FailedReqLogFiles, IISLogs",          L"SAFE (>24h) / WARNING (<24h)", true },
        { CAT_WIN_TEMP,         L"Windows & Drive Temp",      L"<Drive>:\\Windows\\Temp, <Drive>:\\Temp, <Drive>:\\tmp",                 L"SAFE (>1h) / WARNING (<1h)",   true },
        { CAT_USER_TEMP,        L"User Profiles Temp",        L"<Drive>:\\Users\\*\\AppData\\Local\\Temp",                               L"SAFE (>1h) / WARNING (<1h)",   true },
        { CAT_CBS_DISM_LOGS,    L"Windows CBS & Setup Logs",  L"<Drive>:\\Windows\\Logs\\CBS, DISM, DPX, Panther",                       L"SAFE / WARNING (Active Log)",  true },
        { CAT_HTTPERR_SYS_LOGS, L"HTTP.sys & System32 Logs",  L"<Drive>:\\Windows\\System32\\LogFiles\\HTTPERR, W3SVC, PerfLogs",        L"SAFE (>24h) / WARNING (<24h)", true },
        { CAT_WER_REPORTS,      L"Windows Error Reporting",   L"<Drive>:\\ProgramData\\Microsoft\\Windows\\WER (Queue & Archive)",       L"SAFE",                         true },
        { CAT_CRASH_DUMPS,      L"Memory & Crash Dumps",      L"<Drive>:\\Windows\\Minidump, MEMORY.DMP, Users\\*\\CrashDumps",          L"SAFE (>7d) / WARNING (<7d)",   true },
        { CAT_WIN_UPDATE_CACHE, L"Windows Update Cache",      L"<Drive>:\\Windows\\SoftwareDistribution\\Download",                      L"SAFE (>48h) / WARNING (<48h)", true },
        { CAT_IIS_ASPNET_TEMP,  L"IIS & ASP.NET Cache",       L"inetpub\\temp\\IIS Temporary Compressed Files, Temporary ASP.NET Files", L"SAFE (>24h) / WARNING (<24h)", true },
        { CAT_RECYCLE_BIN,      L"Recycle Bin (Per Drive)",   L"<Drive>:\\$Recycle.Bin, <Drive>:\\RECYCLER",                             L"SAFE",                         true },
        { CAT_PREFETCH_MSI,     L"Prefetch & MSI Temp",       L"<Drive>:\\Windows\\Prefetch, <Drive>:\\Config.Msi",                      L"SAFE",                         true }
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
        categories.push_back(defaults[i]);
    }
}

const wchar_t* GetCategoryShortName(JunkCategoryId id) {
    switch (id) {
        case CAT_IIS_LOGS:         return L"IIS Logs";
        case CAT_WIN_TEMP:         return L"Windows/Drive Temp";
        case CAT_USER_TEMP:        return L"User Temp";
        case CAT_CBS_DISM_LOGS:    return L"CBS/Setup Logs";
        case CAT_HTTPERR_SYS_LOGS: return L"HTTPERR/System Logs";
        case CAT_WER_REPORTS:      return L"WER Crash Reports";
        case CAT_CRASH_DUMPS:      return L"Memory Dump";
        case CAT_WIN_UPDATE_CACHE: return L"Windows Update Cache";
        case CAT_IIS_ASPNET_TEMP:  return L"IIS/ASP.NET Cache";
        case CAT_RECYCLE_BIN:      return L"Recycle Bin";
        case CAT_PREFETCH_MSI:     return L"Prefetch/MSI Cache";
        case CAT_CUSTOM_PATH:      return L"Custom Folder";
        default:                   return L"Other Junk";
    }
}

const wchar_t* GetSafetyLevelText(SafetyLevel level) {
    switch (level) {
        case SAFETY_SAFE:    return L"SAFE";
        case SAFETY_WARNING: return L"WARNING";
        case SAFETY_DELETED: return L"DELETED";
        case SAFETY_SKIPPED: return L"LOCKED / SKIPPED";
        default:             return L"UNKNOWN";
    }
}

void FormatByteSize(uint64_t bytes, wchar_t* outBuf, size_t bufCount) {
    const double KB = 1024.0;
    const double MB = 1024.0 * 1024.0;
    const double GB = 1024.0 * 1024.0 * 1024.0;
    const double TB = 1024.0 * 1024.0 * 1024.0 * 1024.0;

    if (bytes >= (uint64_t)TB) {
        _snwprintf(outBuf, bufCount, L"%.2f TB", (double)bytes / TB);
    } else if (bytes >= (uint64_t)GB) {
        _snwprintf(outBuf, bufCount, L"%.2f GB", (double)bytes / GB);
    } else if (bytes >= (uint64_t)MB) {
        _snwprintf(outBuf, bufCount, L"%.2f MB", (double)bytes / MB);
    } else if (bytes >= (uint64_t)KB) {
        _snwprintf(outBuf, bufCount, L"%.1f KB", (double)bytes / KB);
    } else {
        _snwprintf(outBuf, bufCount, L"%llu B", (unsigned long long)bytes);
    }
    outBuf[bufCount - 1] = L'\0';
}

void FormatFileTimeLocal(const FILETIME& ft, wchar_t* outBuf, size_t bufCount) {
    FILETIME localFt;
    SYSTEMTIME st;
    if (FileTimeToLocalFileTime(&ft, &localFt) && FileTimeToSystemTime(&localFt, &st)) {
        _snwprintf(outBuf, bufCount, L"%04u-%02u-%02u %02u:%02u:%02u",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    } else {
        wcsncpy(outBuf, L"-", bufCount);
    }
    outBuf[bufCount - 1] = L'\0';
}

static bool DirectoryExistsW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool FileExistsOnlyW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void AddTargetIfDirectoryExists(
    const wchar_t* driveRoot,
    const std::wstring& relOrFull,
    bool isRelative,
    JunkCategoryId catId,
    std::vector<ResolvedTargetDir>& outTargets
) {
    std::wstring full = isRelative ? (std::wstring(driveRoot) + relOrFull) : relOrFull;
    if (DirectoryExistsW(full)) {
        /* Avoid duplicates */
        for (size_t i = 0; i < outTargets.size(); ++i) {
            if (_wcsicmp(outTargets[i].path.c_str(), full.c_str()) == 0) {
                return;
            }
        }
        ResolvedTargetDir td;
        td.path = full;
        wcsncpy(td.driveRoot, driveRoot, 3);
        td.driveRoot[3] = L'\0';
        td.categoryId = catId;
        td.isSingleFile = false;
        outTargets.push_back(td);
    }
}

static void AddTargetIfSingleFileExists(
    const wchar_t* driveRoot,
    const std::wstring& relPath,
    JunkCategoryId catId,
    std::vector<ResolvedTargetDir>& outTargets
) {
    std::wstring full = std::wstring(driveRoot) + relPath;
    if (FileExistsOnlyW(full)) {
        ResolvedTargetDir td;
        td.path = full;
        wcsncpy(td.driveRoot, driveRoot, 3);
        td.driveRoot[3] = L'\0';
        td.categoryId = catId;
        td.isSingleFile = true;
        outTargets.push_back(td);
    }
}

static void EnumerateUserSubfolders(
    const wchar_t* driveRoot,
    const std::wstring& usersBaseDir,
    const wchar_t* subPath,
    JunkCategoryId catId,
    std::vector<ResolvedTargetDir>& outTargets
) {
    std::wstring searchPattern = usersBaseDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(
        searchPattern.c_str(),
        FindExInfoBasic,
        &fd,
        FindExSearchNameMatch,
        NULL,
        FIND_FIRST_EX_LARGE_FETCH
    );
    if (hFind == INVALID_HANDLE_VALUE) {
        hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    }
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                std::wstring candidate = usersBaseDir + L"\\" + fd.cFileName + L"\\" + subPath;
                AddTargetIfDirectoryExists(driveRoot, candidate, false, catId, outTargets);
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

static bool IsCategoryEnabled(const std::vector<CategoryInfo>& categories, JunkCategoryId id) {
    for (size_t i = 0; i < categories.size(); ++i) {
        if (categories[i].id == id) return categories[i].enabled;
    }
    return true;
}

void ResolveDriveTargetPaths(
    const wchar_t* driveRoot,
    const std::vector<CategoryInfo>& categories,
    const std::vector<std::wstring>& customPaths,
    std::vector<ResolvedTargetDir>& outTargets
) {
    /* 1. IIS Web Server Logs (C:\inetpub\logs\LogFiles, FailedReqLogFiles, IISLogs) */
    if (IsCategoryEnabled(categories, CAT_IIS_LOGS)) {
        AddTargetIfDirectoryExists(driveRoot, L"inetpub\\logs\\LogFiles", true, CAT_IIS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"inetpub\\logs\\FailedReqLogFiles", true, CAT_IIS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"inetpub\\logs\\wmsvc", true, CAT_IIS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"IISLogs", true, CAT_IIS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Logs\\IIS", true, CAT_IIS_LOGS, outTargets);
    }

    /* 2. Windows & Drive Root Temp Folders */
    if (IsCategoryEnabled(categories, CAT_WIN_TEMP)) {
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Temp", true, CAT_WIN_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Temp", true, CAT_WIN_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"tmp", true, CAT_WIN_TEMP, outTargets);
    }

    /* 3. User Profiles Temp (<Drive>:\Users\*\AppData\Local\Temp) */
    std::wstring usersDir = std::wstring(driveRoot) + L"Users";
    std::wstring docSettingsDir = std::wstring(driveRoot) + L"Documents and Settings";
    if (IsCategoryEnabled(categories, CAT_USER_TEMP)) {
        if (DirectoryExistsW(usersDir)) {
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Temp", CAT_USER_TEMP, outTargets);
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files", CAT_USER_TEMP, outTargets);
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Microsoft\\Windows\\INetCache", CAT_USER_TEMP, outTargets);
        }
        if (DirectoryExistsW(docSettingsDir)) {
            EnumerateUserSubfolders(driveRoot, docSettingsDir, L"Local Settings\\Temp", CAT_USER_TEMP, outTargets);
        }
    }

    /* 4. Windows CBS, DISM & Setup Logs */
    if (IsCategoryEnabled(categories, CAT_CBS_DISM_LOGS)) {
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Logs\\CBS", true, CAT_CBS_DISM_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Logs\\DISM", true, CAT_CBS_DISM_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Logs\\DPX", true, CAT_CBS_DISM_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Panther", true, CAT_CBS_DISM_LOGS, outTargets);
        AddTargetIfSingleFileExists(driveRoot, L"Windows\\WindowsUpdate.log", CAT_CBS_DISM_LOGS, outTargets);
    }

    /* 5. HTTP.sys Kernel & System32 LogFiles */
    if (IsCategoryEnabled(categories, CAT_HTTPERR_SYS_LOGS)) {
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\HTTPERR", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\W3SVC", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\W3SVC1", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\SMTPSVC", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\SMTPSVC1", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\FTPSVC", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\System32\\LogFiles\\WMI\\RtBackup", true, CAT_HTTPERR_SYS_LOGS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"PerfLogs", true, CAT_HTTPERR_SYS_LOGS, outTargets);
    }

    /* 6. Windows Error Reporting (WER) */
    if (IsCategoryEnabled(categories, CAT_WER_REPORTS)) {
        AddTargetIfDirectoryExists(driveRoot, L"ProgramData\\Microsoft\\Windows\\WER\\ReportArchive", true, CAT_WER_REPORTS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"ProgramData\\Microsoft\\Windows\\WER\\ReportQueue", true, CAT_WER_REPORTS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"ProgramData\\Microsoft\\Windows\\WER\\Temp", true, CAT_WER_REPORTS, outTargets);
        if (DirectoryExistsW(usersDir)) {
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Microsoft\\Windows\\WER\\ReportArchive", CAT_WER_REPORTS, outTargets);
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Microsoft\\Windows\\WER\\ReportQueue", CAT_WER_REPORTS, outTargets);
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\Microsoft\\Windows\\WER\\Temp", CAT_WER_REPORTS, outTargets);
        }
    }

    /* 7. Memory Dumps & Application Crash Dumps */
    if (IsCategoryEnabled(categories, CAT_CRASH_DUMPS)) {
        AddTargetIfSingleFileExists(driveRoot, L"Windows\\MEMORY.DMP", CAT_CRASH_DUMPS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Minidump", true, CAT_CRASH_DUMPS, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\LiveKernelReports", true, CAT_CRASH_DUMPS, outTargets);
        if (DirectoryExistsW(usersDir)) {
            EnumerateUserSubfolders(driveRoot, usersDir, L"AppData\\Local\\CrashDumps", CAT_CRASH_DUMPS, outTargets);
        }
    }

    /* 8. Windows Update Download Cache */
    if (IsCategoryEnabled(categories, CAT_WIN_UPDATE_CACHE)) {
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\SoftwareDistribution\\Download", true, CAT_WIN_UPDATE_CACHE, outTargets);
    }

    /* 9. IIS Compression & ASP.NET Temporary Files */
    if (IsCategoryEnabled(categories, CAT_IIS_ASPNET_TEMP)) {
        AddTargetIfDirectoryExists(driveRoot, L"inetpub\\temp\\IIS Temporary Compressed Files", true, CAT_IIS_ASPNET_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Microsoft.NET\\Framework\\v2.0.50727\\Temporary ASP.NET Files", true, CAT_IIS_ASPNET_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Microsoft.NET\\Framework64\\v2.0.50727\\Temporary ASP.NET Files", true, CAT_IIS_ASPNET_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Microsoft.NET\\Framework\\v4.0.30319\\Temporary ASP.NET Files", true, CAT_IIS_ASPNET_TEMP, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\Temporary ASP.NET Files", true, CAT_IIS_ASPNET_TEMP, outTargets);
    }

    /* 10. Recycle Bin per Drive */
    if (IsCategoryEnabled(categories, CAT_RECYCLE_BIN)) {
        AddTargetIfDirectoryExists(driveRoot, L"$Recycle.Bin", true, CAT_RECYCLE_BIN, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"RECYCLER", true, CAT_RECYCLE_BIN, outTargets);
    }

    /* 11. Windows Prefetch & MSI Temp */
    if (IsCategoryEnabled(categories, CAT_PREFETCH_MSI)) {
        AddTargetIfDirectoryExists(driveRoot, L"Windows\\Prefetch", true, CAT_PREFETCH_MSI, outTargets);
        AddTargetIfDirectoryExists(driveRoot, L"Config.Msi", true, CAT_PREFETCH_MSI, outTargets);
    }

    /* 12. User Custom Paths matching this drive */
    for (size_t i = 0; i < customPaths.size(); ++i) {
        if (customPaths[i].size() >= 3 &&
            towupper(customPaths[i][0]) == towupper(driveRoot[0]) &&
            customPaths[i][1] == L':') {
            AddTargetIfDirectoryExists(driveRoot, customPaths[i], false, CAT_CUSTOM_PATH, outTargets);
        }
    }
}

void EnumerateServerDrives(std::vector<DriveEntry>& drives) {
    drives.clear();
    wchar_t buf[512] = { 0 };
    DWORD len = GetLogicalDriveStringsW(511, buf);
    if (len == 0) return;

    std::vector<CategoryInfo> allCats;
    InitCategories(allCats);
    std::vector<std::wstring> emptyCustom;

    const wchar_t* p = buf;
    while (*p) {
        UINT dtype = GetDriveTypeW(p);
        if (dtype == DRIVE_FIXED || dtype == DRIVE_REMOVABLE || dtype == DRIVE_RAMDISK) {
            DriveEntry de;
            ZeroMemory(&de, sizeof(de));
            wcsncpy(de.root, p, 7);
            de.driveType = dtype;

            ULARGE_INTEGER freeAvail, totalBytes, totalFree;
            if (GetDiskFreeSpaceExW(p, &freeAvail, &totalBytes, &totalFree)) {
                de.totalBytes = totalBytes.QuadPart;
                de.freeBytes = totalFree.QuadPart;
            }

            wchar_t volName[64] = { 0 };
            wchar_t fsName[32] = { 0 };
            if (GetVolumeInformationW(p, volName, 63, NULL, NULL, NULL, fsName, 31)) {
                wcsncpy(de.label, volName, 63);
                wcsncpy(de.fsName, fsName, 31);
            }
            if (de.label[0] == L'\0') {
                wcsncpy(de.label, (dtype == DRIVE_FIXED) ? L"Local Disk" : L"Removable Disk", 63);
            }

            std::vector<ResolvedTargetDir> targets;
            ResolveDriveTargetPaths(de.root, allCats, emptyCustom, targets);
            de.matchedTargetDirs = (int)targets.size();

            /* Select all fixed drives by default so user can scan per drive or all drives */
            de.selected = (dtype == DRIVE_FIXED);
            drives.push_back(de);
        }
        p += wcslen(p) + 1;
    }
}
