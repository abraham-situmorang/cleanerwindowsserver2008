/*
 * Cleaner Windows Server 2008 (High-Performance Win32 Desktop Edition)
 * Targeted Server Log & Temp Junk Cleaner for Windows Server 2008 R2
 *
 * Copyright (c) 2026 Abraham Situmorang
 * Nanoquity 2026 | https://www.nanoquity.com
 * License: MIT Open Source
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows Server 2008 R2 / Windows 7 (NT 6.1) */
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <wchar.h>
#include <algorithm>
#include <vector>
#include <string>

#include "resource.h"
#include "scanner.h"

/* Global Application State */
static HINSTANCE g_hInst = NULL;
static HWND g_hMainWnd = NULL;
static HWND g_hListDrives = NULL;
static HWND g_hListCategories = NULL;
static HWND g_hListFiles = NULL;
static HWND g_hBtnScan = NULL;
static HWND g_hBtnCleaner = NULL;
static HWND g_hBtnStop = NULL;
static HWND g_hBtnSelSafe = NULL;
static HWND g_hBtnSelAll = NULL;
static HWND g_hBtnSelNone = NULL;
static HWND g_hBtnDrvAll = NULL;
static HWND g_hBtnDrvRefresh = NULL;
static HWND g_hBtnAddCustom = NULL;
static HWND g_hBtnExportCsv = NULL;
static HWND g_hBtnAbout = NULL;
static HWND g_hBtnWebsite = NULL;
static HWND g_hBtnElevate = NULL;
static HWND g_hProgress = NULL;
static HWND g_hLblSummary = NULL;
static HWND g_hLblStatus = NULL;
static HWND g_hLblCopyright = NULL;
static HWND g_hGrpDrives = NULL;
static HWND g_hGrpCategories = NULL;
static HWND g_hGrpFiles = NULL;

static HFONT g_hFontRegular = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontTitle = NULL;
static HIMAGELIST g_hStateImageList = NULL;

static std::vector<DriveEntry> g_drives;
static std::vector<CategoryInfo> g_categories;
static std::vector<std::wstring> g_customPaths;
static std::vector<ScannedFileItem> g_files;
static ScanSummary g_summary = { 0 };

static volatile LONG g_isWorking = 0;
static volatile LONG g_cancelRequested = 0;
static HANDLE g_hWorkerThread = NULL;

static int g_sortColumn = 3; /* Default sort by Size descending */
static bool g_sortAscending = false;

static CRITICAL_SECTION g_statusLock;
static wchar_t g_currentWorkPath[MAX_PATH] = { 0 };
static volatile LONG g_progressPercent = 0;

/* Check if current process has Administrator privileges */
static bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

/* Create 16x16 State ImageList (1 = Unchecked, 2 = Checked) for Virtual ListView */
static HIMAGELIST CreateCheckboxStateImageList(HWND hwnd) {
    HIMAGELIST hImgList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 3, 1);
    if (!hImgList) return NULL;

    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 16;
    bmi.bmiHeader.biHeight = -16; /* Top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    /* Index 0: dummy image (State image indices are 1-based) */
    for (int state = 0; state < 3; ++state) {
        void* pvBits = NULL;
        HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

        RECT rcAll = { 0, 0, 16, 16 };
        HBRUSH hBrBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdcMem, &rcAll, hBrBg);
        DeleteObject(hBrBg);

        RECT rcBox = { 2, 2, 14, 14 };
        HBRUSH hBrFill = CreateSolidBrush(state == 2 ? RGB(16, 110, 190) : RGB(255, 255, 255));
        FillRect(hdcMem, &rcBox, hBrFill);
        DeleteObject(hBrFill);

        HPEN hPenBorder = CreatePen(PS_SOLID, 1, state == 2 ? RGB(10, 82, 148) : RGB(120, 130, 140));
        HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPenBorder);
        SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
        Rectangle(hdcMem, 2, 2, 14, 14);
        SelectObject(hdcMem, hOldPen);
        DeleteObject(hPenBorder);

        if (state == 2) {
            /* Draw crisp white checkmark */
            HPEN hPenCheck = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            hOldPen = (HPEN)SelectObject(hdcMem, hPenCheck);
            MoveToEx(hdcMem, 4, 8, NULL);
            LineTo(hdcMem, 7, 11);
            LineTo(hdcMem, 12, 5);
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPenCheck);
        }

        SelectObject(hdcMem, hOldBmp);
        ImageList_Add(hImgList, hBmp, NULL);
        DeleteObject(hBmp);
    }

    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);
    return hImgList;
}

/* Recalculate summary statistics from g_files */
static void RecalculateSummary() {
    g_summary.totalFiles = g_files.size();
    g_summary.totalBytes = 0;
    g_summary.safeFiles = 0;
    g_summary.safeBytes = 0;
    g_summary.warningFiles = 0;
    g_summary.warningBytes = 0;
    g_summary.checkedFiles = 0;
    g_summary.checkedBytes = 0;

    for (size_t i = 0; i < g_files.size(); ++i) {
        const ScannedFileItem& item = g_files[i];
        g_summary.totalBytes += item.fileSize;
        if (item.safety == SAFETY_SAFE) {
            g_summary.safeFiles++;
            g_summary.safeBytes += item.fileSize;
        } else if (item.safety == SAFETY_WARNING) {
            g_summary.warningFiles++;
            g_summary.warningBytes += item.fileSize;
        }
        if (item.checked && (item.safety == SAFETY_SAFE || item.safety == SAFETY_WARNING)) {
            g_summary.checkedFiles++;
            g_summary.checkedBytes += item.fileSize;
        }
    }

    if (g_hLblSummary) {
        wchar_t szTotal[32], szSafe[32], szWarn[32], szChecked[32];
        FormatByteSize(g_summary.totalBytes, szTotal, 32);
        FormatByteSize(g_summary.safeBytes, szSafe, 32);
        FormatByteSize(g_summary.warningBytes, szWarn, 32);
        FormatByteSize(g_summary.checkedBytes, szChecked, 32);

        wchar_t msg[512];
        _snwprintf(msg, 512,
            L"Total Found: %llu files (%s)   |   SAFE: %llu (%s)   |   WARNING: %llu (%s)   |   Selected for Cleaner: %llu files (%s)",
            (unsigned long long)g_summary.totalFiles, szTotal,
            (unsigned long long)g_summary.safeFiles, szSafe,
            (unsigned long long)g_summary.warningFiles, szWarn,
            (unsigned long long)g_summary.checkedFiles, szChecked);
        msg[511] = L'\0';
        SetWindowTextW(g_hLblSummary, msg);
    }
}

/* Populate Drive ListView */
static void PopulateDriveListView() {
    EnumerateServerDrives(g_drives);
    if (!g_hListDrives) return;

    ListView_DeleteAllItems(g_hListDrives);
    for (size_t i = 0; i < g_drives.size(); ++i) {
        const DriveEntry& d = g_drives[i];
        wchar_t colDrive[96];
        _snwprintf(colDrive, 96, L"%s [%s]", d.root, d.label);

        LVITEMW lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = colDrive;
        ListView_InsertItem(g_hListDrives, &lvi);

        ListView_SetItemText(g_hListDrives, (int)i, 1, (LPWSTR)d.fsName);

        wchar_t szTotal[32], szFree[64];
        FormatByteSize(d.totalBytes, szTotal, 32);
        double pctFree = (d.totalBytes > 0) ? ((double)d.freeBytes * 100.0 / (double)d.totalBytes) : 0.0;
        wchar_t szFreeRaw[32];
        FormatByteSize(d.freeBytes, szFreeRaw, 32);
        _snwprintf(szFree, 64, L"%s (%.1f%%)", szFreeRaw, pctFree);

        ListView_SetItemText(g_hListDrives, (int)i, 2, szTotal);
        ListView_SetItemText(g_hListDrives, (int)i, 3, szFree);

        wchar_t szTargets[32];
        _snwprintf(szTargets, 32, L"%d paths", d.matchedTargetDirs);
        ListView_SetItemText(g_hListDrives, (int)i, 4, szTargets);

        ListView_SetCheckState(g_hListDrives, (int)i, d.selected ? TRUE : FALSE);
    }
}

/* Populate Categories ListView */
static void PopulateCategoryListView() {
    if (!g_hListCategories) return;
    ListView_DeleteAllItems(g_hListCategories);
    for (size_t i = 0; i < g_categories.size(); ++i) {
        const CategoryInfo& c = g_categories[i];
        LVITEMW lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)c.name;
        ListView_InsertItem(g_hListCategories, &lvi);
        ListView_SetItemText(g_hListCategories, (int)i, 1, (LPWSTR)c.defaultSafetyLabel);
        ListView_SetItemText(g_hListCategories, (int)i, 2, (LPWSTR)c.patternDesc);
        ListView_SetCheckState(g_hListCategories, (int)i, c.enabled ? TRUE : FALSE);
    }
}

/* Sync UI Checkboxes from Drive and Category ListViews before Scan */
static void SyncSelectionsFromUI() {
    for (size_t i = 0; i < g_drives.size(); ++i) {
        g_drives[i].selected = (ListView_GetCheckState(g_hListDrives, (int)i) != 0);
    }
    for (size_t i = 0; i < g_categories.size(); ++i) {
        g_categories[i].enabled = (ListView_GetCheckState(g_hListCategories, (int)i) != 0);
    }
}

/* Sort g_files by column */
static void SortScannedFiles() {
    int col = g_sortColumn;
    bool asc = g_sortAscending;

    std::sort(g_files.begin(), g_files.end(), [col, asc](const ScannedFileItem& a, const ScannedFileItem& b) {
        int cmp = 0;
        switch (col) {
            case 0: /* Safety */
                if (a.safety != b.safety) cmp = (a.safety < b.safety) ? -1 : 1;
                else cmp = (a.fileSize < b.fileSize) ? -1 : (a.fileSize > b.fileSize ? 1 : 0);
                break;
            case 1: /* Drive */
                cmp = _wcsicmp(a.driveRoot, b.driveRoot);
                break;
            case 2: /* Category */
                if (a.categoryId != b.categoryId) cmp = (a.categoryId < b.categoryId) ? -1 : 1;
                break;
            case 3: /* Size */
                if (a.fileSize != b.fileSize) cmp = (a.fileSize < b.fileSize) ? -1 : 1;
                break;
            case 4: /* Last Modified */
                cmp = CompareFileTime(&a.lastWriteTime, &b.lastWriteTime);
                break;
            case 5: /* Full Path */
                cmp = _wcsicmp(a.fullPath.c_str(), b.fullPath.c_str());
                break;
            case 6: /* Safety Reason */
                cmp = _wcsicmp(a.safetyReason, b.safetyReason);
                break;
        }
        return asc ? (cmp < 0) : (cmp > 0);
    });
}

/* Classify a file's SafetyLevel and detailed Safety Reason */
static void ClassifyFileSafety(
    JunkCategoryId catId,
    const wchar_t* fileName,
    const FILETIME& lastWriteTime,
    const ULARGE_INTEGER& nowUtc,
    SafetyLevel& outSafety,
    const wchar_t*& outReason
) {
    ULARGE_INTEGER fileUtc;
    fileUtc.LowPart = lastWriteTime.dwLowDateTime;
    fileUtc.HighPart = lastWriteTime.dwHighDateTime;

    /* 100-nanosecond intervals: 1 second = 10,000,000 */
    const uint64_t ONE_HOUR = 3600ULL * 10000000ULL;
    const uint64_t ONE_DAY  = 24ULL * ONE_HOUR;
    const uint64_t TWO_DAYS = 48ULL * ONE_HOUR;
    const uint64_t SEVEN_DAYS = 7ULL * ONE_DAY;

    uint64_t age = (nowUtc.QuadPart > fileUtc.QuadPart) ? (nowUtc.QuadPart - fileUtc.QuadPart) : 0;

    switch (catId) {
        case CAT_IIS_LOGS:
            if (age > ONE_DAY) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Old IIS Web Server log (>24h old, safe to delete)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Active/Today IIS log (<24h old, may be locked by IIS)";
            }
            break;

        case CAT_WIN_TEMP:
            if (age > ONE_HOUR) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Unused Windows/Drive temporary file (>1h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Recent system temp file (<1h old, may be in use)";
            }
            break;

        case CAT_USER_TEMP:
            if (age > ONE_HOUR) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Unused User Profile temporary file (>1h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Recent user temp file (<1h old, may be in use)";
            }
            break;

        case CAT_CBS_DISM_LOGS:
            if (_wcsicmp(fileName, L"CBS.log") == 0 && age <= ONE_DAY) {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Active today's CBS.log (<24h old)";
            } else {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Archived Windows CBS / Servicing / Setup log";
            }
            break;

        case CAT_HTTPERR_SYS_LOGS:
            if (age > ONE_DAY) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Old HTTP.sys / System32 service log (>24h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Recent HTTP.sys / System32 log (<24h old)";
            }
            break;

        case CAT_WER_REPORTS:
            outSafety = SAFETY_SAFE;
            outReason = L"SAFE - Windows Error Reporting (WER) crash report archive";
            break;

        case CAT_CRASH_DUMPS:
            if (age > SEVEN_DAYS) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Old crash memory dump (>7 days old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Recent crash memory dump (<7 days old, keep if debugging)";
            }
            break;

        case CAT_WIN_UPDATE_CACHE:
            if (age > TWO_DAYS) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Old Windows Update download package cache (>48h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Recent Windows Update cache (safe if not currently updating)";
            }
            break;

        case CAT_IIS_ASPNET_TEMP:
            if (age > ONE_DAY) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Unused IIS Compressed / ASP.NET cache (>24h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Active IIS / ASP.NET cache (<24h old, auto-regenerated)";
            }
            break;

        case CAT_RECYCLE_BIN:
            outSafety = SAFETY_SAFE;
            outReason = L"SAFE - Deleted item stored in Drive Recycle Bin";
            break;

        case CAT_PREFETCH_MSI:
            outSafety = SAFETY_SAFE;
            outReason = L"SAFE - Windows Prefetch / Installer cache file";
            break;

        case CAT_CUSTOM_PATH:
        default:
            if (age > ONE_DAY) {
                outSafety = SAFETY_SAFE;
                outReason = L"SAFE - Custom folder file (>24h old)";
            } else {
                outSafety = SAFETY_WARNING;
                outReason = L"WARNING - Custom folder file modified recently (<24h old)";
            }
            break;
    }
}

/* High-Speed Recursive Directory Scanner using FindFirstFileExW (FIND_FIRST_EX_LARGE_FETCH) */
static void ScanTargetDirectoryRecursive(
    const std::wstring& dirPath,
    const wchar_t* driveRoot,
    JunkCategoryId categoryId,
    const ULARGE_INTEGER& nowUtc,
    std::vector<ScannedFileItem>& outList,
    DWORD& lastNotifyTick
) {
    if (InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0) return;

    DWORD nowTick = GetTickCount();
    if (nowTick - lastNotifyTick >= 80) {
        lastNotifyTick = nowTick;
        EnterCriticalSection(&g_statusLock);
        wcsncpy(g_currentWorkPath, dirPath.c_str(), MAX_PATH - 1);
        g_currentWorkPath[MAX_PATH - 1] = L'\0';
        LeaveCriticalSection(&g_statusLock);
        if (g_hMainWnd) {
            PostMessageW(g_hMainWnd, WM_WORKER_PROGRESS, (WPARAM)outList.size(), 0);
        }
    }

    std::wstring pattern = dirPath + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(
        pattern.c_str(),
        FindExInfoBasic,
        &fd,
        FindExSearchNameMatch,
        NULL,
        FIND_FIRST_EX_LARGE_FETCH
    );
    if (hFind == INVALID_HANDLE_VALUE) {
        hFind = FindFirstFileW(pattern.c_str(), &fd);
    }
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subDirs;

    do {
        if (InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0) break;

        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        /* Never traverse NTFS symlinks / junctions / mount points to avoid escaping target folder */
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            subDirs.push_back(dirPath + L"\\" + fd.cFileName);
        } else {
            /* Skip desktop.ini so shell folder icons (e.g. Recycle Bin) stay intact */
            if (_wcsicmp(fd.cFileName, L"desktop.ini") == 0) {
                continue;
            }

            uint64_t fsize = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;

            ScannedFileItem item;
            item.fullPath = dirPath + L"\\" + fd.cFileName;
            wcsncpy(item.driveRoot, driveRoot, 3);
            item.driveRoot[3] = L'\0';
            item.categoryId = categoryId;
            item.fileSize = fsize;
            item.lastWriteTime = fd.ftLastWriteTime;

            ClassifyFileSafety(categoryId, fd.cFileName, fd.ftLastWriteTime, nowUtc, item.safety, item.safetyReason);
            /* Auto-check SAFE items; leave WARNING unchecked until user decides */
            item.checked = (item.safety == SAFETY_SAFE);

            outList.push_back(item);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    for (size_t i = 0; i < subDirs.size(); ++i) {
        if (InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0) break;
        ScanTargetDirectoryRecursive(subDirs[i], driveRoot, categoryId, nowUtc, outList, lastNotifyTick);
    }
}

/* Worker Thread: Targeted High-Speed Scan per Selected Drive */
static DWORD WINAPI ScanWorkerThreadProc(LPVOID lpParam) {
    (void)lpParam;
    DWORD startTick = GetTickCount();

    std::vector<ResolvedTargetDir> allTargets;
    for (size_t d = 0; d < g_drives.size(); ++d) {
        if (!g_drives[d].selected) continue;
        ResolveDriveTargetPaths(g_drives[d].root, g_categories, g_customPaths, allTargets);
    }

    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER nowUtc;
    nowUtc.LowPart = ftNow.dwLowDateTime;
    nowUtc.HighPart = ftNow.dwHighDateTime;

    std::vector<ScannedFileItem> localFiles;
    localFiles.reserve(16384);

    DWORD lastNotifyTick = GetTickCount();
    size_t totalTargets = allTargets.size();

    for (size_t t = 0; t < totalTargets; ++t) {
        if (InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0) break;

        int pct = (totalTargets > 0) ? (int)((t * 100) / totalTargets) : 0;
        InterlockedExchange(&g_progressPercent, pct);

        const ResolvedTargetDir& target = allTargets[t];
        if (target.isSingleFile) {
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(target.path.c_str(), GetFileExInfoStandard, &fad)) {
                ScannedFileItem item;
                item.fullPath = target.path;
                wcsncpy(item.driveRoot, target.driveRoot, 3);
                item.driveRoot[3] = L'\0';
                item.categoryId = target.categoryId;
                item.fileSize = ((uint64_t)fad.nFileSizeHigh << 32) | (uint64_t)fad.nFileSizeLow;
                item.lastWriteTime = fad.ftLastWriteTime;
                const wchar_t* fname = PathFindFileNameW(target.path.c_str());
                ClassifyFileSafety(target.categoryId, fname, fad.ftLastWriteTime, nowUtc, item.safety, item.safetyReason);
                item.checked = (item.safety == SAFETY_SAFE);
                localFiles.push_back(item);
            }
        } else {
            ScanTargetDirectoryRecursive(
                target.path,
                target.driveRoot,
                target.categoryId,
                nowUtc,
                localFiles,
                lastNotifyTick
            );
        }
    }

    InterlockedExchange(&g_progressPercent, 100);
    g_files.swap(localFiles);
    SortScannedFiles();
    g_summary.elapsedMs = GetTickCount() - startTick;

    if (g_hMainWnd) {
        PostMessageW(g_hMainWnd, WM_WORKER_SCAN_DONE, 0, 0);
    }
    return 0;
}

/* Remove empty subdirectories inside a target folder (never removes rootTargetDir itself) */
static void RemoveEmptySubdirectories(const std::wstring& dirPath, bool isRootTarget) {
    std::wstring pattern = dirPath + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::wstring sub = dirPath + L"\\" + fd.cFileName;
            RemoveEmptySubdirectories(sub, false);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    if (!isRootTarget) {
        RemoveDirectoryW(dirPath.c_str());
    }
}

/* Worker Thread: Cleaner (Deletes checked files in the table grid) */
static DWORD WINAPI CleanWorkerThreadProc(LPVOID lpParam) {
    (void)lpParam;
    DWORD startTick = GetTickCount();

    uint64_t deletedCount = 0;
    uint64_t deletedBytes = 0;
    uint64_t skippedCount = 0;

    size_t totalToProcess = 0;
    for (size_t i = 0; i < g_files.size(); ++i) {
        if (g_files[i].checked && (g_files[i].safety == SAFETY_SAFE || g_files[i].safety == SAFETY_WARNING)) {
            totalToProcess++;
        }
    }

    size_t processed = 0;
    DWORD lastNotifyTick = GetTickCount();

    for (size_t i = 0; i < g_files.size(); ++i) {
        if (InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0) break;

        ScannedFileItem& item = g_files[i];
        if (!item.checked || (item.safety != SAFETY_SAFE && item.safety != SAFETY_WARNING)) {
            continue;
        }

        processed++;
        int pct = (totalToProcess > 0) ? (int)((processed * 100) / totalToProcess) : 100;
        InterlockedExchange(&g_progressPercent, pct);

        DWORD nowTick = GetTickCount();
        if (nowTick - lastNotifyTick >= 80) {
            lastNotifyTick = nowTick;
            EnterCriticalSection(&g_statusLock);
            wcsncpy(g_currentWorkPath, item.fullPath.c_str(), MAX_PATH - 1);
            g_currentWorkPath[MAX_PATH - 1] = L'\0';
            LeaveCriticalSection(&g_statusLock);
            if (g_hMainWnd) {
                PostMessageW(g_hMainWnd, WM_WORKER_PROGRESS, (WPARAM)deletedCount, 1);
            }
        }

        /* Clear Read-Only / System attribute if set on temp or old log file */
        DWORD attr = GetFileAttributesW(item.fullPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
            SetFileAttributesW(item.fullPath.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
        }

        if (DeleteFileW(item.fullPath.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) {
            item.safety = SAFETY_DELETED;
            item.safetyReason = L"DELETED - File successfully removed";
            item.checked = false;
            deletedCount++;
            deletedBytes += item.fileSize;
        } else {
            item.safety = SAFETY_SKIPPED;
            item.safetyReason = L"LOCKED / SKIPPED - In use by active Windows service or process";
            skippedCount++;
        }
    }

    /* Clean up empty subfolders inside Temp folders (never deleting the root Temp folder itself) */
    for (size_t d = 0; d < g_drives.size(); ++d) {
        if (!g_drives[d].selected) continue;
        std::vector<ResolvedTargetDir> targets;
        ResolveDriveTargetPaths(g_drives[d].root, g_categories, g_customPaths, targets);
        for (size_t t = 0; t < targets.size(); ++t) {
            if (!targets[t].isSingleFile &&
                (targets[t].categoryId == CAT_WIN_TEMP ||
                 targets[t].categoryId == CAT_USER_TEMP ||
                 targets[t].categoryId == CAT_WER_REPORTS ||
                 targets[t].categoryId == CAT_WIN_UPDATE_CACHE)) {
                RemoveEmptySubdirectories(targets[t].path, true);
            }
        }
    }

    InterlockedExchange(&g_progressPercent, 100);
    g_summary.deletedFiles = deletedCount;
    g_summary.deletedBytes = deletedBytes;
    g_summary.skippedFiles = skippedCount;
    g_summary.elapsedMs = GetTickCount() - startTick;

    if (g_hMainWnd) {
        PostMessageW(g_hMainWnd, WM_WORKER_CLEAN_DONE, 0, 0);
    }
    return 0;
}

/* Enable or disable UI buttons while worker thread is active */
static void SetControlsBusyState(bool busy) {
    EnableWindow(g_hBtnScan, !busy);
    EnableWindow(g_hBtnCleaner, !busy && !g_files.empty());
    EnableWindow(g_hBtnStop, busy);
    EnableWindow(g_hBtnSelSafe, !busy && !g_files.empty());
    EnableWindow(g_hBtnSelAll, !busy && !g_files.empty());
    EnableWindow(g_hBtnSelNone, !busy && !g_files.empty());
    EnableWindow(g_hBtnDrvAll, !busy);
    EnableWindow(g_hBtnDrvRefresh, !busy);
    EnableWindow(g_hBtnAddCustom, !busy);
    EnableWindow(g_hBtnExportCsv, !busy && !g_files.empty());
    EnableWindow(g_hListDrives, !busy);
    EnableWindow(g_hListCategories, !busy);
}

/* Launch Scan Operation */
static void StartScanOperation(HWND hwnd) {
    if (InterlockedCompareExchange(&g_isWorking, 1, 0) != 0) return;

    SyncSelectionsFromUI();

    bool anyDrive = false;
    for (size_t i = 0; i < g_drives.size(); ++i) {
        if (g_drives[i].selected) {
            anyDrive = true;
            break;
        }
    }
    if (!anyDrive) {
        InterlockedExchange(&g_isWorking, 0);
        MessageBoxW(hwnd,
            L"Please select at least one Drive from the list above to scan.",
            L"Cleaner Windows Server 2008",
            MB_OK | MB_ICONWARNING);
        return;
    }

    InterlockedExchange(&g_cancelRequested, 0);
    InterlockedExchange(&g_progressPercent, 0);
    SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);

    ListView_SetItemCountEx(g_hListFiles, 0, 0);
    g_files.clear();
    SetControlsBusyState(true);
    SetWindowTextW(g_hLblStatus, L"Scanning targeted server log & temp paths...");

    if (g_hWorkerThread) {
        CloseHandle(g_hWorkerThread);
        g_hWorkerThread = NULL;
    }
    g_hWorkerThread = CreateThread(NULL, 0, ScanWorkerThreadProc, NULL, 0, NULL);
}

/* Launch Cleaner Operation */
static void StartCleanerOperation(HWND hwnd) {
    if (InterlockedCompareExchange(&g_isWorking, 0, 0) != 0) return;

    RecalculateSummary();
    if (g_summary.checkedFiles == 0) {
        MessageBoxW(hwnd,
            L"No files are selected in the table grid.\n\n"
            L"Use 'Select SAFE Only' or check the files you wish to delete first.",
            L"Cleaner Windows Server 2008",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    /* Count how many checked items are SAFE vs WARNING */
    uint64_t checkedSafe = 0, checkedWarn = 0;
    for (size_t i = 0; i < g_files.size(); ++i) {
        if (g_files[i].checked) {
            if (g_files[i].safety == SAFETY_SAFE) checkedSafe++;
            else if (g_files[i].safety == SAFETY_WARNING) checkedWarn++;
        }
    }

    wchar_t szCheckedBytes[32];
    FormatByteSize(g_summary.checkedBytes, szCheckedBytes, 32);

    wchar_t confirmMsg[1024];
    _snwprintf(confirmMsg, 1024,
        L"Confirm Deletion of Server Junk & Log Files:\n\n"
        L"  - Total Files Checked : %llu files\n"
        L"  - Total Space to Free : %s\n"
        L"  - SAFE Items          : %llu files\n"
        L"  - WARNING Items       : %llu files\n\n"
        L"Safety Note:\n"
        L"Files actively locked or in use by Windows services/IIS will be skipped automatically and safely.\n\n"
        L"Proceed with CLEANER operation now?",
        (unsigned long long)g_summary.checkedFiles,
        szCheckedBytes,
        (unsigned long long)checkedSafe,
        (unsigned long long)checkedWarn);
    confirmMsg[1023] = L'\0';

    if (MessageBoxW(hwnd, confirmMsg, L"Cleaner Confirmation - Windows Server 2008",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    InterlockedExchange(&g_isWorking, 1);
    InterlockedExchange(&g_cancelRequested, 0);
    InterlockedExchange(&g_progressPercent, 0);
    SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);

    SetControlsBusyState(true);
    SetWindowTextW(g_hLblStatus, L"Cleaning checked files from server drives...");

    if (g_hWorkerThread) {
        CloseHandle(g_hWorkerThread);
        g_hWorkerThread = NULL;
    }
    g_hWorkerThread = CreateThread(NULL, 0, CleanWorkerThreadProc, NULL, 0, NULL);
}

/* Add Custom Folder Dialog (SHBrowseForFolderW - supported on Windows Server 2008 R2) */
static void AddCustomFolderDialog(HWND hwnd) {
    wchar_t folderPath[MAX_PATH] = { 0 };
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.pszDisplayName = folderPath;
    bi.lpszTitle = L"Select Custom Log or Temp Folder to Scan (e.g. D:\\AppLogs):";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListW(pidl, folderPath)) {
            /* Prevent adding root drive or C:\Windows directly */
            if (wcslen(folderPath) <= 3 ||
                _wcsicmp(folderPath, L"C:\\Windows") == 0 ||
                _wcsicmp(folderPath, L"C:\\Windows\\System32") == 0 ||
                _wcsicmp(folderPath, L"C:\\Program Files") == 0 ||
                _wcsicmp(folderPath, L"C:\\Program Files (x86)") == 0) {
                MessageBoxW(hwnd,
                    L"For server safety, root drives and critical system directories cannot be added directly.\n"
                    L"Please choose a specific subfolder such as D:\\Logs or C:\\AppTemp.",
                    L"Path Security Warning",
                    MB_OK | MB_ICONWARNING);
            } else {
                g_customPaths.push_back(folderPath);
                wchar_t info[512];
                _snwprintf(info, 512,
                    L"Custom folder added to scan targets successfully:\n%s\n\nClick 'SCAN' to start scanning.",
                    folderPath);
                MessageBoxW(hwnd, info, L"Custom Path Added", MB_OK | MB_ICONINFORMATION);
            }
        }
        CoTaskMemFree(pidl);
    }
}

/* Export Scanned Results to CSV */
static void ExportResultsToCsv(HWND hwnd) {
    if (g_files.empty()) return;

    wchar_t filePath[MAX_PATH] = L"ServerCleanerReport.csv";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV Report (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"csv";

    if (!GetSaveFileNameW(&ofn)) return;

    FILE* fp = _wfopen(filePath, L"wb");
    if (!fp) {
        MessageBoxW(hwnd, L"Failed to save CSV file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    /* Write UTF-8 BOM */
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, fp);
    fprintf(fp, "Checked,Safety,Drive,Category,SizeBytes,SizeFormatted,LastModified,FullPath,SafetyReason\r\n");

    for (size_t i = 0; i < g_files.size(); ++i) {
        const ScannedFileItem& item = g_files[i];
        wchar_t szSize[32], szDate[32];
        FormatByteSize(item.fileSize, szSize, 32);
        FormatFileTimeLocal(item.lastWriteTime, szDate, 32);

        char u8Path[1024] = { 0 }, u8Cat[128] = { 0 }, u8Size[64] = { 0 }, u8Date[64] = { 0 }, u8Reason[256] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, item.fullPath.c_str(), -1, u8Path, sizeof(u8Path) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, GetCategoryShortName(item.categoryId), -1, u8Cat, sizeof(u8Cat) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, szSize, -1, u8Size, sizeof(u8Size) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, szDate, -1, u8Date, sizeof(u8Date) - 1, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, item.safetyReason, -1, u8Reason, sizeof(u8Reason) - 1, NULL, NULL);

        fprintf(fp, "%s,%ls,%ls,\"%s\",%llu,\"%s\",\"%s\",\"%s\",\"%s\"\r\n",
            item.checked ? "YES" : "NO",
            GetSafetyLevelText(item.safety),
            item.driveRoot,
            u8Cat,
            (unsigned long long)item.fileSize,
            u8Size,
            u8Date,
            u8Path,
            u8Reason);
    }

    fclose(fp);
    MessageBoxW(hwnd, L"Scan results exported to CSV report successfully.", L"Export CSV Successful", MB_OK | MB_ICONINFORMATION);
}

/* Open selected file in Windows Explorer */
static void OpenItemInExplorer(int index) {
    if (index < 0 || (size_t)index >= g_files.size()) return;
    std::wstring param = L"/select,\"" + g_files[index].fullPath + L"\"";
    ShellExecuteW(g_hMainWnd, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
}

/* Toggle Checkbox for Virtual ListView Item(s) */
static void ToggleVirtualItemCheck(int clickedIndex) {
    if (clickedIndex < 0 || (size_t)clickedIndex >= g_files.size()) return;
    if (InterlockedCompareExchange(&g_isWorking, 0, 0) != 0) return;

    bool newState = !g_files[clickedIndex].checked;

    /* If multiple items are selected and clickedIndex is one of them, toggle all selected */
    UINT selCount = ListView_GetSelectedCount(g_hListFiles);
    if (selCount > 1 && ListView_GetItemState(g_hListFiles, clickedIndex, LVIS_SELECTED) != 0) {
        int idx = -1;
        while ((idx = ListView_GetNextItem(g_hListFiles, idx, LVNI_SELECTED)) != -1) {
            if ((size_t)idx < g_files.size() &&
                (g_files[idx].safety == SAFETY_SAFE || g_files[idx].safety == SAFETY_WARNING)) {
                g_files[idx].checked = newState;
                ListView_RedrawItems(g_hListFiles, idx, idx);
            }
        }
    } else {
        if (g_files[clickedIndex].safety == SAFETY_SAFE || g_files[clickedIndex].safety == SAFETY_WARNING) {
            g_files[clickedIndex].checked = newState;
            ListView_RedrawItems(g_hListFiles, clickedIndex, clickedIndex);
        }
    }

    UpdateWindow(g_hListFiles);
    RecalculateSummary();
}

/* Show About & Copyright Dialog */
static void ShowAboutDialog(HWND hwnd) {
    const wchar_t* aboutText =
        L"Cleaner Windows Server 2008 (v1.0.0)\n"
        L"High-Performance Native x64 Server Log & Junk Cleaner\n"
        L"Designed for Windows Server 2008 R2 & Later\n\n"
        L"Key Features:\n"
        L"  - Scan per Drive (Displays all drives, volume labels & free space)\n"
        L"  - Targeted Path Scan (Fast scan of common log/temp locations without slow full-disk crawl)\n"
        L"  - Automatically detects IIS Logs (C:\\inetpub\\logs\\LogFiles),\n"
        L"    Windows Temp, User Profiles Temp, CBS Logs, HTTPERR, WER,\n"
        L"    Memory Dumps, Windows Update Cache, and Recycle Bin\n"
        L"  - Automatic Safety Classification: SAFE vs WARNING\n"
        L"  - High-Performance Virtual Grid (>500,000 files supported\n"
        L"    without UI lag using Win32 FindFirstFileExW & LVS_OWNERDATA)\n\n"
        L"------------------------------------------------------------\n"
        L"Copyright (c) 2026 Abraham Situmorang\n"
        L"Nanoquity 2026\n"
        L"Website: www.nanoquity.com\n"
        L"License: MIT Open Source";

    MessageBoxW(hwnd, aboutText, L"About - Cleaner Windows Server 2008", MB_OK | MB_ICONINFORMATION);
}

/* Relaunch as Administrator if not elevated */
static void ElevateProcess(HWND hwnd) {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    HINSTANCE res = ShellExecuteW(hwnd, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res > 32) {
        PostQuitMessage(0);
    }
}

/* Resize and layout all child controls cleanly */
static void LayoutControls(int cx, int cy) {
    if (cx < 780) cx = 780;
    if (cy < 520) cy = 520;

    const int margin = 10;
    const int topPanelH = 182;
    const int driveBoxW = (cx - margin * 3) * 44 / 100;
    const int catBoxW = (cx - margin * 3) - driveBoxW;
    const int catBoxX = margin * 2 + driveBoxW;

    /* 1. Drive Selection GroupBox (Left) */
    MoveWindow(g_hGrpDrives, margin, 8, driveBoxW, topPanelH, TRUE);
    MoveWindow(g_hListDrives, margin + 8, 28, driveBoxW - 16, topPanelH - 64, TRUE);
    MoveWindow(g_hBtnDrvAll, margin + 8, topPanelH - 30, 105, 24, TRUE);
    MoveWindow(g_hBtnDrvRefresh, margin + 118, topPanelH - 30, 105, 24, TRUE);
    MoveWindow(g_hBtnAddCustom, margin + 228, topPanelH - 30, 135, 24, TRUE);

    /* 2. Targeted Paths GroupBox (Right) */
    MoveWindow(g_hGrpCategories, catBoxX, 8, catBoxW, topPanelH, TRUE);
    MoveWindow(g_hListCategories, catBoxX + 8, 28, catBoxW - 16, topPanelH - 36, TRUE);

    /* 3. Action Toolbar (SCAN & CLEANER + Filters) */
    int barY = topPanelH + 16;
    MoveWindow(g_hBtnScan, margin, barY, 135, 36, TRUE);
    MoveWindow(g_hBtnCleaner, margin + 143, barY, 155, 36, TRUE);
    MoveWindow(g_hBtnStop, margin + 306, barY, 75, 36, TRUE);

    MoveWindow(g_hBtnSelSafe, margin + 392, barY + 4, 110, 28, TRUE);
    MoveWindow(g_hBtnSelAll, margin + 506, barY + 4, 150, 28, TRUE);
    MoveWindow(g_hBtnSelNone, margin + 660, barY + 4, 95, 28, TRUE);
    MoveWindow(g_hBtnExportCsv, margin + 759, barY + 4, 90, 28, TRUE);

    int rightBtnX = cx - margin - 75;
    MoveWindow(g_hBtnAbout, rightBtnX, barY + 4, 75, 28, TRUE);
    if (g_hBtnElevate && IsWindowVisible(g_hBtnElevate)) {
        MoveWindow(g_hBtnElevate, rightBtnX - 125, barY + 4, 120, 28, TRUE);
    }

    /* 4. Summary Bar */
    int sumY = barY + 42;
    MoveWindow(g_hLblSummary, margin, sumY, cx - margin * 2, 20, TRUE);

    /* 5. Main Table Grid GroupBox & Virtual ListView */
    int gridTop = sumY + 22;
    int bottomAreaH = 54;
    int gridH = cy - gridTop - bottomAreaH;
    if (gridH < 120) gridH = 120;

    MoveWindow(g_hGrpFiles, margin, gridTop, cx - margin * 2, gridH, TRUE);
    MoveWindow(g_hListFiles, margin + 8, gridTop + 20, cx - margin * 2 - 16, gridH - 28, TRUE);

    /* 6. Progress Bar, Status & Copyright Footer */
    int footY1 = gridTop + gridH + 6;
    MoveWindow(g_hProgress, margin, footY1, 260, 18, TRUE);
    MoveWindow(g_hLblStatus, margin + 268, footY1 + 1, cx - margin * 2 - 268, 18, TRUE);

    int footY2 = footY1 + 22;
    MoveWindow(g_hLblCopyright, margin, footY2 + 2, cx - margin * 2 - 150, 20, TRUE);
    MoveWindow(g_hBtnWebsite, cx - margin - 145, footY2 - 1, 145, 22, TRUE);
}

/* Main Window Procedure */
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitializeCriticalSection(&g_statusLock);

            NONCLIENTMETRICSW ncm;
            ZeroMemory(&ncm, sizeof(ncm));
            ncm.cbSize = sizeof(NONCLIENTMETRICSW);
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            g_hFontRegular = CreateFontIndirectW(&ncm.lfMessageFont);

            LOGFONTW lfBold = ncm.lfMessageFont;
            lfBold.lfWeight = FW_BOLD;
            g_hFontBold = CreateFontIndirectW(&lfBold);

            LOGFONTW lfBtn = ncm.lfMessageFont;
            lfBtn.lfWeight = FW_BOLD;
            lfBtn.lfHeight = -14;
            g_hFontTitle = CreateFontIndirectW(&lfBtn);

            /* 1. Drive Selection Group */
            g_hGrpDrives = CreateWindowExW(0, L"BUTTON",
                L"1. Select Server Drives (Scan per Drive)",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                0, 0, 100, 100, hwnd, NULL, g_hInst, NULL);
            SendMessageW(g_hGrpDrives, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hListDrives = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                0, 0, 100, 100, hwnd, (HMENU)IDC_LIST_DRIVES, g_hInst, NULL);
            SendMessageW(g_hListDrives, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            ListView_SetExtendedListViewStyle(g_hListDrives,
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            LVCOLUMNW lvc;
            ZeroMemory(&lvc, sizeof(lvc));
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvc.cx = 125; lvc.pszText = (LPWSTR)L"Drive & Label"; ListView_InsertColumn(g_hListDrives, 0, &lvc);
            lvc.cx = 55;  lvc.pszText = (LPWSTR)L"FS";            ListView_InsertColumn(g_hListDrives, 1, &lvc);
            lvc.cx = 82;  lvc.pszText = (LPWSTR)L"Total";         ListView_InsertColumn(g_hListDrives, 2, &lvc);
            lvc.cx = 120; lvc.pszText = (LPWSTR)L"Free Space";    ListView_InsertColumn(g_hListDrives, 3, &lvc);
            lvc.cx = 70;  lvc.pszText = (LPWSTR)L"Targets";       ListView_InsertColumn(g_hListDrives, 4, &lvc);

            g_hBtnDrvAll = CreateWindowExW(0, L"BUTTON", L"Check All Drives",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 24, hwnd, (HMENU)IDC_BTN_DRV_ALL, g_hInst, NULL);
            g_hBtnDrvRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh Drives",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 24, hwnd, (HMENU)IDC_BTN_DRV_REFRESH, g_hInst, NULL);
            g_hBtnAddCustom = CreateWindowExW(0, L"BUTTON", L"+ Custom Folder...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 120, 24, hwnd, (HMENU)IDC_BTN_ADD_CUSTOM, g_hInst, NULL);
            SendMessageW(g_hBtnDrvAll, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnDrvRefresh, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnAddCustom, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);

            /* 2. Target Categories Group */
            g_hGrpCategories = CreateWindowExW(0, L"BUTTON",
                L"2. Targeted Server Junk & Log Locations (Fast Targeted Scan - No Full Disk Crawl)",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                0, 0, 100, 100, hwnd, NULL, g_hInst, NULL);
            SendMessageW(g_hGrpCategories, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hListCategories = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                0, 0, 100, 100, hwnd, (HMENU)IDC_LIST_CATEGORIES, g_hInst, NULL);
            SendMessageW(g_hListCategories, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            ListView_SetExtendedListViewStyle(g_hListCategories,
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            lvc.cx = 175; lvc.pszText = (LPWSTR)L"Target Category";         ListView_InsertColumn(g_hListCategories, 0, &lvc);
            lvc.cx = 165; lvc.pszText = (LPWSTR)L"Safety Rule";             ListView_InsertColumn(g_hListCategories, 1, &lvc);
            lvc.cx = 340; lvc.pszText = (LPWSTR)L"Common Server Paths";     ListView_InsertColumn(g_hListCategories, 2, &lvc);

            /* 3. Main Action Buttons */
            g_hBtnScan = CreateWindowExW(0, L"BUTTON", L"SCAN",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 130, 36, hwnd, (HMENU)IDC_BTN_SCAN, g_hInst, NULL);
            g_hBtnCleaner = CreateWindowExW(0, L"BUTTON", L"CLEANER (Delete)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 150, 36, hwnd, (HMENU)IDC_BTN_CLEANER, g_hInst, NULL);
            g_hBtnStop = CreateWindowExW(0, L"BUTTON", L"Stop",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 75, 36, hwnd, (HMENU)IDC_BTN_STOP, g_hInst, NULL);
            SendMessageW(g_hBtnScan, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
            SendMessageW(g_hBtnCleaner, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
            SendMessageW(g_hBtnStop, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);

            g_hBtnSelSafe = CreateWindowExW(0, L"BUTTON", L"Select SAFE Only",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 110, 28, hwnd, (HMENU)IDC_BTN_SEL_SAFE, g_hInst, NULL);
            g_hBtnSelAll = CreateWindowExW(0, L"BUTTON", L"Select SAFE + WARNING",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 150, 28, hwnd, (HMENU)IDC_BTN_SEL_ALL, g_hInst, NULL);
            g_hBtnSelNone = CreateWindowExW(0, L"BUTTON", L"Unselect All",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 95, 28, hwnd, (HMENU)IDC_BTN_SEL_NONE, g_hInst, NULL);
            g_hBtnExportCsv = CreateWindowExW(0, L"BUTTON", L"Export CSV",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 90, 28, hwnd, (HMENU)IDC_BTN_EXPORT_CSV, g_hInst, NULL);
            g_hBtnAbout = CreateWindowExW(0, L"BUTTON", L"About",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 75, 28, hwnd, (HMENU)IDC_BTN_ABOUT, g_hInst, NULL);

            SendMessageW(g_hBtnSelSafe, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnSelAll, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnSelNone, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnExportCsv, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            SendMessageW(g_hBtnAbout, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);

            g_hBtnElevate = CreateWindowExW(0, L"BUTTON", L"Run as Admin",
                WS_CHILD | BS_PUSHBUTTON, 0, 0, 115, 28, hwnd, (HMENU)IDC_BTN_ELEVATE, g_hInst, NULL);
            SendMessageW(g_hBtnElevate, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            if (!IsProcessElevated()) {
                ShowWindow(g_hBtnElevate, SW_SHOW);
            }

            /* 4. Summary Label */
            g_hLblSummary = CreateWindowExW(0, L"STATIC",
                L"Ready. Select drive(s) above and click 'SCAN' to preview all log and temp files in the grid without deleting.",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                0, 0, 600, 20, hwnd, (HMENU)IDC_LBL_SUMMARY, g_hInst, NULL);
            SendMessageW(g_hLblSummary, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            /* 5. Main Table Grid (Virtual ListView LVS_OWNERDATA) */
            g_hGrpFiles = CreateWindowExW(0, L"BUTTON",
                L"3. Server Junk & Log Files (Double-click to open in Explorer | Click headers to sort)",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                0, 0, 100, 100, hwnd, NULL, g_hInst, NULL);
            SendMessageW(g_hGrpFiles, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hListFiles = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
                0, 0, 100, 100, hwnd, (HMENU)IDC_LIST_FILES, g_hInst, NULL);
            SendMessageW(g_hListFiles, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);
            ListView_SetExtendedListViewStyle(g_hListFiles,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

            g_hStateImageList = CreateCheckboxStateImageList(hwnd);
            if (g_hStateImageList) {
                ListView_SetImageList(g_hListFiles, g_hStateImageList, LVSIL_STATE);
            }

            lvc.cx = 115; lvc.pszText = (LPWSTR)L"[x] Safety Status";       ListView_InsertColumn(g_hListFiles, 0, &lvc);
            lvc.cx = 55;  lvc.pszText = (LPWSTR)L"Drive";                   ListView_InsertColumn(g_hListFiles, 1, &lvc);
            lvc.cx = 140; lvc.pszText = (LPWSTR)L"Category";                ListView_InsertColumn(g_hListFiles, 2, &lvc);
            lvc.cx = 95;  lvc.pszText = (LPWSTR)L"Size (Sort)";             ListView_InsertColumn(g_hListFiles, 3, &lvc);
            lvc.cx = 135; lvc.pszText = (LPWSTR)L"Last Modified";           ListView_InsertColumn(g_hListFiles, 4, &lvc);
            lvc.cx = 390; lvc.pszText = (LPWSTR)L"Full File Path";          ListView_InsertColumn(g_hListFiles, 5, &lvc);
            lvc.cx = 340; lvc.pszText = (LPWSTR)L"Safety Info / Reason";    ListView_InsertColumn(g_hListFiles, 6, &lvc);

            /* 6. Progress Bar, Status & Copyright Footer */
            g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                0, 0, 250, 18, hwnd, (HMENU)IDC_PROGRESS, g_hInst, NULL);
            SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

            g_hLblStatus = CreateWindowExW(0, L"STATIC",
                IsProcessElevated()
                    ? L"Status: Idle (Running as Administrator - Full Server Access)"
                    : L"Status: Idle (Standard Mode - Click 'Run as Admin' for full access to C:\\Windows\\Temp & IIS Logs)",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS,
                0, 0, 500, 18, hwnd, (HMENU)IDC_LBL_STATUS, g_hInst, NULL);
            SendMessageW(g_hLblStatus, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);

            g_hLblCopyright = CreateWindowExW(0, L"STATIC",
                L"Copyright (c) 2026 Abraham Situmorang  |  Nanoquity 2026  |  www.nanoquity.com  |  Open Source Edition",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                0, 0, 600, 20, hwnd, (HMENU)IDC_LBL_COPYRIGHT, g_hInst, NULL);
            SendMessageW(g_hLblCopyright, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnWebsite = CreateWindowExW(0, L"BUTTON", L"www.nanoquity.com",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 145, 22, hwnd, (HMENU)IDC_BTN_WEBSITE, g_hInst, NULL);
            SendMessageW(g_hBtnWebsite, WM_SETFONT, (WPARAM)g_hFontRegular, TRUE);

            InitCategories(g_categories);
            PopulateDriveListView();
            PopulateCategoryListView();
            SetControlsBusyState(false);
            return 0;
        }

        case WM_SIZE: {
            LayoutControls(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 880;
            mmi->ptMinTrackSize.y = 580;
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_BTN_SCAN:
                    StartScanOperation(hwnd);
                    break;

                case IDC_BTN_CLEANER:
                    StartCleanerOperation(hwnd);
                    break;

                case IDC_BTN_STOP:
                    if (InterlockedCompareExchange(&g_isWorking, 0, 0) != 0) {
                        InterlockedExchange(&g_cancelRequested, 1);
                        SetWindowTextW(g_hLblStatus, L"Stopping operation...");
                    }
                    break;

                case IDC_BTN_SEL_SAFE:
                    for (size_t i = 0; i < g_files.size(); ++i) {
                        g_files[i].checked = (g_files[i].safety == SAFETY_SAFE);
                    }
                    InvalidateRect(g_hListFiles, NULL, FALSE);
                    RecalculateSummary();
                    break;

                case IDC_BTN_SEL_ALL:
                    for (size_t i = 0; i < g_files.size(); ++i) {
                        if (g_files[i].safety == SAFETY_SAFE || g_files[i].safety == SAFETY_WARNING) {
                            g_files[i].checked = true;
                        }
                    }
                    InvalidateRect(g_hListFiles, NULL, FALSE);
                    RecalculateSummary();
                    break;

                case IDC_BTN_SEL_NONE:
                    for (size_t i = 0; i < g_files.size(); ++i) {
                        g_files[i].checked = false;
                    }
                    InvalidateRect(g_hListFiles, NULL, FALSE);
                    RecalculateSummary();
                    break;

                case IDC_BTN_DRV_ALL:
                    for (size_t i = 0; i < g_drives.size(); ++i) {
                        g_drives[i].selected = true;
                        ListView_SetCheckState(g_hListDrives, (int)i, TRUE);
                    }
                    break;

                case IDC_BTN_DRV_REFRESH:
                    PopulateDriveListView();
                    break;

                case IDC_BTN_ADD_CUSTOM:
                    AddCustomFolderDialog(hwnd);
                    break;

                case IDC_BTN_EXPORT_CSV:
                    ExportResultsToCsv(hwnd);
                    break;

                case IDC_BTN_ABOUT:
                    ShowAboutDialog(hwnd);
                    break;

                case IDC_BTN_WEBSITE:
                    ShellExecuteW(hwnd, L"open", L"https://www.nanoquity.com", NULL, NULL, SW_SHOWNORMAL);
                    break;

                case IDC_BTN_ELEVATE:
                    ElevateProcess(hwnd);
                    break;
            }
            return 0;
        }

        case WM_NOTIFY: {
            LPNMHDR nmh = (LPNMHDR)lParam;
            if (nmh->idFrom == IDC_LIST_FILES) {
                switch (nmh->code) {
                    case LVN_GETDISPINFO: {
                        NMLVDISPINFO* pdi = (NMLVDISPINFO*)lParam;
                        int idx = pdi->item.iItem;
                        if (idx < 0 || (size_t)idx >= g_files.size()) return 0;

                        const ScannedFileItem& item = g_files[idx];

                        if (pdi->item.mask & LVIF_STATE) {
                            pdi->item.stateMask = LVIS_STATEIMAGEMASK;
                            pdi->item.state = INDEXTOSTATEIMAGEMASK(item.checked ? 2 : 1);
                        }

                        if (pdi->item.mask & LVIF_TEXT) {
                            switch (pdi->item.iSubItem) {
                                case 0:
                                    wcsncpy(pdi->item.pszText, GetSafetyLevelText(item.safety), pdi->item.cchTextMax);
                                    break;
                                case 1:
                                    wcsncpy(pdi->item.pszText, item.driveRoot, pdi->item.cchTextMax);
                                    break;
                                case 2:
                                    wcsncpy(pdi->item.pszText, GetCategoryShortName(item.categoryId), pdi->item.cchTextMax);
                                    break;
                                case 3:
                                    FormatByteSize(item.fileSize, pdi->item.pszText, pdi->item.cchTextMax);
                                    break;
                                case 4:
                                    FormatFileTimeLocal(item.lastWriteTime, pdi->item.pszText, pdi->item.cchTextMax);
                                    break;
                                case 5:
                                    wcsncpy(pdi->item.pszText, item.fullPath.c_str(), pdi->item.cchTextMax);
                                    break;
                                case 6:
                                    wcsncpy(pdi->item.pszText, item.safetyReason, pdi->item.cchTextMax);
                                    break;
                            }
                            pdi->item.pszText[pdi->item.cchTextMax - 1] = L'\0';
                        }
                        return 0;
                    }

                    case NM_CUSTOMDRAW: {
                        LPNMLVCUSTOMDRAW lvcd = (LPNMLVCUSTOMDRAW)lParam;
                        if (lvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                            return CDRF_NOTIFYITEMDRAW;
                        }
                        if (lvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                            int idx = (int)lvcd->nmcd.dwItemSpec;
                            if (idx >= 0 && (size_t)idx < g_files.size()) {
                                SafetyLevel s = g_files[idx].safety;
                                if (s == SAFETY_SAFE) {
                                    lvcd->clrTextBk = RGB(237, 251, 242); /* Soft safe green */
                                } else if (s == SAFETY_WARNING) {
                                    lvcd->clrTextBk = RGB(255, 248, 226); /* Soft warning amber */
                                } else if (s == SAFETY_DELETED) {
                                    lvcd->clrTextBk = RGB(238, 242, 246); /* Soft deleted gray */
                                    lvcd->clrText = RGB(110, 120, 130);
                                } else if (s == SAFETY_SKIPPED) {
                                    lvcd->clrTextBk = RGB(254, 236, 236); /* Soft locked rose */
                                }
                            }
                            return CDRF_NOTIFYSUBITEMDRAW;
                        }
                        if (lvcd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                            int idx = (int)lvcd->nmcd.dwItemSpec;
                            if (idx >= 0 && (size_t)idx < g_files.size()) {
                                SafetyLevel s = g_files[idx].safety;
                                if (lvcd->iSubItem == 0) {
                                    SelectObject(lvcd->nmcd.hdc, g_hFontBold);
                                    if (s == SAFETY_SAFE) lvcd->clrText = RGB(14, 116, 56);
                                    else if (s == SAFETY_WARNING) lvcd->clrText = RGB(186, 85, 0);
                                    else if (s == SAFETY_DELETED) lvcd->clrText = RGB(37, 99, 235);
                                    else if (s == SAFETY_SKIPPED) lvcd->clrText = RGB(185, 28, 28);
                                    return CDRF_NEWFONT;
                                } else {
                                    SelectObject(lvcd->nmcd.hdc, g_hFontRegular);
                                    if (s == SAFETY_DELETED) lvcd->clrText = RGB(115, 125, 135);
                                    else lvcd->clrText = RGB(20, 24, 30);
                                    return CDRF_NEWFONT;
                                }
                            }
                        }
                        return CDRF_DODEFAULT;
                    }

                    case NM_CLICK: {
                        LPNMITEMACTIVATE pnm = (LPNMITEMACTIVATE)lParam;
                        LVHITTESTINFO hti;
                        ZeroMemory(&hti, sizeof(hti));
                        hti.pt = pnm->ptAction;
                        int idx = ListView_SubItemHitTest(g_hListFiles, &hti);
                        if (idx >= 0 && (hti.flags & LVHT_ONITEMSTATEICON || hti.iSubItem == 0)) {
                            ToggleVirtualItemCheck(idx);
                        }
                        return 0;
                    }

                    case NM_DBLCLK: {
                        LPNMITEMACTIVATE pnm = (LPNMITEMACTIVATE)lParam;
                        if (pnm->iItem >= 0) {
                            OpenItemInExplorer(pnm->iItem);
                        }
                        return 0;
                    }

                    case LVN_KEYDOWN: {
                        LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN)lParam;
                        if (pnkd->wVKey == VK_SPACE) {
                            int sel = ListView_GetNextItem(g_hListFiles, -1, LVNI_SELECTED);
                            if (sel >= 0) {
                                ToggleVirtualItemCheck(sel);
                            }
                        }
                        return 0;
                    }

                    case LVN_COLUMNCLICK: {
                        LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                        if (InterlockedCompareExchange(&g_isWorking, 0, 0) != 0) return 0;
                        if (g_sortColumn == pnmv->iSubItem) {
                            g_sortAscending = !g_sortAscending;
                        } else {
                            g_sortColumn = pnmv->iSubItem;
                            g_sortAscending = (g_sortColumn != 3); /* Size defaults to descending */
                        }
                        SortScannedFiles();
                        InvalidateRect(g_hListFiles, NULL, FALSE);
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_WORKER_PROGRESS: {
            int pct = (int)InterlockedCompareExchange(&g_progressPercent, 0, 0);
            SendMessageW(g_hProgress, PBM_SETPOS, (WPARAM)pct, 0);

            wchar_t pathCopy[MAX_PATH];
            EnterCriticalSection(&g_statusLock);
            wcsncpy(pathCopy, g_currentWorkPath, MAX_PATH - 1);
            pathCopy[MAX_PATH - 1] = L'\0';
            LeaveCriticalSection(&g_statusLock);

            wchar_t statusBuf[512];
            if (lParam == 0) {
                _snwprintf(statusBuf, 512, L"Scanning (%llu files found): %s",
                    (unsigned long long)wParam, pathCopy);
            } else {
                _snwprintf(statusBuf, 512, L"Cleaning (%llu files deleted): %s",
                    (unsigned long long)wParam, pathCopy);
            }
            statusBuf[511] = L'\0';
            SetWindowTextW(g_hLblStatus, statusBuf);
            return 0;
        }

        case WM_WORKER_SCAN_DONE: {
            InterlockedExchange(&g_isWorking, 0);
            SendMessageW(g_hProgress, PBM_SETPOS, 100, 0);

            ListView_SetItemCountEx(g_hListFiles, (int)g_files.size(), LVSICF_NOINVALIDATEALL);
            InvalidateRect(g_hListFiles, NULL, TRUE);

            RecalculateSummary();
            SetControlsBusyState(false);

            wchar_t szTotal[32], szSafe[32];
            FormatByteSize(g_summary.totalBytes, szTotal, 32);
            FormatByteSize(g_summary.safeBytes, szSafe, 32);

            wchar_t doneMsg[512];
            _snwprintf(doneMsg, 512,
                L"Scan completed in %.2f seconds. Found %llu files (%s) | SAFE: %s. Click 'CLEANER' to delete.",
                (double)g_summary.elapsedMs / 1000.0,
                (unsigned long long)g_summary.totalFiles,
                szTotal,
                szSafe);
            doneMsg[511] = L'\0';
            SetWindowTextW(g_hLblStatus, doneMsg);
            return 0;
        }

        case WM_WORKER_CLEAN_DONE: {
            InterlockedExchange(&g_isWorking, 0);
            SendMessageW(g_hProgress, PBM_SETPOS, 100, 0);

            InvalidateRect(g_hListFiles, NULL, TRUE);
            RecalculateSummary();
            PopulateDriveListView(); /* Refresh drive Free Space after deletion */
            SetControlsBusyState(false);

            wchar_t szFreed[32];
            FormatByteSize(g_summary.deletedBytes, szFreed, 32);

            wchar_t statusBuf[512];
            _snwprintf(statusBuf, 512,
                L"Cleaner complete! Successfully deleted %llu files (%s freed) | Locked/Skipped: %llu files.",
                (unsigned long long)g_summary.deletedFiles,
                szFreed,
                (unsigned long long)g_summary.skippedFiles);
            statusBuf[511] = L'\0';
            SetWindowTextW(g_hLblStatus, statusBuf);

            wchar_t reportMsg[512];
            _snwprintf(reportMsg, 512,
                L"Cleaner Operation Complete!\n\n"
                L"  - Files Successfully Deleted : %llu files\n"
                L"  - Disk Space Freed           : %s\n"
                L"  - Files Skipped (In Use)     : %llu files\n"
                L"  - Execution Time             : %.2f seconds\n\n"
                L"Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026\n"
                L"www.nanoquity.com",
                (unsigned long long)g_summary.deletedFiles,
                szFreed,
                (unsigned long long)g_summary.skippedFiles,
                (double)g_summary.elapsedMs / 1000.0);
            reportMsg[511] = L'\0';
            MessageBoxW(hwnd, reportMsg, L"Cleaner Complete - Cleaner Windows Server 2008", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case WM_DESTROY: {
            InterlockedExchange(&g_cancelRequested, 1);
            if (g_hWorkerThread) {
                WaitForSingleObject(g_hWorkerThread, 2000);
                CloseHandle(g_hWorkerThread);
                g_hWorkerThread = NULL;
            }
            if (g_hStateImageList) ImageList_Destroy(g_hStateImageList);
            if (g_hFontRegular) DeleteObject(g_hFontRegular);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            DeleteCriticalSection(&g_statusLock);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* CLI self-test mode (--self-test) for automated verification without deleting files */
static int RunCliSelfTest() {
    InitCategories(g_categories);
    EnumerateServerDrives(g_drives);
    printf("=== Cleaner Windows Server 2008 - Engine Self-Test ===\n");
    printf("Copyright (c) 2026 Abraham Situmorang | Nanoquity 2026 | www.nanoquity.com\n");
    printf("Detected %zu local drives:\n", g_drives.size());

    std::vector<ResolvedTargetDir> allTargets;
    for (size_t i = 0; i < g_drives.size(); ++i) {
        wchar_t szTot[32], szFree[32];
        FormatByteSize(g_drives[i].totalBytes, szTot, 32);
        FormatByteSize(g_drives[i].freeBytes, szFree, 32);
        printf("  Drive %ls [%ls] (%ls) - Total: %ls, Free: %ls, Target Dirs: %d\n",
            g_drives[i].root, g_drives[i].label, g_drives[i].fsName,
            szTot, szFree, g_drives[i].matchedTargetDirs);
        ResolveDriveTargetPaths(g_drives[i].root, g_categories, g_customPaths, allTargets);
    }
    printf("Total resolved target directories across drives: %zu\n", allTargets.size());
    for (size_t i = 0; i < allTargets.size(); ++i) {
        printf("  [%ls] %ls\n", GetCategoryShortName(allTargets[i].categoryId), allTargets[i].path.c_str());
    }
    fflush(stdout);
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    g_hInst = hInstance;

    if (lpCmdLine && wcsstr(lpCmdLine, L"--self-test") != NULL) {
        return RunCliSelfTest();
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t* CLASS_NAME = L"NanoquityCleanerWinServer2008Class";

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    g_hMainWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Cleaner Windows Server 2008 R2  —  High-Performance Server Log & Temp Cleaner  |  Nanoquity 2026 (www.nanoquity.com)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 700,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
