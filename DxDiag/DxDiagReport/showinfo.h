//----------------------------------------------------------------------------
// File: showinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct ShowFilterInfo
{
    WCHAR   m_szName[1024];             // friendly name
    WCHAR   m_szVersion[32];            // version
    WCHAR   m_ClsidFilter[300];         // guid
    WCHAR   m_szFileName[MAX_PATH];     // file name
    WCHAR   m_szFileVersion[32];        // file version
    WCHAR   m_szCatName[1024];          // category name
    WCHAR   m_ClsidCat[300];            // category guid
    DWORD m_dwInputs;                 // number input pins
    DWORD m_dwOutputs;                // number output pins
    DWORD m_dwMerit;                  // merit - in hex

    DWORD m_nElementCount;
};

struct ShowInfo
{
    std::vector <ShowFilterInfo*> m_vShowFilters;
};
