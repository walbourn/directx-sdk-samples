//----------------------------------------------------------------------------
// File: main.cpp
//
// Desc: Sample app to read info from dxdiagn.dll
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#define STRICT
#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <commctrl.h>
#include <assert.h>
#include <vector>

#include "dxdiaginfo.h"
#include "resource.h"
#if defined(DEBUG) | defined(_DEBUG)
#include <crtdbg.h>
#endif



#define ADD_STRING_LINE_MACRO(x,y)     { AddString( hwndList, x, L## #y, y ); nElementCount++; }
#define ADD_EXPANDED_STRING_LINE_MACRO(x,y)  { AddExpandedString( hwndList, x, L## #y, y ); nElementCount++; }
#define ADD_INT_LINE_MACRO(x,y)        { AddString( hwndList, x, L## #y, _ltow(y,szTmp,10) ); nElementCount++; }
#define ADD_UINT_LINE_MACRO(x,y)       { AddString( hwndList, x, L## #y, _ultow(y,szTmp,10) ); nElementCount++; }
#define ADD_INT64_LINE_MACRO(x,y)      { AddString( hwndList, x, L## #y, _ui64tow(y,szTmp,10) ); nElementCount++; }
#define ADD_UINT_LINE_MACRO2(x,y,z)       { AddString( hwndList, x, y, _ultow(z,szTmp,10) ); nElementCount++; }


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
CDxDiagInfo* g_pDxDiagInfo = nullptr;
HINSTANCE       g_hInst = nullptr;




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
VOID SetupListBox(HWND hDlg);
VOID FillListBox(HWND hDlg);
VOID FillListBoxWithSysInfo(HWND hDlg);
VOID FillListBoxWithLogicalDiskInfo(HWND hwndList);
VOID FillListBoxWithSystemDevices(HWND hDlg);
VOID FillListBoxWithDirectXFilesInfo(HWND hDlg);
VOID FillListBoxWithDisplayInfo(HWND hDlg);
VOID FillListBoxWithDXVAInfo(WCHAR* szParentName, HWND hwndList, std::vector <DxDiag_DXVA_DeinterlaceCaps*>& vDXVACaps);
VOID FillListBoxWithSoundInfo(HWND hDlg);
VOID FillListBoxWithMusicInfo(HWND hDlg);
VOID FillListBoxWithInputInfo(HWND hDlg);
VOID FillListBoxWithInputRelatedInfo(HWND hDlg, std::vector <InputRelatedDeviceInfo*>& vDeviceList);
VOID FillListBoxWithNetworkInfo(HWND hDlg);
VOID FillListBoxWithDirectShowInfo(HWND hDlg);
VOID AddString(HWND hwndList, WCHAR* szKey, WCHAR* szName, WCHAR* szValue);
VOID AddExpandedString(HWND hwndList, WCHAR* szKey, WCHAR* szName, WCHAR* szValue);




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    HRESULT hr;

    g_hInst = hInstance;

    InitCommonControls();

    g_pDxDiagInfo = new CDxDiagInfo();
    if (g_pDxDiagInfo == nullptr)
        return 0;

    if (FAILED(hr = g_pDxDiagInfo->Init(TRUE)))
    {
        delete g_pDxDiagInfo;
        MessageBoxW(nullptr, L"Failed initializing dxdiagn.dll", L"Error", MB_OK);
        return 0;
    }

    if (FAILED(hr = g_pDxDiagInfo->QueryDxDiagViaDll()))
    {
        delete g_pDxDiagInfo;
        MessageBoxW(nullptr, L"Failed querying dxdiagn.dll", L"Error", MB_OK);
        return 0;
    }

    // Show dialog and fill it up with info from dxdiagn.dll
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), nullptr,
        (DLGPROC)MainDlgProc);

    delete g_pDxDiagInfo;

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MainDlgProc()
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // Load and set the icon
        HICON hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MAIN));
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);  // Set big icon
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);  // Set small icon

        SetupListBox(hDlg);
        FillListBox(hDlg);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    }

    return FALSE; // Didn't handle message
}




//-----------------------------------------------------------------------------
// Name: SetupListBox()
// Desc: 
//-----------------------------------------------------------------------------
VOID SetupListBox(HWND hDlg)
{
    LV_COLUMN col;
    LONG iSubItem = 0;

    HWND hwndList = GetDlgItem(hDlg, IDC_LIST);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = L"Container";
    col.cchTextMax = 100;
    col.iSubItem = iSubItem;
    ListView_InsertColumn(hwndList, iSubItem, &col);
    iSubItem++;

    col.pszText = L"Name";
    col.cx = 100;
    col.iSubItem = iSubItem;
    ListView_InsertColumn(hwndList, iSubItem, &col);
    iSubItem++;

    col.pszText = L"Value";
    col.cx = 100;
    col.iSubItem = iSubItem;
    ListView_InsertColumn(hwndList, iSubItem, &col);
    iSubItem++;

    // Add a bogus column so SetColumnWidth doesn't do strange 
    // things with the last real column
    col.fmt = LVCFMT_RIGHT;
    col.pszText = L"";
    col.iSubItem = iSubItem;
    ListView_InsertColumn(hwndList, iSubItem, &col);
    iSubItem++;
}




//-----------------------------------------------------------------------------
// Name: FillListBox()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBox(HWND hDlg)
{
    HWND hwndList = GetDlgItem(hDlg, IDC_LIST);

    FillListBoxWithSysInfo(hwndList);
    FillListBoxWithDisplayInfo(hwndList);
    FillListBoxWithSoundInfo(hwndList);
    FillListBoxWithMusicInfo(hwndList);
    FillListBoxWithInputInfo(hwndList);
    FillListBoxWithNetworkInfo(hwndList);
    FillListBoxWithLogicalDiskInfo(hwndList);
    FillListBoxWithSystemDevices(hwndList);
    FillListBoxWithDirectXFilesInfo(hwndList);
    FillListBoxWithDirectShowInfo(hwndList);

    // Autosize all columns to fit header/text tightly:
    INT iColumn = 0;
    INT iWidthHeader;
    INT iWidthText;
    for (; ; )
    {
        if (FALSE == ListView_SetColumnWidth(hwndList, iColumn, LVSCW_AUTOSIZE_USEHEADER))
            break;
        iWidthHeader = ListView_GetColumnWidth(hwndList, iColumn);
        ListView_SetColumnWidth(hwndList, iColumn, LVSCW_AUTOSIZE);
        iWidthText = ListView_GetColumnWidth(hwndList, iColumn);
        if (iWidthText < iWidthHeader)
            ListView_SetColumnWidth(hwndList, iColumn, iWidthHeader);
        iColumn++;
    }
    // Delete the bogus column that was created
    ListView_DeleteColumn(hwndList, iColumn - 1);

}





//-----------------------------------------------------------------------------
// Name: FillListBoxWithSysInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithSysInfo(HWND hwndList)
{
    SysInfo* pSysInfo = g_pDxDiagInfo->m_pSysInfo;

    WCHAR szTmp[3000];
    WCHAR szKey[] = L"DxDiag_SystemInfo";
    DWORD nElementCount = 0;

    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwOSMajorVersion);
    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwOSMinorVersion);
    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwOSBuildNumber);
    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwOSPlatformID);
    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwDirectXVersionMajor);
    ADD_UINT_LINE_MACRO(szKey, pSysInfo->m_dwDirectXVersionMinor);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szDirectXVersionLetter);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bDebug);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bNECPC98);
    ADD_INT64_LINE_MACRO(szKey, pSysInfo->m_ullPhysicalMemory);
    ADD_INT64_LINE_MACRO(szKey, pSysInfo->m_ullUsedPageFile);
    ADD_INT64_LINE_MACRO(szKey, pSysInfo->m_ullAvailPageFile);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bNetMeetingRunning);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsD3D8DebugRuntimeAvailable);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsD3DDebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDInput8DebugRuntimeAvailable);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDInput8DebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDMusicDebugRuntimeAvailable);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDMusicDebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDDrawDebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDPlayDebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_bIsDSoundDebugRuntime);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nD3DDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDDrawDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDIDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDMusicDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDPlayDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDSoundDebugLevel);
    ADD_INT_LINE_MACRO(szKey, pSysInfo->m_nDShowDebugLevel);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szWindowsDir);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szBuildLab);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szDxDiagVersion);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szSetupParamEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szProcessorEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szSystemManufacturerEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szSystemModelEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szBIOSEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szPhysicalMemoryEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szCSDVersion);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szDirectXVersionEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szDirectXVersionLongEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szMachineNameLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSExLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSExLongLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szLanguagesLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szPageFileLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szTimeLocalized);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szMachineNameEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSExEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szOSExLongEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szLanguagesEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szPageFileEnglish);
    ADD_STRING_LINE_MACRO(szKey, pSysInfo->m_szTimeEnglish);

    for (int i = 0; i < 16; i++)
    {
        WCHAR szName[512];
        swprintf_s(szName, L"pSysInfo->m_ExtFuncBitmasks[%d].dwBits0_31", i);
        ADD_UINT_LINE_MACRO2(szKey, szName, pSysInfo->m_ExtFuncBitmasks[i].dwBits0_31);
        swprintf_s(szName, L"pSysInfo->m_ExtFuncBitmasks[%d].dwBits32_63", i);
        ADD_UINT_LINE_MACRO2(szKey, szName, pSysInfo->m_ExtFuncBitmasks[i].dwBits32_63);
        swprintf_s(szName, L"pSysInfo->m_ExtFuncBitmasks[%d].dwBits64_95", i);
        ADD_UINT_LINE_MACRO2(szKey, szName, pSysInfo->m_ExtFuncBitmasks[i].dwBits64_95);
        swprintf_s(szName, L"pSysInfo->m_ExtFuncBitmasks[%d].dwBits96_127", i);
        ADD_UINT_LINE_MACRO2(szKey, szName, pSysInfo->m_ExtFuncBitmasks[i].dwBits96_127);
    }

#ifdef _DEBUG
    // debug check to make sure we display all the info from the object
    // you do not need to worry about this.  this is for my own verification only
    if (nElementCount != pSysInfo->m_nElementCount)
        OutputDebugStringW(L"**WARNING** -- not all elements from pSysInfo displayed\n");
#endif
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithLogicalDiskInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithLogicalDiskInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];
    WCHAR szName[3000];

    LogicalDisk* pLogicalDisk;
    for (auto iterDisk = pDxDiag->m_vLogicalDiskList.begin(); iterDisk != pDxDiag->m_vLogicalDiskList.end(); iterDisk++)
    {
        pLogicalDisk = *iterDisk;
        wcsncpy(szName, pLogicalDisk->m_szDriveLetter, 30);
        szName[29] = 0;

        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szDriveLetter);
        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szFreeSpace);
        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szMaxSpace);
        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szFileSystem);
        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szModel);
        ADD_STRING_LINE_MACRO(szName, pLogicalDisk->m_szPNPDeviceID);
        ADD_UINT_LINE_MACRO(szName, pLogicalDisk->m_dwHardDriveIndex);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pLogicalDisk->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pLogicalDisk displayed\n");
#endif

        FileNode* pFileNode;
        for (auto iter = pLogicalDisk->m_vDriverList.begin(); iter != pLogicalDisk->m_vDriverList.end(); iter++)
        {
            pFileNode = *iter;
            wcsncpy(szName, pLogicalDisk->m_szDriveLetter, 30);
            szName[29] = 0;
            wcscat_s(szName, L": ");
            wcscat_s(szName, pFileNode->m_szName);

            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szPath);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szName);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szVersion);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageLocalized);

            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwLowDateTime);
            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwHighDateTime);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampLocalized);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szAttributes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_lNumBytes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bExists);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bBeta);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bDebug);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bObsolete);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bProblem);
        }
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithSystemDevices()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithSystemDevices(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];
    WCHAR szName[3000];

    SystemDevice* pSystemDevice;
    for (auto iterDevice = pDxDiag->m_vSystemDevices.begin(); iterDevice != pDxDiag->m_vSystemDevices.end(); iterDevice++)
    {
        pSystemDevice = *iterDevice;
        wcsncpy(szName, pSystemDevice->m_szDescription, 30);
        szName[29] = 0;

        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pSystemDevice->m_szDescription);
        ADD_STRING_LINE_MACRO(szName, pSystemDevice->m_szDeviceID);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pSystemDevice->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pSystemDevice displayed\n");
#endif

        FileNode* pFileNode;
        for (auto iter = pSystemDevice->m_vDriverList.begin(); iter != pSystemDevice->m_vDriverList.end(); iter++)
        {
            pFileNode = *iter;
            wcsncpy(szName, pSystemDevice->m_szDescription, 30);
            szName[29] = 0;
            wcscat_s(szName, L": ");
            wcscat_s(szName, pFileNode->m_szName);

            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szPath);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szName);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szVersion);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageLocalized);

            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwLowDateTime);
            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwHighDateTime);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampLocalized);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szAttributes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_lNumBytes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bExists);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bBeta);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bDebug);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bObsolete);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bProblem);
        }
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithDirectXFilesInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithDirectXFilesInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    FileNode* pFileNode;
    FileInfo* pFileInfo = pDxDiag->m_pFileInfo;
    WCHAR szKey[] = L"DxDiag_FileInfo";
    DWORD nElementCount = 0;

    if (pDxDiag->m_pFileInfo == nullptr)
        return;

    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pFileInfo->m_szDXFileNotesLocalized);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pFileInfo->m_szDXFileNotesEnglish);

#ifdef _DEBUG
    // debug check to make sure we display all the info from the object
    // you do not need to worry about this.  this is for my own verification only
    if (nElementCount != pFileInfo->m_nElementCount)
        OutputDebugStringW(L"**WARNING** -- not all elements from pFileInfo displayed\n");
#endif

    for (auto iter = pDxDiag->m_pFileInfo->m_vDxComponentsFiles.begin();
        iter != pDxDiag->m_pFileInfo->m_vDxComponentsFiles.end(); iter++)
    {
        pFileNode = *iter;
        WCHAR* szName = pFileNode->m_szName;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szName);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szVersion);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageEnglish);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageLocalized);
        ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwHighDateTime);
        ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwLowDateTime);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampEnglish);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampLocalized);
        ADD_STRING_LINE_MACRO(szName, pFileNode->m_szAttributes);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_lNumBytes);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_bExists);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_bBeta);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_bDebug);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_bObsolete);
        ADD_INT_LINE_MACRO(szName, pFileNode->m_bProblem);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pFileNode->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pFileNode displayed\n");
#endif
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithDisplayInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithDisplayInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    DisplayInfo* pDisplayInfo;
    for (auto iter = pDxDiag->m_vDisplayInfo.begin(); iter != pDxDiag->m_vDisplayInfo.end(); iter++)
    {
        pDisplayInfo = *iter;
        WCHAR* szName = pDisplayInfo->m_szDescription;
        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDeviceName);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDescription);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szKeyDeviceID);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szKeyDeviceKey);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szManufacturer);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szChipType);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDACType);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szRevision);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDisplayMemoryLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDisplayMemoryEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDisplayModeLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDisplayModeEnglish);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwWidth);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwHeight);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwBpp);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwRefreshRate);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szMonitorName);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szMonitorMaxRes);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverName);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverVersion);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverAttributes);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverLanguageEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverLanguageLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverDateEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverDateLocalized);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_lDriverSize);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szMiniVdd);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szMiniVddDateLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szMiniVddDateEnglish);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_lMiniVddSize);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szVdd);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bCanRenderWindow);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bDriverBeta);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bDriverDebug);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bDriverSigned);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bDriverSignedValid);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwDDIVersion);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDDIVersionLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDDIVersionEnglish);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_iAdapter);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szVendorId);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDeviceId);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szSubSysId);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szRevisionId);
        ADD_UINT_LINE_MACRO(szName, pDisplayInfo->m_dwWHQLLevel);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDeviceIdentifier);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDriverSignDate);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bNoHardware);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bDDAccelerationEnabled);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_b3DAccelerationExists);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_b3DAccelerationEnabled);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bAGPEnabled);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bAGPExists);
        ADD_INT_LINE_MACRO(szName, pDisplayInfo->m_bAGPExistenceValid);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDXVAModes);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDDStatusLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szDDStatusEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szD3DStatusLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szD3DStatusEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szAGPStatusLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szAGPStatusEnglish);
        ADD_EXPANDED_STRING_LINE_MACRO(szName, pDisplayInfo->m_szNotesLocalized);
        ADD_EXPANDED_STRING_LINE_MACRO(szName, pDisplayInfo->m_szNotesEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szRegHelpText);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultDDLocalized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultDDEnglish);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D7Localized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D7English);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D8Localized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D8English);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D9Localized);
        ADD_STRING_LINE_MACRO(szName, pDisplayInfo->m_szTestResultD3D9English);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pDisplayInfo->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pDisplayInfo displayed\n");
#endif

        FillListBoxWithDXVAInfo(szName, hwndList, pDisplayInfo->m_vDXVACaps);
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithDXVAInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithDXVAInfo(WCHAR* szParentName, HWND hwndList, std::vector <DxDiag_DXVA_DeinterlaceCaps*>& vDXVACaps)
{
    DWORD dwIndex = 0;
    WCHAR szTmp[3000];
    WCHAR szName[256];

    DxDiag_DXVA_DeinterlaceCaps* pDXVANode;
    for (auto iter = vDXVACaps.begin(); iter != vDXVACaps.end(); iter++)
    {
        pDXVANode = *iter;
        swprintf_s(szName, L"%ls : DXVA %d", szParentName, dwIndex);
        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pDXVANode->szGuid);
        ADD_STRING_LINE_MACRO(szName, pDXVANode->szD3DInputFormat);
        ADD_STRING_LINE_MACRO(szName, pDXVANode->szD3DOutputFormat);
        ADD_STRING_LINE_MACRO(szName, pDXVANode->szCaps);
        ADD_UINT_LINE_MACRO(szName, pDXVANode->dwNumPreviousOutputFrames);
        ADD_UINT_LINE_MACRO(szName, pDXVANode->dwNumForwardRefSamples);
        ADD_UINT_LINE_MACRO(szName, pDXVANode->dwNumBackwardRefSamples);

        dwIndex++;
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithSoundInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithSoundInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    SoundInfo* pSoundInfo;
    for (auto iter = pDxDiag->m_vSoundInfos.begin(); iter != pDxDiag->m_vSoundInfos.end(); iter++)
    {
        pSoundInfo = *iter;
        WCHAR* szName = pSoundInfo->m_szDescription;
        DWORD nElementCount = 0;

        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwDevnode);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szGuidDeviceID);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szHardwareID);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szRegKey);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szManufacturerID);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szProductID);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDescription);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverName);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverPath);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverVersion);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverLanguageEnglish);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverLanguageLocalized);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverAttributes);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverDateEnglish);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szDriverDateLocalized);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szOtherDrivers);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szProvider);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szType);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_lNumBytes);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDriverBeta);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDriverDebug);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDriverSigned);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDriverSignedValid);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_lAccelerationLevel);

        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDefaultSoundPlayback);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bDefaultVoicePlayback);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bVoiceManager);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bEAX20Listener);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bEAX20Source);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bI3DL2Listener);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bI3DL2Source);
        ADD_INT_LINE_MACRO(szName, pSoundInfo->m_bZoomFX);

        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFlags);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMinSecondarySampleRate);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxSecondarySampleRate);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwPrimaryBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHwMixingAllBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHwMixingStaticBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHwMixingStreamingBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHwMixingAllBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHwMixingStaticBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHwMixingStreamingBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHw3DAllBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHw3DStaticBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxHw3DStreamingBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHw3DAllBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHw3DStaticBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHw3DStreamingBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwTotalHwMemBytes);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwFreeHwMemBytes);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwMaxContigFreeHwMemBytes);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwUnlockTransferRateHwBuffers);
        ADD_UINT_LINE_MACRO(szName, pSoundInfo->m_dwPlayCpuOverheadSwBuffers);

        ADD_EXPANDED_STRING_LINE_MACRO(szName, pSoundInfo->m_szNotesLocalized);
        ADD_EXPANDED_STRING_LINE_MACRO(szName, pSoundInfo->m_szNotesEnglish);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szRegHelpText);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szTestResultLocalized);
        ADD_STRING_LINE_MACRO(szName, pSoundInfo->m_szTestResultEnglish);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pSoundInfo->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pSoundInfo displayed\n");
#endif
    }

    SoundCaptureInfo* pSoundCaptureInfo;
    for (auto iterCapture = pDxDiag->m_vSoundCaptureInfos.begin(); iterCapture != pDxDiag->m_vSoundCaptureInfos.end();
        iterCapture++)
    {
        pSoundCaptureInfo = *iterCapture;
        WCHAR* szName = pSoundCaptureInfo->m_szDescription;
        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDescription);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szGuidDeviceID);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverName);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverPath);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverVersion);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverLanguageEnglish);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverLanguageLocalized);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverAttributes);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverDateEnglish);
        ADD_STRING_LINE_MACRO(szName, pSoundCaptureInfo->m_szDriverDateLocalized);
        ADD_INT_LINE_MACRO(szName, pSoundCaptureInfo->m_lNumBytes);
        ADD_INT_LINE_MACRO(szName, pSoundCaptureInfo->m_bDriverBeta);
        ADD_INT_LINE_MACRO(szName, pSoundCaptureInfo->m_bDriverDebug);

        ADD_INT_LINE_MACRO(szName, pSoundCaptureInfo->m_bDefaultSoundRecording);
        ADD_INT_LINE_MACRO(szName, pSoundCaptureInfo->m_bDefaultVoiceRecording);
        ADD_UINT_LINE_MACRO(szName, pSoundCaptureInfo->m_dwFlags);
        ADD_UINT_LINE_MACRO(szName, pSoundCaptureInfo->m_dwFormats);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pSoundCaptureInfo->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pSoundCaptureInfo displayed\n");
#endif
    }

}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithMusicInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithMusicInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    MusicInfo* pMusicInfo = pDxDiag->m_pMusicInfo;
    WCHAR szKey[] = L"DxDiag_MusicInfo";
    DWORD nElementCount = 0;

    if (pMusicInfo == nullptr)
        return;

    ADD_INT_LINE_MACRO(szKey, pMusicInfo->m_bDMusicInstalled);
    ADD_STRING_LINE_MACRO(szKey, pMusicInfo->m_szGMFilePath);
    ADD_STRING_LINE_MACRO(szKey, pMusicInfo->m_szGMFileVersion);
    ADD_INT_LINE_MACRO(szKey, pMusicInfo->m_bAccelerationEnabled);
    ADD_INT_LINE_MACRO(szKey, pMusicInfo->m_bAccelerationExists);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pMusicInfo->m_szNotesLocalized);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pMusicInfo->m_szNotesEnglish);
    ADD_STRING_LINE_MACRO(szKey, pMusicInfo->m_szRegHelpText);
    ADD_STRING_LINE_MACRO(szKey, pMusicInfo->m_szTestResultLocalized);
    ADD_STRING_LINE_MACRO(szKey, pMusicInfo->m_szTestResultEnglish);

#ifdef _DEBUG
    // debug check to make sure we display all the info from the object
    // you do not need to worry about this.  this is for my own verification only
    if (nElementCount != pMusicInfo->m_nElementCount)
        OutputDebugStringW(L"**WARNING** -- not all elements from pMusicInfo displayed\n");
#endif

    MusicPort* pMusicPort;
    for (auto iter = pDxDiag->m_pMusicInfo->m_vMusicPorts.begin(); iter != pDxDiag->m_pMusicInfo->m_vMusicPorts.end();
        iter++)
    {
        pMusicPort = *iter;
        WCHAR* szName = pMusicPort->m_szDescription;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pMusicPort->m_szGuid);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bSoftware);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bKernelMode);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bUsesDLS);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bExternal);
        ADD_UINT_LINE_MACRO(szName, pMusicPort->m_dwMaxAudioChannels);
        ADD_UINT_LINE_MACRO(szName, pMusicPort->m_dwMaxChannelGroups);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bDefaultPort);
        ADD_INT_LINE_MACRO(szName, pMusicPort->m_bOutputPort);
        ADD_STRING_LINE_MACRO(szName, pMusicPort->m_szDescription);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pMusicPort->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pMusicPort displayed\n");
#endif
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithInputInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithInputInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    InputInfo* pInputInfo = pDxDiag->m_pInputInfo;
    WCHAR szKey[] = L"DxDiag_InputInfo";
    DWORD nElementCount = 0;

    if (pInputInfo == nullptr)
        return;

    ADD_INT_LINE_MACRO(szKey, pInputInfo->m_bPollFlags);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pInputInfo->m_szInputNotesLocalized);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pInputInfo->m_szInputNotesEnglish);
    ADD_STRING_LINE_MACRO(szKey, pInputInfo->m_szRegHelpText);

#ifdef _DEBUG
    // debug check to make sure we display all the info from the object
    // you do not need to worry about this.  this is for my own verification only
    if (nElementCount != pInputInfo->m_nElementCount)
        OutputDebugStringW(L"**WARNING** -- not all elements from pInputInfo displayed\n");
#endif

    InputDeviceInfo* pInputDevice;
    for (auto iter = pDxDiag->m_pInputInfo->m_vDirectInputDevices.begin();
        iter != pDxDiag->m_pInputInfo->m_vDirectInputDevices.end(); iter++)
    {
        pInputDevice = *iter;
        WCHAR* szName = pInputDevice->m_szInstanceName;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pInputDevice->m_szInstanceName);
        ADD_INT_LINE_MACRO(szName, pInputDevice->m_bAttached);
        ADD_UINT_LINE_MACRO(szName, pInputDevice->m_dwVendorID);
        ADD_UINT_LINE_MACRO(szName, pInputDevice->m_dwProductID);
        ADD_UINT_LINE_MACRO(szName, pInputDevice->m_dwJoystickID);
        ADD_UINT_LINE_MACRO(szName, pInputDevice->m_dwDevType);
        ADD_STRING_LINE_MACRO(szName, pInputDevice->m_szFFDriverName);
        ADD_STRING_LINE_MACRO(szName, pInputDevice->m_szFFDriverDateEnglish);
        ADD_STRING_LINE_MACRO(szName, pInputDevice->m_szFFDriverVersion);
        ADD_INT_LINE_MACRO(szName, pInputDevice->m_lFFDriverSize);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pInputDevice->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pInputDevice displayed\n");
#endif
    }

    FillListBoxWithInputRelatedInfo(hwndList, pDxDiag->m_pInputInfo->m_vGamePortDevices);
    FillListBoxWithInputRelatedInfo(hwndList, pDxDiag->m_pInputInfo->m_vUsbRoot);
    FillListBoxWithInputRelatedInfo(hwndList, pDxDiag->m_pInputInfo->m_vPS2Devices);
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithInputRelatedInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithInputRelatedInfo(HWND hwndList, std::vector <InputRelatedDeviceInfo*>& vDeviceList)
{
    WCHAR szTmp[3000];

    DWORD nElementCount = 0;

    InputRelatedDeviceInfo* pInputRelatedDevice;
    for (auto iter = vDeviceList.begin(); iter != vDeviceList.end(); iter++)
    {
        pInputRelatedDevice = *iter;
        WCHAR* szName = pInputRelatedDevice->m_szDescription;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szDescription);
        ADD_UINT_LINE_MACRO(szName, pInputRelatedDevice->m_dwVendorID);
        ADD_UINT_LINE_MACRO(szName, pInputRelatedDevice->m_dwProductID);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szLocation);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szMatchingDeviceId);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szUpperFilters);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szService);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szLowerFilters);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szOEMData);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szFlags1);
        ADD_STRING_LINE_MACRO(szName, pInputRelatedDevice->m_szFlags2);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pInputRelatedDevice->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pInputRelatedDevice displayed\n");
#endif

        FileNode* pFileNode;
        for (auto iterFile = pInputRelatedDevice->m_vDriverList.begin();
            iterFile != pInputRelatedDevice->m_vDriverList.end(); iterFile++)
        {
            pFileNode = *iterFile;
            wcsncpy(szName, pInputRelatedDevice->m_szDescription, 30);
            szName[29] = 0;
            wcscat_s(szName, MAX_PATH, L": ");
            wcscat_s(szName, MAX_PATH, pFileNode->m_szName);

            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szPath);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szName);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szVersion);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szLanguageLocalized);

            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwLowDateTime);
            ADD_UINT_LINE_MACRO(szName, pFileNode->m_FileTime.dwHighDateTime);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampEnglish);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szDatestampLocalized);
            ADD_STRING_LINE_MACRO(szName, pFileNode->m_szAttributes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_lNumBytes);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bExists);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bBeta);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bDebug);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bObsolete);
            ADD_INT_LINE_MACRO(szName, pFileNode->m_bProblem);
        }

        if (!pInputRelatedDevice->m_vChildren.empty())
            FillListBoxWithInputRelatedInfo(hwndList, pInputRelatedDevice->m_vChildren);
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithNetworkInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithNetworkInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    NetInfo* pNetInfo = pDxDiag->m_pNetInfo;
    WCHAR szKey[] = L"DxDiag_NetInfo";
    DWORD nElementCount = 0;

    if (pNetInfo == nullptr)
        return;

    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pNetInfo->m_szNetworkNotesLocalized);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pNetInfo->m_szNetworkNotesEnglish);
    ADD_EXPANDED_STRING_LINE_MACRO(szKey, pNetInfo->m_szRegHelpText);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szTestResultLocalized);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szTestResultEnglish);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardFullDuplexTestLocalized);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardHalfDuplexTestLocalized);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardMicTestLocalized);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardFullDuplexTestEnglish);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardHalfDuplexTestEnglish);
    ADD_STRING_LINE_MACRO(szKey, pNetInfo->m_szVoiceWizardMicTestEnglish);

#ifdef _DEBUG
    // debug check to make sure we display all the info from the object
    // you do not need to worry about this.  this is for my own verification only
    if (nElementCount != pNetInfo->m_nElementCount)
        OutputDebugStringW(L"**WARNING** -- not all elements from pNetInfo displayed\n");
#endif

    NetApp* pNetApp;
    for (auto iterNetApp = pDxDiag->m_pNetInfo->m_vNetApps.begin(); iterNetApp != pDxDiag->m_pNetInfo->m_vNetApps.end();
        iterNetApp++)
    {
        pNetApp = *iterNetApp;
        WCHAR* szName = pNetApp->m_szName;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szName);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szGuid);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szExeFile);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szExePath);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szExeVersionLocalized);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szExeVersionEnglish);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szLauncherFile);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szLauncherPath);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szLauncherVersionLocalized);
        ADD_STRING_LINE_MACRO(szName, pNetApp->m_szLauncherVersionEnglish);
        ADD_INT_LINE_MACRO(szName, pNetApp->m_bRegistryOK);
        ADD_INT_LINE_MACRO(szName, pNetApp->m_bProblem);
        ADD_INT_LINE_MACRO(szName, pNetApp->m_bFileMissing);
        ADD_UINT_LINE_MACRO(szName, pNetApp->m_dwDXVer);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pNetApp->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pNetApp displayed\n");
#endif
    }

    NetSP* pNetSP;
    for (auto iterNetSP = pDxDiag->m_pNetInfo->m_vNetSPs.begin(); iterNetSP != pDxDiag->m_pNetInfo->m_vNetSPs.end();
        iterNetSP++)
    {
        pNetSP = *iterNetSP;
        WCHAR* szName = pNetSP->m_szNameEnglish;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szNameLocalized);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szNameEnglish);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szGuid);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szFile);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szPath);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szVersionLocalized);
        ADD_STRING_LINE_MACRO(szName, pNetSP->m_szVersionEnglish);
        ADD_INT_LINE_MACRO(szName, pNetSP->m_bRegistryOK);
        ADD_INT_LINE_MACRO(szName, pNetSP->m_bProblem);
        ADD_INT_LINE_MACRO(szName, pNetSP->m_bFileMissing);
        ADD_INT_LINE_MACRO(szName, pNetSP->m_bInstalled);
        ADD_UINT_LINE_MACRO(szName, pNetSP->m_dwDXVer);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pNetSP->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pNetSP displayed\n");
#endif
    }

    NetAdapter* pNetAdapter;
    for (auto iterNetAdapter = pDxDiag->m_pNetInfo->m_vNetAdapters.begin();
        iterNetAdapter != pDxDiag->m_pNetInfo->m_vNetAdapters.end(); iterNetAdapter++)
    {
        pNetAdapter = *iterNetAdapter;
        WCHAR szName[200];
        wcsncpy(szName, pNetAdapter->m_szAdapterName, 50);
        szName[49] = 0;

        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pNetAdapter->m_szAdapterName);
        ADD_STRING_LINE_MACRO(szName, pNetAdapter->m_szSPNameEnglish);
        ADD_STRING_LINE_MACRO(szName, pNetAdapter->m_szSPNameLocalized);
        ADD_STRING_LINE_MACRO(szName, pNetAdapter->m_szGuid);
        ADD_UINT_LINE_MACRO(szName, pNetAdapter->m_dwFlags);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pNetAdapter->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pNetAdapter displayed\n");
#endif
    }

    NetVoiceCodec* pNetVoiceCodec;
    for (auto iterNetCodec = pDxDiag->m_pNetInfo->m_vNetVoiceCodecs.begin();
        iterNetCodec != pDxDiag->m_pNetInfo->m_vNetVoiceCodecs.end(); iterNetCodec++)
    {
        pNetVoiceCodec = *iterNetCodec;
        WCHAR* szName = pNetVoiceCodec->m_szName;
        nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pNetVoiceCodec->m_szName);
        ADD_STRING_LINE_MACRO(szName, pNetVoiceCodec->m_szGuid);
        ADD_STRING_LINE_MACRO(szName, pNetVoiceCodec->m_szDescription);
        ADD_UINT_LINE_MACRO(szName, pNetVoiceCodec->m_dwFlags);
        ADD_UINT_LINE_MACRO(szName, pNetVoiceCodec->m_dwMaxBitsPerSecond);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pNetVoiceCodec->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pNetVoiceCodec displayed\n");
#endif
    }
}




//-----------------------------------------------------------------------------
// Name: FillListBoxWithDirectShowInfo()
// Desc: 
//-----------------------------------------------------------------------------
VOID FillListBoxWithDirectShowInfo(HWND hwndList)
{
    CDxDiagInfo* pDxDiag = g_pDxDiagInfo;
    WCHAR szTmp[3000];

    if (pDxDiag->m_pShowInfo == nullptr)
        return;

    ShowFilterInfo* pShowFilterInfo;
    for (auto iter = pDxDiag->m_pShowInfo->m_vShowFilters.begin(); iter != pDxDiag->m_pShowInfo->m_vShowFilters.end();
        iter++)
    {
        pShowFilterInfo = *iter;
        WCHAR* szName = pShowFilterInfo->m_szName;
        DWORD nElementCount = 0;

        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_szName);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_szVersion);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_ClsidFilter);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_szFileName);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_szFileVersion);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_szCatName);
        ADD_STRING_LINE_MACRO(szName, pShowFilterInfo->m_ClsidCat);
        ADD_UINT_LINE_MACRO(szName, pShowFilterInfo->m_dwInputs);
        ADD_UINT_LINE_MACRO(szName, pShowFilterInfo->m_dwOutputs);
        ADD_UINT_LINE_MACRO(szName, pShowFilterInfo->m_dwMerit);

#ifdef _DEBUG
        // debug check to make sure we display all the info from the object
        // you do not need to worry about this.  this is for my own verification only
        if (nElementCount != pShowFilterInfo->m_nElementCount)
            OutputDebugStringW(L"**WARNING** -- not all elements from pShowFilterInfo displayed\n");
#endif
    }
}




//-----------------------------------------------------------------------------
// Name: AddString()
// Desc: 
//-----------------------------------------------------------------------------
VOID AddString(HWND hwndList, WCHAR* szKey, WCHAR* szName, WCHAR* szValue)
{
    LV_ITEM item;
    LONG iSubItem;
    iSubItem = 0;
    item.mask = LVIF_TEXT | LVIF_STATE;
    item.iItem = ListView_GetItemCount(hwndList);
    item.stateMask = 0xffff;
    item.state = 0;

    item.iSubItem = iSubItem++;
    item.pszText = szKey;
    if (-1 == ListView_InsertItem(hwndList, &item))
        return;

    item.iSubItem = iSubItem++;
    item.pszText = szName;
    if (-1 == ListView_SetItem(hwndList, &item))
        return;

    item.iSubItem = iSubItem++;
    item.pszText = szValue;
    if (-1 == ListView_SetItem(hwndList, &item))
        return;
}




//-----------------------------------------------------------------------------
// Name: AddExpandedString()
// Desc: 
//-----------------------------------------------------------------------------
VOID AddExpandedString(HWND hwndList, WCHAR* szKey, WCHAR* szName, WCHAR* szValue)
{
    WCHAR szBuffer[1024 * 7];
    wcsncpy(szBuffer, szValue, 1024 * 7);
    szBuffer[1024 * 7 - 1] = 0;

    WCHAR* pEndOfLine;
    WCHAR* pCurrent = szBuffer;
    WCHAR* pStartOfNext;

    pEndOfLine = wcschr(pCurrent, L'\r');
    if (pEndOfLine == nullptr)
    {
        AddString(hwndList, szKey, szName, pCurrent);
        return;
    }

    for (; ; )
    {
        *pEndOfLine = 0;
        pStartOfNext = pEndOfLine + 2;

        AddString(hwndList, szKey, szName, pCurrent);

        // Advance current
        pCurrent = pStartOfNext;

        // Look for the end of the next, and stop if there's no more
        pEndOfLine = wcschr(pStartOfNext, L'\r');
        if (pEndOfLine == nullptr)
            break;
    }
}



