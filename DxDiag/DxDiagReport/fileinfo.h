//----------------------------------------------------------------------------
// File: fileinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct FileNode
{
    WCHAR   m_szName[MAX_PATH];
    WCHAR   m_szPath[MAX_PATH];
    WCHAR   m_szVersion[50];
    WCHAR   m_szLanguageEnglish[100];
    WCHAR   m_szLanguageLocalized[100];
    FILETIME m_FileTime;
    WCHAR   m_szDatestampEnglish[30];
    WCHAR   m_szDatestampLocalized[30];
    WCHAR   m_szAttributes[50];
    LONG m_lNumBytes;
    BOOL m_bExists;
    BOOL m_bBeta;
    BOOL m_bDebug;
    BOOL m_bObsolete;
    BOOL m_bProblem;

    DWORD m_nElementCount;
};

struct FileInfo
{
    std::vector <FileNode*> m_vDxComponentsFiles;
    WCHAR   m_szDXFileNotesLocalized[3000];       // DirectX file notes (localized)
    WCHAR   m_szDXFileNotesEnglish[3000];         // DirectX file notes (english)
    DWORD m_nElementCount;
};
