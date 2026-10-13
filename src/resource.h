/*
 * Cleaner Windows Server 2008 (High-Performance Win32 Edition)
 * Copyright (c) 2026 Abraham Situmorang - Nanoquity 2026
 * Website: https://www.nanoquity.com
 * License: MIT Open Source
 */

#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP_ICON            101

/* Control IDs */
#define IDC_LIST_DRIVES         1001
#define IDC_LIST_CATEGORIES     1002
#define IDC_LIST_FILES          1003
#define IDC_BTN_SCAN            1004
#define IDC_BTN_CLEANER         1005
#define IDC_BTN_STOP            1006
#define IDC_BTN_SEL_SAFE        1007
#define IDC_BTN_SEL_ALL         1008
#define IDC_BTN_SEL_NONE        1009
#define IDC_BTN_DRV_ALL         1010
#define IDC_BTN_DRV_REFRESH     1011
#define IDC_BTN_ADD_CUSTOM      1012
#define IDC_BTN_EXPORT_CSV      1013
#define IDC_BTN_ABOUT           1014
#define IDC_BTN_WEBSITE         1015
#define IDC_BTN_ELEVATE         1016
#define IDC_PROGRESS            1017
#define IDC_LBL_SUMMARY         1018
#define IDC_LBL_STATUS          1019
#define IDC_LBL_COPYRIGHT       1020

/* Custom Window Messages for Background Worker Threads */
#define WM_WORKER_PROGRESS      (WM_APP + 10)
#define WM_WORKER_SCAN_DONE     (WM_APP + 11)
#define WM_WORKER_CLEAN_DONE    (WM_APP + 12)

#endif /* RESOURCE_H */
