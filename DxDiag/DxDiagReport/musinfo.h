//----------------------------------------------------------------------------
// File: musinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct MusicPort
{
    WCHAR   m_szGuid[300];
    BOOL m_bSoftware;
    BOOL m_bKernelMode;
    BOOL m_bUsesDLS;
    BOOL m_bExternal;
    DWORD m_dwMaxAudioChannels;
    DWORD m_dwMaxChannelGroups;
    BOOL m_bDefaultPort;
    BOOL m_bOutputPort;
    WCHAR   m_szDescription[300];

    DWORD m_nElementCount;
};

struct MusicInfo
{
    BOOL m_bDMusicInstalled;
    WCHAR   m_szGMFilePath[MAX_PATH];
    WCHAR   m_szGMFileVersion[100];
    BOOL m_bAccelerationEnabled;
    BOOL m_bAccelerationExists;

    WCHAR   m_szNotesLocalized[3000];
    WCHAR   m_szNotesEnglish[3000];
    WCHAR   m_szRegHelpText[3000];
    WCHAR   m_szTestResultLocalized[3000];
    WCHAR   m_szTestResultEnglish[3000];

    std::vector <MusicPort*> m_vMusicPorts;
    DWORD m_nElementCount;
};
