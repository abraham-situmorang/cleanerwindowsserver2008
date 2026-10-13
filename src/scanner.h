/*
 * Cleaner Windows Server 2008 - High-Performance Scanner & Cleaner Engine
 * Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026
 * Website: https://www.nanoquity.com
 * License: MIT Open Source
 */

#ifndef SCANNER_H
#define SCANNER_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Target Windows Server 2008 R2 / Windows 7 (NT 6.1) */
#endif

#include <windows.h>
#include <stdint.h>
#include <vector>
#include <string>

/* Safety Status Level for Scanned Files */
enum SafetyLevel {
    SAFETY_SAFE = 0,       /* 100% Safe to delete (old logs, unused temp, cache, WER, Recycle Bin) */
    SAFETY_WARNING = 1,    /* Needs attention (recent <24h logs, recent <1h temp, recent dump/WU cache) */
    SAFETY_DELETED = 2,    /* Successfully deleted by Cleaner */
    SAFETY_SKIPPED = 3     /* Locked by active Windows process / skipped during Clean */
};

/* Target Junk / Log Categories for Windows Server 2008 R2 */
enum JunkCategoryId {
    CAT_IIS_LOGS = 0,          /* C:\inetpub\logs\LogFiles, FailedReqLogFiles, IISLogs */
    CAT_WIN_TEMP = 1,          /* C:\Windows\Temp, <Drive>:\Temp, <Drive>:\tmp */
    CAT_USER_TEMP = 2,         /* <Drive>:\Users\*\AppData\Local\Temp */
    CAT_CBS_DISM_LOGS = 3,     /* C:\Windows\Logs\CBS, DISM, DPX, Panther */
    CAT_HTTPERR_SYS_LOGS = 4,  /* C:\Windows\System32\LogFiles\HTTPERR, W3SVC, PerfLogs */
    CAT_WER_REPORTS = 5,       /* ProgramData & Users Microsoft\Windows\WER */
    CAT_CRASH_DUMPS = 6,       /* C:\Windows\Minidump, MEMORY.DMP, CrashDumps */
    CAT_WIN_UPDATE_CACHE = 7,  /* C:\Windows\SoftwareDistribution\Download */
    CAT_IIS_ASPNET_TEMP = 8,   /* inetpub\temp\IIS Temporary Compressed Files, Temporary ASP.NET Files */
    CAT_RECYCLE_BIN = 9,       /* <Drive>:\$Recycle.Bin, RECYCLER */
    CAT_PREFETCH_MSI = 10,     /* C:\Windows\Prefetch, <Drive>:\Config.Msi */
    CAT_CUSTOM_PATH = 11,      /* User-added custom log/temp folder */
    CAT_COUNT = 12
};

struct CategoryInfo {
    JunkCategoryId id;
    const wchar_t* name;
    const wchar_t* patternDesc;
    const wchar_t* defaultSafetyLabel;
    bool enabled;
};

struct DriveEntry {
    wchar_t root[8];           /* e.g. L"C:\\" */
    wchar_t label[64];         /* Volume label */
    wchar_t fsName[32];        /* e.g. L"NTFS" */
    UINT driveType;
    uint64_t totalBytes;
    uint64_t freeBytes;
    int matchedTargetDirs;     /* Number of existing target junk folders on this drive */
    bool selected;
};

struct ScannedFileItem {
    std::wstring fullPath;
    wchar_t driveRoot[4];      /* e.g. L"C:\\" */
    JunkCategoryId categoryId;
    uint64_t fileSize;
    FILETIME lastWriteTime;
    SafetyLevel safety;
    const wchar_t* safetyReason;
    bool checked;
};

struct ScanSummary {
    uint64_t totalFiles;
    uint64_t totalBytes;
    uint64_t safeFiles;
    uint64_t safeBytes;
    uint64_t warningFiles;
    uint64_t warningBytes;
    uint64_t checkedFiles;
    uint64_t checkedBytes;
    uint64_t deletedFiles;
    uint64_t deletedBytes;
    uint64_t skippedFiles;
    DWORD elapsedMs;
};

/* Global Category Table */
void InitCategories(std::vector<CategoryInfo>& categories);
const wchar_t* GetCategoryShortName(JunkCategoryId id);
const wchar_t* GetSafetyLevelText(SafetyLevel level);

/* Drive Discovery */
void EnumerateServerDrives(std::vector<DriveEntry>& drives);

/* Formatting Utilities */
void FormatByteSize(uint64_t bytes, wchar_t* outBuf, size_t bufCount);
void FormatFileTimeLocal(const FILETIME& ft, wchar_t* outBuf, size_t bufCount);

/* Target Directory Resolution for a Specific Drive */
struct ResolvedTargetDir {
    std::wstring path;
    wchar_t driveRoot[4];
    JunkCategoryId categoryId;
    bool isSingleFile;
};

void ResolveDriveTargetPaths(
    const wchar_t* driveRoot,
    const std::vector<CategoryInfo>& categories,
    const std::vector<std::wstring>& customPaths,
    std::vector<ResolvedTargetDir>& outTargets
);

#endif /* SCANNER_H */
