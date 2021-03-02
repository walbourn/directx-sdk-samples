//----------------------------------------------------------------------------
// File: dxdiaginfo.cpp
//
// Desc: Sample app to read info from dxdiagn.dll
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#define STRICT
#define INITGUID

#include <Windows.h>
#include <initguid.h>
#include <wchar.h>
#include <new>
#include <vector>

#include "dxdiaginfo.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=nullptr; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=nullptr; } }
#define SAFE_BSTR_FREE(x)    if(x) { SysFreeString( x ); x = nullptr; }
#define EXPAND(x)            x, sizeof(x)/sizeof(WCHAR)




//-----------------------------------------------------------------------------
// Name: CDxDiagInfo()
// Desc: Constuct class
//-----------------------------------------------------------------------------
CDxDiagInfo::CDxDiagInfo() :
    m_pDxDiagProvider(nullptr),
    m_pDxDiagRoot(nullptr),
    m_bCleanupCOM(FALSE),
    m_pSysInfo(nullptr),
    m_pFileInfo(nullptr),
    m_pMusicInfo(nullptr),
    m_pInputInfo(nullptr),
    m_pNetInfo(nullptr),
    m_pShowInfo(nullptr)
{
}




//-----------------------------------------------------------------------------
// Name: ~CDxDiagInfo()
// Desc: Cleanup
//-----------------------------------------------------------------------------
CDxDiagInfo::~CDxDiagInfo()
{
    SAFE_RELEASE(m_pDxDiagRoot);
    SAFE_RELEASE(m_pDxDiagProvider);

    SAFE_DELETE(m_pSysInfo);
    DestroySystemDevice(m_vSystemDevices);
    DestroyFileList(m_pFileInfo);
    DestroyDisplayInfo(m_vDisplayInfo);
    DestroyInputInfo(m_pInputInfo);
    DestroyMusicInfo(m_pMusicInfo);
    DestroyNetworkInfo(m_pNetInfo);
    DestroySoundInfo(m_vSoundInfos);
    DestroySoundCaptureInfo(m_vSoundCaptureInfos);
    DestroyShowInfo(m_pShowInfo);

    if (m_bCleanupCOM)
        CoUninitialize();
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Connect to dxdiagn.dll and init it
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::Init(BOOL bAllowWHQLChecks)
{
    HRESULT hr = CoInitialize(nullptr);
    m_bCleanupCOM = SUCCEEDED(hr);

    hr = CoCreateInstance(CLSID_DxDiagProvider,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IDxDiagProvider,
        (LPVOID*)&m_pDxDiagProvider);
    if (FAILED(hr))
        return hr;
    if (m_pDxDiagProvider == nullptr)
    {
        return E_POINTER;
    }

    // Fill out a DXDIAG_INIT_PARAMS struct and pass it to IDxDiagContainer::Initialize
    // Passing in TRUE for bAllowWHQLChecks, allows dxdiag to check if drivers are 
    // digital signed as logo'd by WHQL which may connect via internet to update 
    // WHQL certificates.    
    DXDIAG_INIT_PARAMS dxDiagInitParam = {};
    dxDiagInitParam.dwSize = sizeof(DXDIAG_INIT_PARAMS);
    dxDiagInitParam.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
    dxDiagInitParam.bAllowWHQLChecks = bAllowWHQLChecks;
    dxDiagInitParam.pReserved = nullptr;

    hr = m_pDxDiagProvider->Initialize(&dxDiagInitParam);
    if (FAILED(hr))
        return hr;

    return m_pDxDiagProvider->GetRootContainer(&m_pDxDiagRoot);
}




//-----------------------------------------------------------------------------
// Name: QueryDxDiagViaDll()
// Desc: Query dxdiagn.dll for all its information
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::QueryDxDiagViaDll()
{
    if (nullptr == m_pDxDiagProvider)
        return E_INVALIDARG;

    // Any of these might fail, but if they do we 
    // can still process the others

    GetSystemInfo(&m_pSysInfo);

    GetSystemDevices(m_vSystemDevices);

    GetDirectXFilesInfo(&m_pFileInfo);

    GetDisplayInfo(m_vDisplayInfo);

    GetSoundInfo(m_vSoundInfos, m_vSoundCaptureInfos);

    GetMusicInfo(&m_pMusicInfo);

    GetInputInfo(&m_pInputInfo);

    GetNetworkInfo(&m_pNetInfo);

    GetShowInfo(&m_pShowInfo);

    GetLogicalDiskInfo(m_vLogicalDiskList);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSystemInfo()
// Desc: Get the system info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetSystemInfo(SysInfo** ppSysInfo)
{
    HRESULT hr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nCurCount = 0;

    if (nullptr == m_pDxDiagProvider)
        return E_INVALIDARG;

    int i;
    SysInfo* pSysInfo = new (std::nothrow) SysInfo;
    if (nullptr == pSysInfo)
        return E_OUTOFMEMORY;
    ZeroMemory(pSysInfo, sizeof(SysInfo));
    *ppSysInfo = pSysInfo;

    // Get the IDxDiagContainer object called "DxDiag_SystemInfo".
    // This call may take some time while dxdiag gathers the info.
    hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_SystemInfo", &pObject);
    if (FAILED(hr) || pObject == nullptr)
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    if (FAILED(hr = GetUIntValue(pObject, L"dwOSMajorVersion", &pSysInfo->m_dwOSMajorVersion)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwOSMinorVersion", &pSysInfo->m_dwOSMinorVersion)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwOSBuildNumber", &pSysInfo->m_dwOSBuildNumber)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwOSPlatformID", &pSysInfo->m_dwOSPlatformID)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwDirectXVersionMajor", &pSysInfo->m_dwDirectXVersionMajor)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwDirectXVersionMinor", &pSysInfo->m_dwDirectXVersionMinor)))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetStringValue(pObject, L"szDirectXVersionLetter", EXPAND(pSysInfo->m_szDirectXVersionLetter))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bDebug", &pSysInfo->m_bDebug)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bNECPC98", &pSysInfo->m_bNECPC98)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetInt64Value(pObject, L"ullPhysicalMemory", &pSysInfo->m_ullPhysicalMemory)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetInt64Value(pObject, L"ullUsedPageFile", &pSysInfo->m_ullUsedPageFile)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetInt64Value(pObject, L"ullAvailPageFile", &pSysInfo->m_ullAvailPageFile)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bNetMeetingRunning", &pSysInfo->m_bNetMeetingRunning)))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetBoolValue(pObject,
        L"bIsD3D8DebugRuntimeAvailable", &pSysInfo->m_bIsD3D8DebugRuntimeAvailable)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsD3DDebugRuntime", &pSysInfo->m_bIsD3DDebugRuntime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject,
        L"bIsDInput8DebugRuntimeAvailable",
        &pSysInfo->m_bIsDInput8DebugRuntimeAvailable)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsDInput8DebugRuntime", &pSysInfo->m_bIsDInput8DebugRuntime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject,
        L"bIsDMusicDebugRuntimeAvailable", &pSysInfo->m_bIsDMusicDebugRuntimeAvailable)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsDMusicDebugRuntime", &pSysInfo->m_bIsDMusicDebugRuntime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsDDrawDebugRuntime", &pSysInfo->m_bIsDDrawDebugRuntime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsDPlayDebugRuntime", &pSysInfo->m_bIsDPlayDebugRuntime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bIsDSoundDebugRuntime", &pSysInfo->m_bIsDSoundDebugRuntime)))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetIntValue(pObject, L"nD3DDebugLevel", &pSysInfo->m_nD3DDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDDrawDebugLevel", &pSysInfo->m_nDDrawDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDIDebugLevel", &pSysInfo->m_nDIDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDMusicDebugLevel", &pSysInfo->m_nDMusicDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDPlayDebugLevel", &pSysInfo->m_nDPlayDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDSoundDebugLevel", &pSysInfo->m_nDSoundDebugLevel)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"nDShowDebugLevel", &pSysInfo->m_nDShowDebugLevel)))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetStringValue(pObject, L"szWindowsDir", EXPAND(pSysInfo->m_szWindowsDir))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szBuildLab", EXPAND(pSysInfo->m_szBuildLab))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDxDiagVersion", EXPAND(pSysInfo->m_szDxDiagVersion))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szSetupParamEnglish", EXPAND(pSysInfo->m_szSetupParamEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szProcessorEnglish", EXPAND(pSysInfo->m_szProcessorEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szSystemManufacturerEnglish",
        EXPAND(pSysInfo->m_szSystemManufacturerEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szSystemModelEnglish", EXPAND(pSysInfo->m_szSystemModelEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szBIOSEnglish", EXPAND(pSysInfo->m_szBIOSEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szPhysicalMemoryEnglish", EXPAND(pSysInfo->m_szPhysicalMemoryEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szCSDVersion", EXPAND(pSysInfo->m_szCSDVersion))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDirectXVersionEnglish", EXPAND(pSysInfo->m_szDirectXVersionEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDirectXVersionLongEnglish",
        EXPAND(pSysInfo->m_szDirectXVersionLongEnglish))))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetStringValue(pObject, L"szMachineNameLocalized", EXPAND(pSysInfo->m_szMachineNameLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSLocalized", EXPAND(pSysInfo->m_szOSLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSExLocalized", EXPAND(pSysInfo->m_szOSExLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSExLongLocalized", EXPAND(pSysInfo->m_szOSExLongLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szLanguagesLocalized", EXPAND(pSysInfo->m_szLanguagesLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szPageFileLocalized", EXPAND(pSysInfo->m_szPageFileLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTimeLocalized", EXPAND(pSysInfo->m_szTimeLocalized))))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetStringValue(pObject, L"szMachineNameEnglish", EXPAND(pSysInfo->m_szMachineNameEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSEnglish", EXPAND(pSysInfo->m_szOSEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSExEnglish", EXPAND(pSysInfo->m_szOSExEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szOSExLongEnglish", EXPAND(pSysInfo->m_szOSExLongEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szLanguagesEnglish", EXPAND(pSysInfo->m_szLanguagesEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szPageFileEnglish", EXPAND(pSysInfo->m_szPageFileEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTimeEnglish", EXPAND(pSysInfo->m_szTimeEnglish))))
        goto LCleanup; nCurCount++;

    // Get the extended cpuid for args 0x80000008 through 0x80000018.  
    // pSysInfo->m_ExtFuncBitmasks[0]  will contain extended cpuid info from arg 0x80000009 
    // pSysInfo->m_ExtFuncBitmasks[15] will contain extended cpuid info from arg 0x80000018
    for (i = 0; i < 16; i++)
    {
        WCHAR strName[512];
        WCHAR strName2[512];
        swprintf_s(strName, L"ExtendedCPUFunctionBitmasks_0x800000%0.2x_bits", i + 0x09);

        wcscpy_s(strName2, strName); wcscat_s(strName2, L"0_31");
        if (FAILED(hr = GetUIntValue(pObject, strName2, &pSysInfo->m_ExtFuncBitmasks[i].dwBits0_31)))
            goto LCleanup; nCurCount++;
        wcscpy_s(strName2, strName); wcscat_s(strName2, L"32_63");
        if (FAILED(hr = GetUIntValue(pObject, strName2, &pSysInfo->m_ExtFuncBitmasks[i].dwBits32_63)))
            goto LCleanup; nCurCount++;
        wcscpy_s(strName2, strName); wcscat_s(strName2, L"64_95");
        if (FAILED(hr = GetUIntValue(pObject, strName2, &pSysInfo->m_ExtFuncBitmasks[i].dwBits64_95)))
            goto LCleanup; nCurCount++;
        wcscpy_s(strName2, strName); wcscat_s(strName2, L"96_127");
        if (FAILED(hr = GetUIntValue(pObject, strName2, &pSysInfo->m_ExtFuncBitmasks[i].dwBits96_127)))
            goto LCleanup; nCurCount++;
    }

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    // normal clients should't do this
    if (FAILED(hr = pObject->GetNumberOfProps(&pSysInfo->m_nElementCount)))
        return hr;
    if (pSysInfo->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pSysInfo recorded");
#endif

LCleanup:
    SAFE_RELEASE(pObject);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetSystemDevices()
// Desc: Get the system devices info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetSystemDevices(std::vector <SystemDevice*>& vSystemDevices)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    if (nullptr == m_pDxDiagProvider)
        return E_INVALIDARG;

    // Get the IDxDiagContainer object called "DxDiag_SystemDevices".
    // This call may take some time while dxdiag gathers the info.
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_SystemDevices", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        SystemDevice* pSystemDevice = new SystemDevice;
        if (pSystemDevice == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pSystemDevice, sizeof(SystemDevice));

        // Add pSystemDevice to vSystemDevices
        vSystemDevices.push_back(pSystemDevice);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pSystemDevice->m_szDescription))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDeviceID", EXPAND(pSystemDevice->m_szDeviceID))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GatherSystemDeviceDriverList(pObject, pSystemDevice->m_vDriverList)))
            goto LCleanup;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pSystemDevice->m_nElementCount)))
            return hr;
        if (pSystemDevice->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pSystemDevice recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetLogicalDiskInfo()
// Desc: Get the logical disk info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetLogicalDiskInfo(std::vector <LogicalDisk*>& vLogicalDisks)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    if (nullptr == m_pDxDiagProvider)
        return E_INVALIDARG;

    // Get the IDxDiagContainer object called "DxDiag_LogicalDisks".
    // This call may take some time while dxdiag gathers the info.
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_LogicalDisks", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        LogicalDisk* pLogicalDisk = new (std::nothrow) LogicalDisk;
        if (pLogicalDisk == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pLogicalDisk, sizeof(LogicalDisk));

        // Add pLogicalDisk to vLogicalDisks
        vLogicalDisks.push_back(pLogicalDisk);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szDriveLetter", EXPAND(pLogicalDisk->m_szDriveLetter))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFreeSpace", EXPAND(pLogicalDisk->m_szFreeSpace))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szMaxSpace", EXPAND(pLogicalDisk->m_szMaxSpace))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFileSystem", EXPAND(pLogicalDisk->m_szFileSystem))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szModel", EXPAND(pLogicalDisk->m_szModel))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szPNPDeviceID", EXPAND(pLogicalDisk->m_szPNPDeviceID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwHardDriveIndex", &pLogicalDisk->m_dwHardDriveIndex)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GatherSystemDeviceDriverList(pObject, pLogicalDisk->m_vDriverList)))
            goto LCleanup;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pLogicalDisk->m_nElementCount)))
            return hr;
        if (pLogicalDisk->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pLogicalDisk recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GatherSystemDeviceDriverList()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GatherSystemDeviceDriverList(IDxDiagContainer* pParent, std::vector <FileNode*>& vDriverList)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    if (FAILED(hr = pParent->GetChildContainer(L"Drivers", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        FileNode* pFileNode = new (std::nothrow) FileNode;
        if (pFileNode == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pFileNode, sizeof(FileNode));

        // Add pFileNode to vDriverList
        vDriverList.push_back(pFileNode);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GatherFileNodeInst(pFileNode, pObject)))
            goto LCleanup;

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GatherFileNodeInst()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GatherFileNodeInst(FileNode* pFileNode, IDxDiagContainer* pObject)
{
    HRESULT hr;
    DWORD nCurCount = 0;

    if (FAILED(hr = GetStringValue(pObject, L"szPath", EXPAND(pFileNode->m_szPath))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szName", EXPAND(pFileNode->m_szName))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVersion", EXPAND(pFileNode->m_szVersion))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szLanguageEnglish", EXPAND(pFileNode->m_szLanguageEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szLanguageLocalized", EXPAND(pFileNode->m_szLanguageLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwFileTimeLow", &pFileNode->m_FileTime.dwLowDateTime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pObject, L"dwFileTimeHigh", &pFileNode->m_FileTime.dwHighDateTime)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDatestampEnglish", EXPAND(pFileNode->m_szDatestampEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDatestampLocalized", EXPAND(pFileNode->m_szDatestampLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szAttributes", EXPAND(pFileNode->m_szAttributes))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetIntValue(pObject, L"lNumBytes", &pFileNode->m_lNumBytes)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bExists", &pFileNode->m_bExists)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bBeta", &pFileNode->m_bBeta)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bDebug", &pFileNode->m_bDebug)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bObsolete", &pFileNode->m_bObsolete)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bProblem", &pFileNode->m_bProblem)))
        goto LCleanup; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pObject->GetNumberOfProps(&pFileNode->m_nElementCount)))
        goto LCleanup;
    if (pFileNode->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pFileNode recorded");
#endif

LCleanup:
    return hr;
}


//-----------------------------------------------------------------------------
// Name: GetDirectXFilesInfo()
// Desc: Get the DirectX file info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetDirectXFilesInfo(FileInfo** ppFileInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    FileInfo* pFileInfo = new (std::nothrow) FileInfo;
    if (pFileInfo == nullptr)
        return E_OUTOFMEMORY;
    ZeroMemory(pFileInfo, sizeof(FileInfo));
    *ppFileInfo = pFileInfo;

    // Get the IDxDiagContainer object called "DxDiag_DirectXFiles".
    // This call may take some time while dxdiag gathers the info.
    hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectXFiles", &pObject);
    if (FAILED(hr) || pObject == nullptr)
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    if (FAILED(hr = GetStringValue(pObject, L"szDXFileNotesLocalized", EXPAND(pFileInfo->m_szDXFileNotesLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szDXFileNotesEnglish", EXPAND(pFileInfo->m_szDXFileNotesEnglish))))
        goto LCleanup; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pObject->GetNumberOfProps(&pFileInfo->m_nElementCount)))
        return hr;
    if (pFileInfo->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pFileInfo recorded");
#endif

    SAFE_RELEASE(pObject);

    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectXFiles", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        FileNode* pFileNode = new (std::nothrow) FileNode;
        if (pFileNode == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pFileNode, sizeof(FileNode));

        // Add pFileNode to pFileInfo->m_vDxComponentsFiles
        pFileInfo->m_vDxComponentsFiles.push_back(pFileNode);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szName", EXPAND(pFileNode->m_szName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVersion", EXPAND(pFileNode->m_szVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLanguageEnglish", EXPAND(pFileNode->m_szLanguageEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLanguageLocalized", EXPAND(pFileNode->m_szLanguageLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFileTimeLow", &pFileNode->m_FileTime.dwLowDateTime)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFileTimeHigh", &pFileNode->m_FileTime.dwHighDateTime)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDatestampEnglish", EXPAND(pFileNode->m_szDatestampEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDatestampLocalized", EXPAND(pFileNode->m_szDatestampLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szAttributes", EXPAND(pFileNode->m_szAttributes))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lNumBytes", &pFileNode->m_lNumBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bExists", &pFileNode->m_bExists)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bBeta", &pFileNode->m_bBeta)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDebug", &pFileNode->m_bDebug)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bObsolete", &pFileNode->m_bObsolete)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bProblem", &pFileNode->m_bProblem)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pFileNode->m_nElementCount)))
            return hr;
        if (pFileNode->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pFileNode recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetDisplayInfo()
// Desc: Get the display info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetDisplayInfo(std::vector <DisplayInfo*>& vDisplayInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    // Get the IDxDiagContainer object called "DxDiag_DisplayDevices".
    // This call may take some time while dxdiag gathers the info.
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DisplayDevices", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        DisplayInfo* pDisplayInfo = new (std::nothrow) DisplayInfo;
        if (pDisplayInfo == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pDisplayInfo, sizeof(DisplayInfo));

        // Add pDisplayInfo to vDisplayInfo
        vDisplayInfo.push_back(pDisplayInfo);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szDeviceName", EXPAND(pDisplayInfo->m_szDeviceName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pDisplayInfo->m_szDescription))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szKeyDeviceID", EXPAND(pDisplayInfo->m_szKeyDeviceID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szKeyDeviceKey", EXPAND(pDisplayInfo->m_szKeyDeviceKey))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szManufacturer", EXPAND(pDisplayInfo->m_szManufacturer))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szChipType", EXPAND(pDisplayInfo->m_szChipType))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDACType", EXPAND(pDisplayInfo->m_szDACType))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szRevision", EXPAND(pDisplayInfo->m_szRevision))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDisplayMemoryLocalized",
            EXPAND(pDisplayInfo->m_szDisplayMemoryLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDisplayMemoryEnglish",
            EXPAND(pDisplayInfo->m_szDisplayMemoryEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDisplayModeLocalized",
            EXPAND(pDisplayInfo->m_szDisplayModeLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDisplayModeEnglish", EXPAND(pDisplayInfo->m_szDisplayModeEnglish))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetUIntValue(pObject, L"dwWidth", &pDisplayInfo->m_dwWidth)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwHeight", &pDisplayInfo->m_dwHeight)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwBpp", &pDisplayInfo->m_dwBpp)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwRefreshRate", &pDisplayInfo->m_dwRefreshRate)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szMonitorName", EXPAND(pDisplayInfo->m_szMonitorName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szMonitorMaxRes", EXPAND(pDisplayInfo->m_szMonitorMaxRes))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szDriverName", EXPAND(pDisplayInfo->m_szDriverName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverVersion", EXPAND(pDisplayInfo->m_szDriverVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverAttributes", EXPAND(pDisplayInfo->m_szDriverAttributes))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageEnglish",
            EXPAND(pDisplayInfo->m_szDriverLanguageEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageLocalized",
            EXPAND(pDisplayInfo->m_szDriverLanguageLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateEnglish", EXPAND(pDisplayInfo->m_szDriverDateEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateLocalized",
            EXPAND(pDisplayInfo->m_szDriverDateLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lDriverSize", &pDisplayInfo->m_lDriverSize)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szMiniVdd", EXPAND(pDisplayInfo->m_szMiniVdd))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szMiniVddDateLocalized",
            EXPAND(pDisplayInfo->m_szMiniVddDateLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szMiniVddDateEnglish", EXPAND(pDisplayInfo->m_szMiniVddDateEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lMiniVddSize", &pDisplayInfo->m_lMiniVddSize)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVdd", EXPAND(pDisplayInfo->m_szVdd))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetBoolValue(pObject, L"bCanRenderWindow", &pDisplayInfo->m_bCanRenderWindow)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverBeta", &pDisplayInfo->m_bDriverBeta)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverDebug", &pDisplayInfo->m_bDriverDebug)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverSigned", &pDisplayInfo->m_bDriverSigned)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverSignedValid", &pDisplayInfo->m_bDriverSignedValid)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDeviceIdentifier", EXPAND(pDisplayInfo->m_szDeviceIdentifier))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverSignDate", EXPAND(pDisplayInfo->m_szDriverSignDate))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwDDIVersion", &pDisplayInfo->m_dwDDIVersion)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDDIVersionEnglish", EXPAND(pDisplayInfo->m_szDDIVersionEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDDIVersionLocalized",
            EXPAND(pDisplayInfo->m_szDDIVersionLocalized))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetUIntValue(pObject, L"iAdapter", &pDisplayInfo->m_iAdapter)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVendorId", EXPAND(pDisplayInfo->m_szVendorId))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDeviceId", EXPAND(pDisplayInfo->m_szDeviceId))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szSubSysId", EXPAND(pDisplayInfo->m_szSubSysId))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szRevisionId", EXPAND(pDisplayInfo->m_szRevisionId))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwWHQLLevel", &pDisplayInfo->m_dwWHQLLevel)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetBoolValue(pObject, L"bNoHardware", &pDisplayInfo->m_bNoHardware)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject,
            L"bDDAccelerationEnabled", &pDisplayInfo->m_bDDAccelerationEnabled)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"b3DAccelerationExists", &pDisplayInfo->m_b3DAccelerationExists)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject,
            L"b3DAccelerationEnabled", &pDisplayInfo->m_b3DAccelerationEnabled)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bAGPEnabled", &pDisplayInfo->m_bAGPEnabled)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bAGPExists", &pDisplayInfo->m_bAGPExists)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bAGPExistenceValid", &pDisplayInfo->m_bAGPExistenceValid)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDXVAModes", EXPAND(pDisplayInfo->m_szDXVAModes))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szDDStatusLocalized", EXPAND(pDisplayInfo->m_szDDStatusLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDDStatusEnglish", EXPAND(pDisplayInfo->m_szDDStatusEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szD3DStatusLocalized", EXPAND(pDisplayInfo->m_szD3DStatusLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szD3DStatusEnglish", EXPAND(pDisplayInfo->m_szD3DStatusEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szAGPStatusLocalized", EXPAND(pDisplayInfo->m_szAGPStatusLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szAGPStatusEnglish", EXPAND(pDisplayInfo->m_szAGPStatusEnglish))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szNotesLocalized", EXPAND(pDisplayInfo->m_szNotesLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szNotesEnglish", EXPAND(pDisplayInfo->m_szNotesEnglish))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szRegHelpText", EXPAND(pDisplayInfo->m_szRegHelpText))))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szTestResultDDLocalized",
            EXPAND(pDisplayInfo->m_szTestResultDDLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultDDEnglish",
            EXPAND(pDisplayInfo->m_szTestResultDDEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D7Localized",
            EXPAND(pDisplayInfo->m_szTestResultD3D7Localized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D7English",
            EXPAND(pDisplayInfo->m_szTestResultD3D7English))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D8Localized",
            EXPAND(pDisplayInfo->m_szTestResultD3D8Localized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D8English",
            EXPAND(pDisplayInfo->m_szTestResultD3D8English))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D9Localized",
            EXPAND(pDisplayInfo->m_szTestResultD3D9Localized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultD3D9English",
            EXPAND(pDisplayInfo->m_szTestResultD3D9English))))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pDisplayInfo->m_nElementCount)))
            return hr;
        if (pDisplayInfo->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pDisplayInfo recorded");
#endif

        GatherDXVA_DeinterlaceCaps(pObject, pDisplayInfo->m_vDXVACaps);

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GatherDXVA_DeinterlaceCaps
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GatherDXVA_DeinterlaceCaps(IDxDiagContainer* pParent,
    std::vector <DxDiag_DXVA_DeinterlaceCaps*>& vDXVACaps)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    if (FAILED(hr = pParent->GetChildContainer(L"DXVADeinterlaceCaps", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        DxDiag_DXVA_DeinterlaceCaps* pDXVANode = new DxDiag_DXVA_DeinterlaceCaps;
        if (pDXVANode == nullptr)
            return E_OUTOFMEMORY;

        // Add pDXVANode to vDXVACaps
        vDXVACaps.push_back(pDXVANode);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szD3DInputFormat", EXPAND(pDXVANode->szD3DInputFormat))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szD3DOutputFormat", EXPAND(pDXVANode->szD3DOutputFormat))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pDXVANode->szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szCaps", EXPAND(pDXVANode->szCaps))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwNumPreviousOutputFrames", &pDXVANode->dwNumPreviousOutputFrames)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwNumForwardRefSamples", &pDXVANode->dwNumForwardRefSamples)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwNumBackwardRefSamples", &pDXVANode->dwNumBackwardRefSamples)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pDXVANode->m_nElementCount)))
            goto LCleanup;
        if (pDXVANode->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pDXVANode recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetSoundInfo()
// Desc: Get the sound info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetSoundInfo(std::vector <SoundInfo*>& vSoundInfos, std::vector <SoundCaptureInfo*>& vSoundCaptureInfos)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    // Get the IDxDiagContainer object called "DxDiag_DirectSound.DxDiag_SoundDevices".
    // This call may take some time while dxdiag gathers the info.
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectSound.DxDiag_SoundDevices", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        SoundInfo* pSoundInfo = new SoundInfo;
        if (pSoundInfo == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pSoundInfo, sizeof(SoundInfo));

        // Add pSoundInfo to vSoundInfos
        vSoundInfos.push_back(pSoundInfo);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetUIntValue(pObject, L"dwDevnode", &pSoundInfo->m_dwDevnode)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuidDeviceID", EXPAND(pSoundInfo->m_szGuidDeviceID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szHardwareID", EXPAND(pSoundInfo->m_szHardwareID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szRegKey", EXPAND(pSoundInfo->m_szRegKey))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szManufacturerID", EXPAND(pSoundInfo->m_szManufacturerID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szProductID", EXPAND(pSoundInfo->m_szProductID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pSoundInfo->m_szDescription))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverName", EXPAND(pSoundInfo->m_szDriverName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverPath", EXPAND(pSoundInfo->m_szDriverPath))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverVersion", EXPAND(pSoundInfo->m_szDriverVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageEnglish",
            EXPAND(pSoundInfo->m_szDriverLanguageEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageLocalized",
            EXPAND(pSoundInfo->m_szDriverLanguageLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverAttributes", EXPAND(pSoundInfo->m_szDriverAttributes))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateEnglish", EXPAND(pSoundInfo->m_szDriverDateEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateLocalized", EXPAND(pSoundInfo->m_szDriverDateLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szOtherDrivers", EXPAND(pSoundInfo->m_szOtherDrivers))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szProvider", EXPAND(pSoundInfo->m_szProvider))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szType", EXPAND(pSoundInfo->m_szType))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lNumBytes", &pSoundInfo->m_lNumBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverBeta", &pSoundInfo->m_bDriverBeta)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverDebug", &pSoundInfo->m_bDriverDebug)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverSigned", &pSoundInfo->m_bDriverSigned)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverSignedValid", &pSoundInfo->m_bDriverSignedValid)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lAccelerationLevel", &pSoundInfo->m_lAccelerationLevel)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetBoolValue(pObject, L"bDefaultSoundPlayback", &pSoundInfo->m_bDefaultSoundPlayback)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDefaultVoicePlayback", &pSoundInfo->m_bDefaultVoicePlayback)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bVoiceManager", &pSoundInfo->m_bVoiceManager)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bEAX20Listener", &pSoundInfo->m_bEAX20Listener)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bEAX20Source", &pSoundInfo->m_bEAX20Source)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bI3DL2Listener", &pSoundInfo->m_bI3DL2Listener)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bI3DL2Source", &pSoundInfo->m_bI3DL2Source)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bZoomFX", &pSoundInfo->m_bZoomFX)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetUIntValue(pObject, L"dwFlags", &pSoundInfo->m_dwFlags)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMinSecondarySampleRate", &pSoundInfo->m_dwMinSecondarySampleRate)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxSecondarySampleRate", &pSoundInfo->m_dwMaxSecondarySampleRate)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwPrimaryBuffers", &pSoundInfo->m_dwPrimaryBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxHwMixingAllBuffers", &pSoundInfo->m_dwMaxHwMixingAllBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxHwMixingStaticBuffers", &pSoundInfo->m_dwMaxHwMixingStaticBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxHwMixingStreamingBuffers",
            &pSoundInfo->m_dwMaxHwMixingStreamingBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwFreeHwMixingAllBuffers", &pSoundInfo->m_dwFreeHwMixingAllBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwFreeHwMixingStaticBuffers", &pSoundInfo->m_dwFreeHwMixingStaticBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwFreeHwMixingStreamingBuffers",
            &pSoundInfo->m_dwFreeHwMixingStreamingBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMaxHw3DAllBuffers", &pSoundInfo->m_dwMaxHw3DAllBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMaxHw3DStaticBuffers", &pSoundInfo->m_dwMaxHw3DStaticBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxHw3DStreamingBuffers", &pSoundInfo->m_dwMaxHw3DStreamingBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFreeHw3DAllBuffers", &pSoundInfo->m_dwFreeHw3DAllBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwFreeHw3DStaticBuffers", &pSoundInfo->m_dwFreeHw3DStaticBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwFreeHw3DStreamingBuffers", &pSoundInfo->m_dwFreeHw3DStreamingBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwTotalHwMemBytes", &pSoundInfo->m_dwTotalHwMemBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFreeHwMemBytes", &pSoundInfo->m_dwFreeHwMemBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwMaxContigFreeHwMemBytes", &pSoundInfo->m_dwMaxContigFreeHwMemBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwUnlockTransferRateHwBuffers",
            &pSoundInfo->m_dwUnlockTransferRateHwBuffers)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject,
            L"dwPlayCpuOverheadSwBuffers", &pSoundInfo->m_dwPlayCpuOverheadSwBuffers)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetStringValue(pObject, L"szNotesLocalized", EXPAND(pSoundInfo->m_szNotesLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szNotesEnglish", EXPAND(pSoundInfo->m_szNotesEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szRegHelpText", EXPAND(pSoundInfo->m_szRegHelpText))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultLocalized", EXPAND(pSoundInfo->m_szTestResultLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szTestResultEnglish", EXPAND(pSoundInfo->m_szTestResultEnglish))))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pSoundInfo->m_nElementCount)))
            return hr;
        if (pSoundInfo->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pSoundInfo recorded");
#endif

        SAFE_RELEASE(pObject);
    }

    SAFE_RELEASE(pContainer);

    // Get the IDxDiagContainer object called "DxDiag_DirectSound.DxDiag_SoundCaptureDevices".
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectSound.DxDiag_SoundCaptureDevices",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        SoundCaptureInfo* pSoundCaptureInfo = new (std::nothrow) SoundCaptureInfo;
        if (pSoundCaptureInfo == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pSoundCaptureInfo, sizeof(SoundCaptureInfo));

        // Add pSoundCaptureInfo to vSoundCaptureInfos
        vSoundCaptureInfos.push_back(pSoundCaptureInfo);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pSoundCaptureInfo->m_szDescription))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuidDeviceID", EXPAND(pSoundCaptureInfo->m_szGuidDeviceID))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverName", EXPAND(pSoundCaptureInfo->m_szDriverName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverPath", EXPAND(pSoundCaptureInfo->m_szDriverPath))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverVersion", EXPAND(pSoundCaptureInfo->m_szDriverVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageEnglish",
            EXPAND(pSoundCaptureInfo->m_szDriverLanguageEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverLanguageLocalized",
            EXPAND(pSoundCaptureInfo->m_szDriverLanguageLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverAttributes",
            EXPAND(pSoundCaptureInfo->m_szDriverAttributes))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateEnglish",
            EXPAND(pSoundCaptureInfo->m_szDriverDateEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDriverDateLocalized",
            EXPAND(pSoundCaptureInfo->m_szDriverDateLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lNumBytes", &pSoundCaptureInfo->m_lNumBytes)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverBeta", &pSoundCaptureInfo->m_bDriverBeta)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDriverDebug", &pSoundCaptureInfo->m_bDriverDebug)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetBoolValue(pObject,
            L"bDefaultSoundRecording", &pSoundCaptureInfo->m_bDefaultSoundRecording)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject,
            L"bDefaultVoiceRecording", &pSoundCaptureInfo->m_bDefaultVoiceRecording)))
            goto LCleanup; nCurCount++;

        if (FAILED(hr = GetUIntValue(pObject, L"dwFlags", &pSoundCaptureInfo->m_dwFlags)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFormats", &pSoundCaptureInfo->m_dwFormats)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pSoundCaptureInfo->m_nElementCount)))
            return hr;
        if (pSoundCaptureInfo->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pSoundCaptureInfo recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetMusicInfo()
// Desc: Get the music info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetMusicInfo(MusicInfo** ppMusicInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;
    MusicInfo* pMusicInfo = nullptr;

    pMusicInfo = new (std::nothrow) MusicInfo;
    if (nullptr == pMusicInfo)
        return E_OUTOFMEMORY;
    ZeroMemory(pMusicInfo, sizeof(MusicInfo));
    *ppMusicInfo = pMusicInfo;

    // Get the IDxDiagContainer object called "DxDiag_DirectMusic".
    // This call may take some time while dxdiag gathers the info.
    hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectMusic", &pObject);
    if (FAILED(hr) || pObject == nullptr)
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    if (FAILED(hr = GetBoolValue(pObject, L"bDMusicInstalled", &pMusicInfo->m_bDMusicInstalled)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szGMFilePath", EXPAND(pMusicInfo->m_szGMFilePath))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szGMFileVersion", EXPAND(pMusicInfo->m_szGMFileVersion))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bAccelerationEnabled", &pMusicInfo->m_bAccelerationEnabled)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetBoolValue(pObject, L"bAccelerationExists", &pMusicInfo->m_bAccelerationExists)))
        goto LCleanup; nCurCount++;

    if (FAILED(hr = GetStringValue(pObject, L"szNotesLocalized", EXPAND(pMusicInfo->m_szNotesLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szNotesEnglish", EXPAND(pMusicInfo->m_szNotesEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szRegHelpText", EXPAND(pMusicInfo->m_szRegHelpText))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTestResultLocalized", EXPAND(pMusicInfo->m_szTestResultLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTestResultEnglish", EXPAND(pMusicInfo->m_szTestResultEnglish))))
        goto LCleanup; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pObject->GetNumberOfProps(&pMusicInfo->m_nElementCount)))
        return hr;
    if (pMusicInfo->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pMusicInfo recorded");
#endif

    SAFE_RELEASE(pObject);

    // Get the number of "DxDiag_DirectMusic.DxDiag_DirectMusicPorts" objects in the dll
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectMusic.DxDiag_DirectMusicPorts", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        MusicPort* pMusicPort = new (std::nothrow) MusicPort;
        if (pMusicPort == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pMusicPort, sizeof(MusicPort));

        // Add pMusicPort to pMusicInfo->m_vMusicPorts
        pMusicInfo->m_vMusicPorts.push_back(pMusicPort);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pMusicPort->m_szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bSoftware", &pMusicPort->m_bSoftware)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bKernelMode", &pMusicPort->m_bKernelMode)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bUsesDLS", &pMusicPort->m_bUsesDLS)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bExternal", &pMusicPort->m_bExternal)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMaxAudioChannels", &pMusicPort->m_dwMaxAudioChannels)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMaxChannelGroups", &pMusicPort->m_dwMaxChannelGroups)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bDefaultPort", &pMusicPort->m_bDefaultPort)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bOutputPort", &pMusicPort->m_bOutputPort)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pMusicPort->m_szDescription))))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pMusicPort->m_nElementCount)))
            return hr;
        if (pMusicPort->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pMusicPort recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetInputInfo()
// Desc: Get the input info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetInputInfo(InputInfo** ppInputInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pChild = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;
    InputInfo* pInputInfo = nullptr;

    pInputInfo = new (std::nothrow) InputInfo;
    if (nullptr == pInputInfo)
        return E_OUTOFMEMORY;
    ZeroMemory(pInputInfo, sizeof(InputInfo));
    *ppInputInfo = pInputInfo;

    // Get the IDxDiagContainer object called "DxDiag_DirectInput".
    // This call may take some time while dxdiag gathers the info.
    hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectInput", &pObject);
    if (FAILED(hr) || pObject == nullptr)
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    if (FAILED(hr = GetBoolValue(pObject, L"bPollFlags", &pInputInfo->m_bPollFlags)))
        return hr; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szInputNotesLocalized", EXPAND(pInputInfo->m_szInputNotesLocalized))))
        return hr; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szInputNotesEnglish", EXPAND(pInputInfo->m_szInputNotesEnglish))))
        return hr; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szRegHelpText", EXPAND(pInputInfo->m_szRegHelpText))))
        return hr; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pObject->GetNumberOfProps(&pInputInfo->m_nElementCount)))
        return hr;
    if (pInputInfo->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pInputInfo recorded");
#endif

    SAFE_RELEASE(pObject);

    // Get the number of "DxDiag_DirectInput.DxDiag_DirectInputDevices" objects in the dll
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectInput.DxDiag_DirectInputDevices",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        InputDeviceInfo* pInputDevice = new (std::nothrow) InputDeviceInfo;
        if (pInputDevice == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pInputDevice, sizeof(InputDeviceInfo));

        // Add pInputDevice to pInputInfo->m_vDirectInputDevices
        pInputInfo->m_vDirectInputDevices.push_back(pInputDevice);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szInstanceName", EXPAND(pInputDevice->m_szInstanceName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bAttached", &pInputDevice->m_bAttached)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwVendorID", &pInputDevice->m_dwVendorID)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwProductID", &pInputDevice->m_dwProductID)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwJoystickID", &pInputDevice->m_dwJoystickID)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwDevType", &pInputDevice->m_dwDevType)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFFDriverName", EXPAND(pInputDevice->m_szFFDriverName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFFDriverDateEnglish",
            EXPAND(pInputDevice->m_szFFDriverDateEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFFDriverVersion", EXPAND(pInputDevice->m_szFFDriverVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetIntValue(pObject, L"lFFDriverSize", &pInputDevice->m_lFFDriverSize)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pInputDevice->m_nElementCount)))
            return hr;
        if (pInputDevice->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pInputDevice recorded");
#endif

        SAFE_RELEASE(pObject);
    }

    SAFE_RELEASE(pContainer);

    // Get "DxDiag_DirectInput.DxDiag_DirectInputGameports" tree
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectInput.DxDiag_DirectInputGameports",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;
    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        InputRelatedDeviceInfo* pInputRelatedDevice = new InputRelatedDeviceInfo;
        if (pInputRelatedDevice == nullptr)
            return E_OUTOFMEMORY;
        m_pInputInfo->m_vGamePortDevices.push_back(pInputRelatedDevice);
        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pChild);
        if (FAILED(hr) || pChild == nullptr)
        {
            if (pChild == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }
        GatherInputRelatedDeviceInst(pInputRelatedDevice, pChild);
        SAFE_RELEASE(pChild);
    }
    SAFE_RELEASE(pContainer);

    // Get "DxDiag_DirectInput.DxDiag_DirectInputUSBRoot" tree
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectInput.DxDiag_DirectInputUSBRoot",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;
    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        InputRelatedDeviceInfo* pInputRelatedDevice = new InputRelatedDeviceInfo;
        if (pInputRelatedDevice == nullptr)
            return E_OUTOFMEMORY;
        m_pInputInfo->m_vUsbRoot.push_back(pInputRelatedDevice);
        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pChild);
        if (FAILED(hr) || pChild == nullptr)
        {
            if (pChild == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }
        GatherInputRelatedDeviceInst(pInputRelatedDevice, pChild);
        SAFE_RELEASE(pChild);
    }
    SAFE_RELEASE(pContainer);

    // Get "DxDiag_DirectInput.DxDiag_DirectInputPS2Devices" tree
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectInput.DxDiag_DirectInputPS2Devices",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;
    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        InputRelatedDeviceInfo* pInputRelatedDevice = new InputRelatedDeviceInfo;
        if (pInputRelatedDevice == nullptr)
            return E_OUTOFMEMORY;
        m_pInputInfo->m_vPS2Devices.push_back(pInputRelatedDevice);
        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pChild);
        if (FAILED(hr) || pChild == nullptr)
        {
            if (pChild == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }
        GatherInputRelatedDeviceInst(pInputRelatedDevice, pChild);
        SAFE_RELEASE(pChild);
    }
    SAFE_RELEASE(pContainer);

LCleanup:
    SAFE_RELEASE(pContainer);
    SAFE_RELEASE(pChild);
    SAFE_RELEASE(pObject);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GatherInputRelatedDeviceInst()
// Desc: Get the InputRelatedDeviceInfo tree from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GatherInputRelatedDeviceInst(InputRelatedDeviceInfo* pInputRelatedDevice,
    IDxDiagContainer* pContainer)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pChild = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;

    nCurCount = 0;

    if (FAILED(hr = GetUIntValue(pContainer, L"dwVendorID", &pInputRelatedDevice->m_dwVendorID)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetUIntValue(pContainer, L"dwProductID", &pInputRelatedDevice->m_dwProductID)))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szDescription", EXPAND(pInputRelatedDevice->m_szDescription))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szLocation", EXPAND(pInputRelatedDevice->m_szLocation))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szMatchingDeviceId",
        EXPAND(pInputRelatedDevice->m_szMatchingDeviceId))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szUpperFilters", EXPAND(pInputRelatedDevice->m_szUpperFilters))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szService", EXPAND(pInputRelatedDevice->m_szService))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szLowerFilters", EXPAND(pInputRelatedDevice->m_szLowerFilters))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szOEMData", EXPAND(pInputRelatedDevice->m_szOEMData))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szFlags1", EXPAND(pInputRelatedDevice->m_szFlags1))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pContainer, L"szFlags2", EXPAND(pInputRelatedDevice->m_szFlags2))))
        goto LCleanup; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pContainer->GetNumberOfProps(&pInputRelatedDevice->m_nElementCount)))
        goto LCleanup;
    if (pInputRelatedDevice->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pInputRelatedDevice recorded");
#endif

    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pChild);
        if (FAILED(hr) || pChild == nullptr)
        {
            if (pChild == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (wcscmp(wszContainer, L"Drivers") == 0)
        {
            if (FAILED(hr = GatherInputRelatedDeviceInstDrivers(pInputRelatedDevice, pChild)))
                goto LCleanup;
        }
        else
        {
            InputRelatedDeviceInfo* pChildInputRelatedDevice = new InputRelatedDeviceInfo;
            if (pChildInputRelatedDevice == nullptr)
            {
                hr = E_OUTOFMEMORY;
                goto LCleanup;
            }

            pInputRelatedDevice->m_vChildren.push_back(pChildInputRelatedDevice);

            if (FAILED(hr = GatherInputRelatedDeviceInst(pChildInputRelatedDevice, pChild)))
                goto LCleanup;
        }

        SAFE_RELEASE(pChild);
    }

LCleanup:
    SAFE_RELEASE(pChild);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GatherInputRelatedDeviceInstDrivers()
// Desc: Get the driver list and store it in a InputRelatedDeviceInfo node
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GatherInputRelatedDeviceInstDrivers(InputRelatedDeviceInfo* pInputRelatedDevice,
    IDxDiagContainer* pChild)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pDriverChild = nullptr;
    DWORD nChildInstanceCount = 0;

    if (FAILED(hr = pChild->GetNumberOfChildContainers(&nChildInstanceCount)))
        goto LCleanup;

    DWORD nFileItem;
    for (nFileItem = 0; nFileItem < nChildInstanceCount; nFileItem++)
    {
        hr = pChild->EnumChildContainerNames(nFileItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pChild->GetChildContainer(wszContainer, &pDriverChild);
        if (FAILED(hr) || pDriverChild == nullptr)
        {
            if (pDriverChild == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        FileNode* pFileNode = new FileNode;
        if (pFileNode == nullptr)
            return E_OUTOFMEMORY;

        pInputRelatedDevice->m_vDriverList.push_back(pFileNode);

        if (FAILED(hr = GatherFileNodeInst(pFileNode, pDriverChild)))
            goto LCleanup;

        SAFE_RELEASE(pDriverChild);
    }

LCleanup:
    SAFE_RELEASE(pDriverChild);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetNetworkInfo()
// Desc: Get the network info from the dll
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetNetworkInfo(NetInfo** ppNetInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;
    NetInfo* pNetInfo = nullptr;

    pNetInfo = new (std::nothrow) NetInfo;
    if (nullptr == pNetInfo)
        return E_OUTOFMEMORY;
    ZeroMemory(pNetInfo, sizeof(NetInfo));
    *ppNetInfo = pNetInfo;

    // Get the IDxDiagContainer object called "DxDiag_DirectPlay".
    // This call may take some time while dxdiag gathers the info.
    hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectPlay", &pObject);
    if (FAILED(hr) || pObject == nullptr)
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    if (FAILED(hr = GetStringValue(pObject, L"szNetworkNotesLocalized", EXPAND(pNetInfo->m_szNetworkNotesLocalized))))
        return hr; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szNetworkNotesEnglish", EXPAND(pNetInfo->m_szNetworkNotesEnglish))))
        return hr; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szRegHelpText", EXPAND(m_pNetInfo->m_szRegHelpText))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTestResultLocalized", EXPAND(pNetInfo->m_szTestResultLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szTestResultEnglish", EXPAND(pNetInfo->m_szTestResultEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardFullDuplexTestLocalized",
        EXPAND(m_pNetInfo->m_szVoiceWizardFullDuplexTestLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardHalfDuplexTestLocalized",
        EXPAND(m_pNetInfo->m_szVoiceWizardHalfDuplexTestLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardMicTestLocalized",
        EXPAND(m_pNetInfo->m_szVoiceWizardMicTestLocalized))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardFullDuplexTestEnglish",
        EXPAND(m_pNetInfo->m_szVoiceWizardFullDuplexTestEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardHalfDuplexTestEnglish",
        EXPAND(m_pNetInfo->m_szVoiceWizardHalfDuplexTestEnglish))))
        goto LCleanup; nCurCount++;
    if (FAILED(hr = GetStringValue(pObject, L"szVoiceWizardMicTestEnglish",
        EXPAND(m_pNetInfo->m_szVoiceWizardMicTestEnglish))))
        goto LCleanup; nCurCount++;

#ifdef _DEBUG
    // debug check to make sure we got all the info from the object
    if (FAILED(hr = pObject->GetNumberOfProps(&pNetInfo->m_nElementCount)))
        return hr;
    if (pNetInfo->m_nElementCount != nCurCount)
        OutputDebugStringW(L"Not all elements in pNetInfo recorded");
#endif

    SAFE_RELEASE(pObject);

    // Get the number of "DxDiag_DirectPlay.DxDiag_DirectPlayApps" objects in the dll
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectPlay.DxDiag_DirectPlayApps", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        NetApp* pNetApp = new (std::nothrow) NetApp;
        if (pNetApp == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pNetApp, sizeof(NetApp));

        // Add pNetApp to pNetInfo->m_vNetApps
        pNetInfo->m_vNetApps.push_back(pNetApp);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szName", EXPAND(pNetApp->m_szName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pNetApp->m_szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szExeFile", EXPAND(pNetApp->m_szExeFile))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szExePath", EXPAND(pNetApp->m_szExePath))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szExeVersionLocalized", EXPAND(pNetApp->m_szExeVersionLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szExeVersionEnglish", EXPAND(pNetApp->m_szExeVersionEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLauncherFile", EXPAND(pNetApp->m_szLauncherFile))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLauncherPath", EXPAND(pNetApp->m_szLauncherPath))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLauncherVersionLocalized",
            EXPAND(pNetApp->m_szLauncherVersionLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szLauncherVersionEnglish",
            EXPAND(pNetApp->m_szLauncherVersionEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bRegistryOK", &pNetApp->m_bRegistryOK)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bProblem", &pNetApp->m_bProblem)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bFileMissing", &pNetApp->m_bFileMissing)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwDXVer", &pNetApp->m_dwDXVer)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pNetApp->m_nElementCount)))
            return hr;
        if (pNetApp->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pNetApp recorded");
#endif

        SAFE_RELEASE(pObject);
    }

    SAFE_RELEASE(pContainer);

    // Get the number of "DxDiag_DirectPlaySP" objects in the dll
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectPlay.DxDiag_DirectPlaySPs", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        NetSP* pNetSP = new (std::nothrow) NetSP;
        if (pNetSP == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pNetSP, sizeof(NetSP));

        // Add pNetSP to pNetInfo->m_vNetSPs
        pNetInfo->m_vNetSPs.push_back(pNetSP);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szNameLocalized", EXPAND(pNetSP->m_szNameLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szNameEnglish", EXPAND(pNetSP->m_szNameEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pNetSP->m_szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFile", EXPAND(pNetSP->m_szFile))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szPath", EXPAND(pNetSP->m_szPath))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVersionLocalized", EXPAND(pNetSP->m_szVersionLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVersionEnglish", EXPAND(pNetSP->m_szVersionEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bRegistryOK", &pNetSP->m_bRegistryOK)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bProblem", &pNetSP->m_bProblem)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bFileMissing", &pNetSP->m_bFileMissing)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetBoolValue(pObject, L"bInstalled", &pNetSP->m_bInstalled)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwDXVer", &pNetSP->m_dwDXVer)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pNetSP->m_nElementCount)))
            return hr;
        if (pNetSP->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pNetSP recorded");
#endif

        SAFE_RELEASE(pObject);
    }

    SAFE_RELEASE(pContainer);

    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectPlay.DxDiag_DirectPlayAdapters",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        NetAdapter* pNetAdapter = new (std::nothrow) NetAdapter;
        if (pNetAdapter == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pNetAdapter, sizeof(NetAdapter));

        // Add pNetAdapter to m_pNetInfo->m_vNetAdapters
        m_pNetInfo->m_vNetAdapters.push_back(pNetAdapter);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szAdapterName", EXPAND(pNetAdapter->m_szAdapterName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szSPNameEnglish", EXPAND(pNetAdapter->m_szSPNameEnglish))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szSPNameLocalized", EXPAND(pNetAdapter->m_szSPNameLocalized))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pNetAdapter->m_szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFlags", &pNetAdapter->m_dwFlags)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pNetAdapter->m_nElementCount)))
            return hr;
        if (pNetAdapter->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pNetAdapter recorded");
#endif

        SAFE_RELEASE(pObject);
    }

    SAFE_RELEASE(pContainer);

    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectPlay.DxDiag_DirectPlayVoiceCodecs",
        &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        NetVoiceCodec* pNetVoiceCodec = new (std::nothrow) NetVoiceCodec;
        if (pNetVoiceCodec == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pNetVoiceCodec, sizeof(NetVoiceCodec));

        // Add pNetVoiceCodec to m_pNetInfo->m_vNetVoiceCodecs
        m_pNetInfo->m_vNetVoiceCodecs.push_back(pNetVoiceCodec);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szName", EXPAND(pNetVoiceCodec->m_szName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szGuid", EXPAND(pNetVoiceCodec->m_szGuid))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szDescription", EXPAND(pNetVoiceCodec->m_szDescription))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwFlags", &pNetVoiceCodec->m_dwFlags)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMaxBitsPerSecond", &pNetVoiceCodec->m_dwMaxBitsPerSecond)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pNetVoiceCodec->m_nElementCount)))
            return hr;
        if (pNetVoiceCodec->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pNetVoiceCodec recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetShowInfo()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetShowInfo(ShowInfo** ppShowInfo)
{
    HRESULT hr;
    WCHAR wszContainer[512];
    IDxDiagContainer* pContainer = nullptr;
    IDxDiagContainer* pObject = nullptr;
    DWORD nInstanceCount = 0;
    DWORD nItem = 0;
    DWORD nCurCount = 0;
    ShowInfo* pShowInfo = nullptr;

    pShowInfo = new (std::nothrow) ShowInfo;
    if (nullptr == pShowInfo)
        return E_OUTOFMEMORY;
    ZeroMemory(pShowInfo, sizeof(ShowInfo));
    *ppShowInfo = pShowInfo;

    // Get the IDxDiagContainer object called "DxDiag_DirectShowFilters".
    // This call may take some time while dxdiag gathers the info.
    if (FAILED(hr = m_pDxDiagRoot->GetChildContainer(L"DxDiag_DirectShowFilters", &pContainer)))
        goto LCleanup;
    if (FAILED(hr = pContainer->GetNumberOfChildContainers(&nInstanceCount)))
        goto LCleanup;

    for (nItem = 0; nItem < nInstanceCount; nItem++)
    {
        nCurCount = 0;

        ShowFilterInfo* pShowFilter = new (std::nothrow) ShowFilterInfo;
        if (pShowFilter == nullptr)
            return E_OUTOFMEMORY;
        ZeroMemory(pShowFilter, sizeof(ShowFilterInfo));

        // Add pShowFilter to pShowInfo->m_vShowFilters
        pShowInfo->m_vShowFilters.push_back(pShowFilter);

        hr = pContainer->EnumChildContainerNames(nItem, wszContainer, 512);
        if (FAILED(hr))
            goto LCleanup;
        hr = pContainer->GetChildContainer(wszContainer, &pObject);
        if (FAILED(hr) || pObject == nullptr)
        {
            if (pObject == nullptr)
                hr = E_FAIL;
            goto LCleanup;
        }

        if (FAILED(hr = GetStringValue(pObject, L"szName", EXPAND(pShowFilter->m_szName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szVersion", EXPAND(pShowFilter->m_szVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"ClsidFilter", EXPAND(pShowFilter->m_ClsidFilter))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFileName", EXPAND(pShowFilter->m_szFileName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szFileVersion", EXPAND(pShowFilter->m_szFileVersion))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"szCatName", EXPAND(pShowFilter->m_szCatName))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetStringValue(pObject, L"ClsidCat", EXPAND(pShowFilter->m_ClsidCat))))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwInputs", &pShowFilter->m_dwInputs)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwOutputs", &pShowFilter->m_dwOutputs)))
            goto LCleanup; nCurCount++;
        if (FAILED(hr = GetUIntValue(pObject, L"dwMerit", &pShowFilter->m_dwMerit)))
            goto LCleanup; nCurCount++;

#ifdef _DEBUG
        // debug check to make sure we got all the info from the object
        if (FAILED(hr = pObject->GetNumberOfProps(&pShowFilter->m_nElementCount)))
            return hr;
        if (pShowFilter->m_nElementCount != nCurCount)
            OutputDebugStringW(L"Not all elements in pShowFilter recorded");
#endif

        SAFE_RELEASE(pObject);
    }

LCleanup:
    SAFE_RELEASE(pObject);
    SAFE_RELEASE(pContainer);
    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetStringValue()
// Desc: Get a string value from a IDxDiagContainer object
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetStringValue(IDxDiagContainer* pObject, WCHAR* wstrName, WCHAR* strValue, int nStrLen)
{
    HRESULT hr;
    VARIANT var;
    VariantInit(&var);

    if (FAILED(hr = pObject->GetProp(wstrName, &var)))
        return hr;

    if (var.vt != VT_BSTR)
        return E_INVALIDARG;

#ifdef _UNICODE
    wcsncpy(strValue, var.bstrVal, nStrLen - 1);
#else
    wcstombs(strValue, var.bstrVal, nStrLen);
#endif
    strValue[nStrLen - 1] = L'\0';
    VariantClear(&var);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetUIntValue()
// Desc: Get a UINT value from a IDxDiagContainer object
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetUIntValue(IDxDiagContainer* pObject, WCHAR* wstrName, DWORD* pdwValue)
{
    HRESULT hr;
    VARIANT var;
    VariantInit(&var);

    if (FAILED(hr = pObject->GetProp(wstrName, &var)))
        return hr;

    if (var.vt != VT_UI4)
        return E_INVALIDARG;

    *pdwValue = var.ulVal;
    VariantClear(&var);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetIntValue()
// Desc: Get a INT value from a IDxDiagContainer object
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetIntValue(IDxDiagContainer* pObject, WCHAR* wstrName, LONG* pnValue)
{
    HRESULT hr;
    VARIANT var;
    VariantInit(&var);

    if (FAILED(hr = pObject->GetProp(wstrName, &var)))
        return hr;

    if (var.vt != VT_I4)
        return E_INVALIDARG;

    *pnValue = var.lVal;
    VariantClear(&var);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetBoolValue()
// Desc: Get a BOOL value from a IDxDiagContainer object
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetBoolValue(IDxDiagContainer* pObject, WCHAR* wstrName, BOOL* pbValue)
{
    HRESULT hr;
    VARIANT var;
    VariantInit(&var);

    if (FAILED(hr = pObject->GetProp(wstrName, &var)))
        return hr;

    if (var.vt != VT_BOOL)
        return E_INVALIDARG;

    *pbValue = (var.boolVal != 0);
    VariantClear(&var);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetInt64Value()
// Desc: Get a ULONGLONG value from a IDxDiagContainer object
//-----------------------------------------------------------------------------
HRESULT CDxDiagInfo::GetInt64Value(IDxDiagContainer* pObject, WCHAR* wstrName, ULONGLONG* pullValue)
{
    HRESULT hr;
    VARIANT var;
    VariantInit(&var);

    if (FAILED(hr = pObject->GetProp(wstrName, &var)))
        return hr;

    // 64-bit values are stored as strings in BSTRs
    if (var.vt != VT_BSTR)
        return E_INVALIDARG;

    *pullValue = _wtoi64(var.bstrVal);
    VariantClear(&var);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DestroySystemDevice()
// Desc: 
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroySystemDevice(std::vector <SystemDevice*>& vSystemDevices)
{
    for (auto iter = vSystemDevices.begin(); iter != vSystemDevices.end(); iter++)
    {
        SystemDevice* pSystemDevice = *iter;
        SAFE_DELETE(pSystemDevice);
    }
    vSystemDevices.clear();
}




//-----------------------------------------------------------------------------
// Name: DestroyFileList()
// Desc: Cleanup the file list 
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyFileList(FileInfo* pFileInfo)
{
    if (pFileInfo)
    {
        for (auto iter = pFileInfo->m_vDxComponentsFiles.begin(); iter != pFileInfo->m_vDxComponentsFiles.end(); iter++)
        {
            FileNode* pFileNode = *iter;
            SAFE_DELETE(pFileNode);
        }
        pFileInfo->m_vDxComponentsFiles.clear();

        SAFE_DELETE(pFileInfo);
    }
}




//-----------------------------------------------------------------------------
// Name: DestroyDisplayInfo()
// Desc: Cleanup the display info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyDisplayInfo(std::vector <DisplayInfo*>& vDisplayInfo)
{
    for (auto iter = vDisplayInfo.begin(); iter != vDisplayInfo.end(); iter++)
    {
        DisplayInfo* pDisplayInfo = *iter;

        for (auto iterDXVA = pDisplayInfo->m_vDXVACaps.begin(); iterDXVA != pDisplayInfo->m_vDXVACaps.end(); iterDXVA++)
        {
            DxDiag_DXVA_DeinterlaceCaps* pDXVANode = *iterDXVA;
            SAFE_DELETE(pDXVANode);
        }
        if (pDisplayInfo)
        {
            pDisplayInfo->m_vDXVACaps.clear();
        }

        SAFE_DELETE(pDisplayInfo);
    }
    vDisplayInfo.clear();
}




//-----------------------------------------------------------------------------
// Name: DestroyInputInfo()
// Desc: Cleanup the input info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyInputInfo(InputInfo* pInputInfo)
{
    if (pInputInfo)
    {
        for (auto iter = pInputInfo->m_vDirectInputDevices.begin(); iter != pInputInfo->m_vDirectInputDevices.end();
            iter++)
        {
            InputDeviceInfo* pInputDeviceInfoDelete = *iter;
            SAFE_DELETE(pInputDeviceInfoDelete);
        }
        pInputInfo->m_vDirectInputDevices.clear();

        DeleteInputTree(pInputInfo->m_vGamePortDevices);
        DeleteInputTree(pInputInfo->m_vUsbRoot);
        DeleteInputTree(pInputInfo->m_vPS2Devices);

        SAFE_DELETE(pInputInfo);
    }
}




//-----------------------------------------------------------------------------
// Name: DeleteInputTree
// Desc: 
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DeleteInputTree(std::vector <InputRelatedDeviceInfo*>& vDeviceList)
{
    for (auto iter = vDeviceList.begin(); iter != vDeviceList.end(); iter++)
    {
        InputRelatedDeviceInfo* pInputNode = *iter;
        if (pInputNode)
        {

            if (!pInputNode->m_vChildren.empty())
                DeleteInputTree(pInputNode->m_vChildren);

            DeleteFileList(pInputNode->m_vDriverList);
        }
        SAFE_DELETE(pInputNode);
    }
    vDeviceList.clear();
}




//-----------------------------------------------------------------------------
// Name: DeleteFileList()
// Desc: 
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DeleteFileList(std::vector <FileNode*>& vDriverList)
{
    for (auto iter = vDriverList.begin(); iter != vDriverList.end(); iter++)
    {
        FileNode* pFileNodeDelete = *iter;
        SAFE_DELETE(pFileNodeDelete);
    }
    vDriverList.clear();
}




//-----------------------------------------------------------------------------
// Name: DestroyMusicInfo()
// Desc: Cleanup the music info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyMusicInfo(MusicInfo* pMusicInfo)
{
    if (pMusicInfo)
    {
        for (auto iter = pMusicInfo->m_vMusicPorts.begin(); iter != pMusicInfo->m_vMusicPorts.end(); iter++)
        {
            MusicPort* pMusicPort = *iter;
            SAFE_DELETE(pMusicPort);
        }
        pMusicInfo->m_vMusicPorts.clear();

        SAFE_DELETE(pMusicInfo);
    }
}




//-----------------------------------------------------------------------------
// Name: DestroyNetworkInfo()
// Desc: Cleanup the network info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyNetworkInfo(NetInfo* pNetInfo)
{
    if (pNetInfo)
    {
        for (auto iterNetApp = pNetInfo->m_vNetApps.begin(); iterNetApp != pNetInfo->m_vNetApps.end(); iterNetApp++)
        {
            NetApp* pNetApp = *iterNetApp;
            SAFE_DELETE(pNetApp);
        }
        pNetInfo->m_vNetApps.clear();

        for (auto iterNetSP = pNetInfo->m_vNetSPs.begin(); iterNetSP != pNetInfo->m_vNetSPs.end(); iterNetSP++)
        {
            NetSP* pNetSP = *iterNetSP;
            SAFE_DELETE(pNetSP);
        }
        pNetInfo->m_vNetSPs.clear();

        for (auto iterNetAdapter = pNetInfo->m_vNetAdapters.begin(); iterNetAdapter != pNetInfo->m_vNetAdapters.end();
            iterNetAdapter++)
        {
            NetAdapter* pNetAdapter = *iterNetAdapter;
            SAFE_DELETE(pNetAdapter);
        }
        pNetInfo->m_vNetAdapters.clear();

        for (auto iterNetCodec = pNetInfo->m_vNetVoiceCodecs.begin(); iterNetCodec != pNetInfo->m_vNetVoiceCodecs.end();
            iterNetCodec++)
        {
            NetVoiceCodec* pNetVoiceCodec = *iterNetCodec;
            SAFE_DELETE(pNetVoiceCodec);
        }
        pNetInfo->m_vNetVoiceCodecs.clear();

        SAFE_DELETE(pNetInfo);
    }
}




//-----------------------------------------------------------------------------
// Name: DestroySoundInfo()
// Desc: Cleanup the sound info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroySoundInfo(std::vector <SoundInfo*>& vSoundInfos)
{
    for (auto iter = vSoundInfos.begin(); iter != vSoundInfos.end(); iter++)
    {
        SoundInfo* pSoundInfo = *iter;
        SAFE_DELETE(pSoundInfo);
    }
    vSoundInfos.clear();
}




//-----------------------------------------------------------------------------
// Name: DestroySoundCaptureInfo()
// Desc: Cleanup the sound info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroySoundCaptureInfo(std::vector <SoundCaptureInfo*>& vSoundCaptureInfos)
{
    for (auto iter = vSoundCaptureInfos.begin(); iter != vSoundCaptureInfos.end(); iter++)
    {
        SoundCaptureInfo* pSoundCaptureInfo = *iter;
        SAFE_DELETE(pSoundCaptureInfo);
    }
    vSoundCaptureInfos.clear();
}



//-----------------------------------------------------------------------------
// Name: DestroySoundInfo()
// Desc: Cleanup the show info
//-----------------------------------------------------------------------------
VOID CDxDiagInfo::DestroyShowInfo(ShowInfo* pShowInfo)
{
    for (auto iter = pShowInfo->m_vShowFilters.begin(); iter != pShowInfo->m_vShowFilters.end(); iter++)
    {
        ShowFilterInfo* pShowFilterInfo = *iter;
        SAFE_DELETE(pShowFilterInfo);
    }
    pShowInfo->m_vShowFilters.clear();

    SAFE_DELETE(pShowInfo);
}


