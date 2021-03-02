//----------------------------------------------------------------------------
// File: dxdiag.h
//
// Desc: 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT)
//-----------------------------------------------------------------------------
#pragma once

// Headers for dxdiagn.dll
#include <dxdiag.h>

// Headers for structs to hold info
#include "fileinfo.h"
#include "sysinfo.h"
#include "dispinfo.h"
#include "sndinfo.h"
#include "musinfo.h"
#include "inptinfo.h"
#include "netinfo.h"
#include "showinfo.h"

//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
class CDxDiagInfo
{
public:
    CDxDiagInfo();
    ~CDxDiagInfo();

    HRESULT Init(BOOL bAllowWHQLChecks);
    HRESULT QueryDxDiagViaDll();

protected:
    HRESULT GetSystemInfo(SysInfo** ppSysInfo);
    HRESULT GetSystemDevices(std::vector <SystemDevice*>& vSystemDevices);
    HRESULT GatherSystemDeviceDriverList(IDxDiagContainer* pParent, std::vector <FileNode*>& vDriverList);
    HRESULT GatherFileNodeInst(FileNode* pFileNode, IDxDiagContainer* pObject);
    HRESULT GetDirectXFilesInfo(FileInfo** ppFileInfo);
    HRESULT GetDisplayInfo(std::vector <DisplayInfo*>& vDisplayInfo);
    HRESULT GatherDXVA_DeinterlaceCaps(IDxDiagContainer* pParent, std::vector <DxDiag_DXVA_DeinterlaceCaps*>& vDXVACaps);
    HRESULT GetSoundInfo(std::vector <SoundInfo*>& vSoundInfos, std::vector <SoundCaptureInfo*>& vSoundCaptureInfos);
    HRESULT GetMusicInfo(MusicInfo** ppMusicInfo);
    HRESULT GetInputInfo(InputInfo** ppInputInfo);
    HRESULT GatherInputRelatedDeviceInst(InputRelatedDeviceInfo* pInputRelatedDevice, IDxDiagContainer* pContainer);
    HRESULT GatherInputRelatedDeviceInstDrivers(InputRelatedDeviceInfo* pInputRelatedDevice,
        IDxDiagContainer* pChild);
    HRESULT GetNetworkInfo(NetInfo** ppNetInfo);
    HRESULT GetShowInfo(ShowInfo** ppShowInfo);
    HRESULT GetLogicalDiskInfo(std::vector <LogicalDisk*>& vLogicalDisks);

    HRESULT GetStringValue(IDxDiagContainer* pObject, WCHAR* wstrName, WCHAR* strValue, int nStrLen);
    HRESULT GetUIntValue(IDxDiagContainer* pObject, WCHAR* wstrName, DWORD* pdwValue);
    HRESULT GetIntValue(IDxDiagContainer* pObject, WCHAR* wstrName, LONG* pnValue);
    HRESULT GetBoolValue(IDxDiagContainer* pObject, WCHAR* wstrName, BOOL* pbValue);
    HRESULT GetInt64Value(IDxDiagContainer* pObject, WCHAR* wstrName, ULONGLONG* pullValue);

    VOID    DestroyFileList(FileInfo* pFileInfo);
    VOID    DestroySystemDevice(std::vector <SystemDevice*>& vSystemDevices);
    VOID    DestroyDisplayInfo(std::vector <DisplayInfo*>& vDisplayInfo);
    VOID    DeleteInputTree(std::vector <InputRelatedDeviceInfo*>& vDeviceList);
    VOID    DeleteFileList(std::vector <FileNode*>& vDriverList);
    VOID    DestroyInputInfo(InputInfo* pInputInfo);
    VOID    DestroyMusicInfo(MusicInfo* pMusicInfo);
    VOID    DestroyNetworkInfo(NetInfo* pNetInfo);
    VOID    DestroySoundInfo(std::vector <SoundInfo*>& vSoundInfos);
    VOID    DestroySoundCaptureInfo(std::vector <SoundCaptureInfo*>& vSoundCaptureInfos);
    VOID    DestroyShowInfo(ShowInfo* pShowInfo);

    IDxDiagProvider* m_pDxDiagProvider;
    IDxDiagContainer* m_pDxDiagRoot;
    BOOL m_bCleanupCOM;

public:
    SysInfo* m_pSysInfo;
    std::vector <SystemDevice*> m_vSystemDevices;
    FileInfo* m_pFileInfo;
    std::vector <DisplayInfo*> m_vDisplayInfo;
    std::vector <SoundInfo*> m_vSoundInfos;
    std::vector <SoundCaptureInfo*> m_vSoundCaptureInfos;
    MusicInfo* m_pMusicInfo;
    InputInfo* m_pInputInfo;
    NetInfo* m_pNetInfo;
    ShowInfo* m_pShowInfo;
    std::vector <LogicalDisk*> m_vLogicalDiskList;
};

