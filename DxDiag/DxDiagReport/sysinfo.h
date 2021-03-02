//----------------------------------------------------------------------------
// File: sysinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#pragma once

struct LogicalDisk
{
    WCHAR   m_szDriveLetter[64];
    WCHAR   m_szFreeSpace[128];
    WCHAR   m_szMaxSpace[128];
    WCHAR   m_szFileSystem[128];
    WCHAR   m_szModel[512];
    WCHAR   m_szPNPDeviceID[512];
    DWORD m_dwHardDriveIndex;
    std::vector <FileNode*> m_vDriverList;

    DWORD m_nElementCount;
};

struct SystemDevice
{
    WCHAR   m_szDescription[500];
    WCHAR   m_szDeviceID[300];
    std::vector <FileNode*> m_vDriverList;

    DWORD m_nElementCount;
};

struct CPUExtendedFunctionBitmask
{
    DWORD dwBits0_31;
    DWORD dwBits32_63;
    DWORD dwBits64_95;
    DWORD dwBits96_127;
};

struct SysInfo
{
    DWORD m_dwOSMajorVersion;         // OS major version
    DWORD m_dwOSMinorVersion;         // OS minor version
    DWORD m_dwOSBuildNumber;          // OS build number
    DWORD m_dwOSPlatformID;           // OS platform id
    DWORD m_dwDirectXVersionMajor;    // DirectX major version (ex. 8) -- info in m_szDirectXVersion*
    DWORD m_dwDirectXVersionMinor;    // DirectX minor version (ex. 1) -- info in m_szDirectXVersion*
    WCHAR                       m_szDirectXVersionLetter[2]; // DirectX letter version (ex. a) -- info in m_szDirectXVersion*
    BOOL m_bDebug;                   // debug version of user.exe -- info in m_szOSEx*
    BOOL m_bNECPC98;                 // info in m_szMachineName*
    DWORDLONG m_ullPhysicalMemory;        // info in m_szPageFile*
    DWORDLONG m_ullUsedPageFile;          // info in m_szPageFile*
    DWORDLONG m_ullAvailPageFile;         // info in m_szPageFile*
    BOOL m_bNetMeetingRunning;       // info in DX file notes

    BOOL m_bIsD3D8DebugRuntimeAvailable;
    BOOL m_bIsD3DDebugRuntime;
    BOOL m_bIsDInput8DebugRuntimeAvailable;
    BOOL m_bIsDInput8DebugRuntime;
    BOOL m_bIsDMusicDebugRuntimeAvailable;
    BOOL m_bIsDMusicDebugRuntime;
    BOOL m_bIsDDrawDebugRuntime;
    BOOL m_bIsDPlayDebugRuntime;
    BOOL m_bIsDSoundDebugRuntime;

    LONG m_nD3DDebugLevel;
    LONG m_nDDrawDebugLevel;
    LONG m_nDIDebugLevel;
    LONG m_nDMusicDebugLevel;
    LONG m_nDPlayDebugLevel;
    LONG m_nDSoundDebugLevel;
    LONG m_nDShowDebugLevel;

    // English only or un-localizable strings 
    WCHAR                       m_szWindowsDir[MAX_PATH];             // location of windows dir
    WCHAR                       m_szBuildLab[100];                    // Win2k build lab (not localizable)
    WCHAR                       m_szDxDiagVersion[100];               // DxDiag version (not localizable)
    WCHAR                       m_szSetupParamEnglish[100];           // setup params (English)
    WCHAR                       m_szProcessorEnglish[200];            // Processor name and speed (english)
    WCHAR                       m_szSystemManufacturerEnglish[200];   // System manufacturer (english)
    WCHAR                       m_szSystemModelEnglish[200];          // System model (english)
    WCHAR                       m_szBIOSEnglish[200];                 // BIOS (english)
    WCHAR                       m_szPhysicalMemoryEnglish[100];       // Formatted version of physical memory (english)
    WCHAR                       m_szCSDVersion[200];                  // OS version with CSD info (localized)
    WCHAR                       m_szDirectXVersionEnglish[100];       // DirectX version (english)
    WCHAR                       m_szDirectXVersionLongEnglish[100];   // long DirectX version (english)

    // strings localized to OS language 
    WCHAR                       m_szMachineNameLocalized[200];        // machine name 
    WCHAR                       m_szOSLocalized[100];                 // Formatted version of platform (localized)
    WCHAR                       m_szOSExLocalized[100];               // Formatted version of platform, version, build num (localized)
    WCHAR                       m_szOSExLongLocalized[300];           // Formatted version of platform, version, build num, patch, lab (localized)
    WCHAR                       m_szLanguagesLocalized[200];          // m_szLanguages, in local language (localized)
    WCHAR                       m_szPageFileLocalized[100];           // Formatted version of pagefile (localized)
    WCHAR                       m_szTimeLocalized[100];               // Date/time, localized for UI (localized)

    // strings localized to english 
    WCHAR                       m_szMachineNameEnglish[200];          // machine name 
    WCHAR                       m_szOSEnglish[100];                   // Formatted version of platform (english)
    WCHAR                       m_szOSExEnglish[100];                 // Formatted version of platform, version, build num (english)
    WCHAR                       m_szOSExLongEnglish[300];             // Formatted version of platform, version, build num, patch, lab (english)
    WCHAR                       m_szLanguagesEnglish[200];            // Formatted version of m_szLanguage, m_szLanguageRegional (english) 
    WCHAR                       m_szPageFileEnglish[100];             // Formatted version of pagefile (english)
    WCHAR                       m_szTimeEnglish[100];                 // Date/time, dd/mm/yyyy hh:mm:ss for saved report (english)

    CPUExtendedFunctionBitmask  m_ExtFuncBitmasks[16];  // 128-bit CPU Extended Function Bitmasks (array of 16-byte structs) 

    DWORD m_nElementCount;
};
