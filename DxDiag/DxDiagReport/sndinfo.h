//----------------------------------------------------------------------------
// File: sndinfo.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#pragma once

struct SoundInfo
{
    DWORD m_dwDevnode;
    WCHAR   m_szGuidDeviceID[100];
    WCHAR   m_szHardwareID[200];
    WCHAR   m_szRegKey[200];
    WCHAR   m_szManufacturerID[100];
    WCHAR   m_szProductID[100];
    WCHAR   m_szDescription[200];
    WCHAR   m_szDriverName[200];
    WCHAR   m_szDriverPath[MAX_PATH + 1];
    WCHAR   m_szDriverVersion[100];
    WCHAR   m_szDriverLanguageEnglish[100];
    WCHAR   m_szDriverLanguageLocalized[100];
    WCHAR   m_szDriverAttributes[100];
    WCHAR   m_szDriverDateEnglish[60];
    WCHAR   m_szDriverDateLocalized[60];
    WCHAR   m_szOtherDrivers[200];
    WCHAR   m_szProvider[200];
    WCHAR   m_szType[100]; // Emulated / vxd / wdm
    LONG m_lNumBytes;
    BOOL m_bDriverBeta;
    BOOL m_bDriverDebug;
    BOOL m_bDriverSigned;
    BOOL m_bDriverSignedValid;
    LONG m_lAccelerationLevel;
    DWORD m_dwSpeakerConfig;

    BOOL m_bDefaultSoundPlayback;
    BOOL m_bDefaultVoicePlayback;
    BOOL m_bVoiceManager;
    BOOL m_bEAX20Listener;
    BOOL m_bEAX20Source;
    BOOL m_bI3DL2Listener;
    BOOL m_bI3DL2Source;
    BOOL m_bZoomFX;

    DWORD m_dwFlags;
    DWORD m_dwMinSecondarySampleRate;
    DWORD m_dwMaxSecondarySampleRate;
    DWORD m_dwPrimaryBuffers;
    DWORD m_dwMaxHwMixingAllBuffers;
    DWORD m_dwMaxHwMixingStaticBuffers;
    DWORD m_dwMaxHwMixingStreamingBuffers;
    DWORD m_dwFreeHwMixingAllBuffers;
    DWORD m_dwFreeHwMixingStaticBuffers;
    DWORD m_dwFreeHwMixingStreamingBuffers;
    DWORD m_dwMaxHw3DAllBuffers;
    DWORD m_dwMaxHw3DStaticBuffers;
    DWORD m_dwMaxHw3DStreamingBuffers;
    DWORD m_dwFreeHw3DAllBuffers;
    DWORD m_dwFreeHw3DStaticBuffers;
    DWORD m_dwFreeHw3DStreamingBuffers;
    DWORD m_dwTotalHwMemBytes;
    DWORD m_dwFreeHwMemBytes;
    DWORD m_dwMaxContigFreeHwMemBytes;
    DWORD m_dwUnlockTransferRateHwBuffers;
    DWORD m_dwPlayCpuOverheadSwBuffers;

    WCHAR   m_szNotesLocalized[3000];
    WCHAR   m_szNotesEnglish[3000];
    WCHAR   m_szRegHelpText[3000];
    WCHAR   m_szTestResultLocalized[3000];
    WCHAR   m_szTestResultEnglish[3000];

    DWORD m_nElementCount;
};

struct SoundCaptureInfo
{
    WCHAR   m_szDescription[200];
    WCHAR   m_szGuidDeviceID[100];
    WCHAR   m_szDriverName[200];
    WCHAR   m_szDriverPath[MAX_PATH + 1];
    WCHAR   m_szDriverVersion[100];
    WCHAR   m_szDriverLanguageEnglish[100];
    WCHAR   m_szDriverLanguageLocalized[100];
    WCHAR   m_szDriverAttributes[100];
    WCHAR   m_szDriverDateEnglish[60];
    WCHAR   m_szDriverDateLocalized[60];
    LONG m_lNumBytes;
    BOOL m_bDriverBeta;
    BOOL m_bDriverDebug;

    BOOL m_bDefaultSoundRecording;
    BOOL m_bDefaultVoiceRecording;

    DWORD m_dwFlags;
    DWORD m_dwFormats;

    DWORD m_nElementCount;
};
