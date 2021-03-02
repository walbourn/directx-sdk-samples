//----------------------------------------------------------------------------
// File: netinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct NetSP
{
    WCHAR   m_szNameLocalized[200];
    WCHAR   m_szNameEnglish[200];
    WCHAR   m_szGuid[100];
    WCHAR   m_szFile[100];
    WCHAR   m_szPath[MAX_PATH];
    WCHAR   m_szVersionLocalized[50];
    WCHAR   m_szVersionEnglish[50];
    BOOL m_bRegistryOK;
    BOOL m_bProblem;
    BOOL m_bFileMissing;
    BOOL m_bInstalled;
    DWORD m_dwDXVer;

    DWORD m_nElementCount;
};

struct NetAdapter
{
    WCHAR   m_szAdapterName[200];
    WCHAR   m_szSPNameEnglish[200];
    WCHAR   m_szSPNameLocalized[200];
    WCHAR   m_szGuid[100];
    DWORD m_dwFlags;

    DWORD m_nElementCount;
};

struct NetApp
{
    WCHAR   m_szName[200];
    WCHAR   m_szGuid[100];
    WCHAR   m_szExeFile[100];
    WCHAR   m_szExePath[MAX_PATH];
    WCHAR   m_szExeVersionLocalized[50];
    WCHAR   m_szExeVersionEnglish[50];
    WCHAR   m_szLauncherFile[100];
    WCHAR   m_szLauncherPath[MAX_PATH];
    WCHAR   m_szLauncherVersionLocalized[50];
    WCHAR   m_szLauncherVersionEnglish[50];
    BOOL m_bRegistryOK;
    BOOL m_bProblem;
    BOOL m_bFileMissing;
    DWORD m_dwDXVer;

    DWORD m_nElementCount;
};

struct NetVoiceCodec
{
    WCHAR   m_szName[200];
    WCHAR   m_szGuid[100];
    WCHAR   m_szDescription[500];
    DWORD m_dwFlags;
    DWORD m_dwMaxBitsPerSecond;

    DWORD m_nElementCount;
};

struct NetInfo
{
    WCHAR   m_szNetworkNotesLocalized[3000];       // DirectX file notes (localized)
    WCHAR   m_szNetworkNotesEnglish[3000];         // DirectX file notes (english)
    WCHAR   m_szRegHelpText[3000];
    WCHAR   m_szTestResultLocalized[3000];
    WCHAR   m_szTestResultEnglish[3000];
    WCHAR   m_szVoiceWizardFullDuplexTestLocalized[200];
    WCHAR   m_szVoiceWizardHalfDuplexTestLocalized[200];
    WCHAR   m_szVoiceWizardMicTestLocalized[200];
    WCHAR   m_szVoiceWizardFullDuplexTestEnglish[200];
    WCHAR   m_szVoiceWizardHalfDuplexTestEnglish[200];
    WCHAR   m_szVoiceWizardMicTestEnglish[200];

    std::vector <NetSP*> m_vNetSPs;
    std::vector <NetApp*> m_vNetApps;
    std::vector <NetVoiceCodec*> m_vNetVoiceCodecs;
    std::vector <NetAdapter*> m_vNetAdapters;
    DWORD m_nElementCount;
};

