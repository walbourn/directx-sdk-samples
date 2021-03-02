//----------------------------------------------------------------------------
// File: dispinfo.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

struct DxDiag_DXVA_DeinterlaceCaps
{
    WCHAR   szD3DInputFormat[100];
    WCHAR   szD3DOutputFormat[100];
    WCHAR   szGuid[100];
    WCHAR   szCaps[100];
    DWORD dwNumPreviousOutputFrames;
    DWORD dwNumForwardRefSamples;
    DWORD dwNumBackwardRefSamples;

    DWORD m_nElementCount;
};

struct DisplayInfo
{
    WCHAR   m_szDeviceName[100];
    WCHAR   m_szDescription[200];
    WCHAR   m_szKeyDeviceID[200];
    WCHAR   m_szKeyDeviceKey[200];
    WCHAR   m_szManufacturer[200];
    WCHAR   m_szChipType[100];
    WCHAR   m_szDACType[100];
    WCHAR   m_szRevision[100];
    WCHAR   m_szDisplayMemoryLocalized[100];
    WCHAR   m_szDisplayMemoryEnglish[100];
    WCHAR   m_szDisplayModeLocalized[100];
    WCHAR   m_szDisplayModeEnglish[100];

    DWORD m_dwWidth;
    DWORD m_dwHeight;
    DWORD m_dwBpp;
    DWORD m_dwRefreshRate;

    WCHAR   m_szMonitorName[100];
    WCHAR   m_szMonitorMaxRes[100];

    WCHAR   m_szDriverName[100];
    WCHAR   m_szDriverVersion[100];
    WCHAR   m_szDriverAttributes[100];
    WCHAR   m_szDriverLanguageEnglish[100];
    WCHAR   m_szDriverLanguageLocalized[100];
    WCHAR   m_szDriverDateEnglish[100];
    WCHAR   m_szDriverDateLocalized[100];
    LONG m_lDriverSize;
    WCHAR   m_szMiniVdd[100];
    WCHAR   m_szMiniVddDateLocalized[100];
    WCHAR   m_szMiniVddDateEnglish[100];
    LONG m_lMiniVddSize;
    WCHAR   m_szVdd[100];

    BOOL m_bCanRenderWindow;
    BOOL m_bDriverBeta;
    BOOL m_bDriverDebug;
    BOOL m_bDriverSigned;
    BOOL m_bDriverSignedValid;
    DWORD m_dwDDIVersion;
    WCHAR   m_szDDIVersionEnglish[100];
    WCHAR   m_szDDIVersionLocalized[100];

    DWORD m_iAdapter;
    WCHAR   m_szVendorId[50];
    WCHAR   m_szDeviceId[50];
    WCHAR   m_szSubSysId[50];
    WCHAR   m_szRevisionId[50];
    DWORD m_dwWHQLLevel;
    WCHAR   m_szDeviceIdentifier[100];
    WCHAR   m_szDriverSignDate[50];

    BOOL m_bNoHardware;
    BOOL m_bDDAccelerationEnabled;
    BOOL m_b3DAccelerationExists;
    BOOL m_b3DAccelerationEnabled;
    BOOL m_bAGPEnabled;
    BOOL m_bAGPExists;
    BOOL m_bAGPExistenceValid;

    WCHAR   m_szDXVAModes[100];

    std::vector <DxDiag_DXVA_DeinterlaceCaps*> m_vDXVACaps;

    WCHAR   m_szDDStatusLocalized[100];
    WCHAR   m_szDDStatusEnglish[100];
    WCHAR   m_szD3DStatusLocalized[100];
    WCHAR   m_szD3DStatusEnglish[100];
    WCHAR   m_szAGPStatusLocalized[100];
    WCHAR   m_szAGPStatusEnglish[100];

    WCHAR   m_szNotesLocalized[3000];
    WCHAR   m_szNotesEnglish[3000];
    WCHAR   m_szRegHelpText[3000];

    WCHAR   m_szTestResultDDLocalized[3000];
    WCHAR   m_szTestResultDDEnglish[3000];
    WCHAR   m_szTestResultD3D7Localized[3000];
    WCHAR   m_szTestResultD3D7English[3000];
    WCHAR   m_szTestResultD3D8Localized[3000];
    WCHAR   m_szTestResultD3D8English[3000];
    WCHAR   m_szTestResultD3D9Localized[3000];
    WCHAR   m_szTestResultD3D9English[3000];

    DWORD m_nElementCount;
};
