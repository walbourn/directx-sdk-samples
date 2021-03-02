//----------------------------------------------------------------------------
// File: inptinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct InputRelatedDeviceInfo
{
    DWORD m_dwVendorID;
    DWORD m_dwProductID;
    WCHAR   m_szDescription[MAX_PATH];
    WCHAR   m_szLocation[MAX_PATH];
    WCHAR   m_szMatchingDeviceId[MAX_PATH];
    WCHAR   m_szUpperFilters[MAX_PATH];
    WCHAR   m_szService[MAX_PATH];
    WCHAR   m_szLowerFilters[MAX_PATH];
    WCHAR   m_szOEMData[MAX_PATH];
    WCHAR   m_szFlags1[MAX_PATH];
    WCHAR   m_szFlags2[MAX_PATH];

    std::vector <FileNode*> m_vDriverList;
    std::vector <InputRelatedDeviceInfo*> m_vChildren;
    DWORD m_nElementCount;
};

struct InputDeviceInfo
{
    WCHAR   m_szInstanceName[MAX_PATH];
    BOOL m_bAttached;
    DWORD m_dwJoystickID;
    DWORD m_dwVendorID;
    DWORD m_dwProductID;
    DWORD m_dwDevType;
    WCHAR   m_szFFDriverName[MAX_PATH];
    WCHAR   m_szFFDriverDateEnglish[MAX_PATH];
    WCHAR   m_szFFDriverVersion[MAX_PATH];
    LONG m_lFFDriverSize;

    DWORD m_nElementCount;
};

struct InputInfo
{
    BOOL m_bPollFlags;
    WCHAR   m_szInputNotesLocalized[3000];       // DirectX file notes (localized)
    WCHAR   m_szInputNotesEnglish[3000];         // DirectX file notes (english)
    WCHAR   m_szRegHelpText[3000];

    std::vector <InputRelatedDeviceInfo*> m_vGamePortDevices;
    std::vector <InputRelatedDeviceInfo*> m_vUsbRoot;
    std::vector <InputRelatedDeviceInfo*> m_vPS2Devices;
    std::vector <InputDeviceInfo*> m_vDirectInputDevices;

    DWORD m_nElementCount;
};

