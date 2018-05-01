//--------------------------------------------------------------------------------------
// File: GameuxInstallHelper.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#define _WIN32_DCOM

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#include <windows.h>
#include <shlobj.h>
#include <rpcsal.h>
#include <gameux.h>
#include <crtdbg.h>
// The Microsoft Platform SDK or Microsoft Windows SDK is required to compile this sample
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <assert.h>
#include <wbemidl.h>
#include <objbase.h>
#include <shellapi.h>
#include <WtsApi32.h>

#define NO_SHLWAPI_STRFCNS
#include <shlwapi.h>

#include "GameuxInstallHelper.h"
#include "GDFParse.h"

// Uncomment to get a debug messagebox
// #define SHOW_S1_DEBUG_MSGBOXES 
// #define SHOW_S2_DEBUG_MSGBOXES // more detail

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LPWSTR GetPropertyFromMSI(MSIHANDLE hMSI, LPCWSTR szPropName);
HRESULT GenerateGUID(GUID* pInstanceGUID);
HRESULT ConvertStringToGUID(const WCHAR* strSrc, GUID* pGuidDest);

HRESULT ConvertGUIDToStringCch(const GUID* pGuidSrc, 
                            WCHAR* strDest, 
                            int cchDestChar);

HRESULT CreateShortcut(WCHAR* strLaunchPath,
                            WCHAR* strCommandLineArgs, 
                            WCHAR* strShortcutFilePath);

HRESULT GetAccountName(WCHAR* strUser, 
                            DWORD cchUser, 
                            WCHAR* strDomain, 
                            DWORD cchDomain);

HRESULT RetrieveGUIDForApplication(WCHAR* szPathToGDFdll, GUID* pGUID);

HRESULT GameExplorerInstallToRegistry(WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath, 
                            GAME_INSTALL_SCOPE InstallScope);

HRESULT GameExplorerUninstallFromRegistry(WCHAR* strGDFBinPath);

HRESULT RemoveTasks(WCHAR* strGDFBinPath);
HRESULT CreateTasks(WCHAR* strGDFBinPath,
                            WCHAR* strGameInstallPath, 
                            GAME_INSTALL_SCOPE InstallScope);
HRESULT IsV2GDF (
    __in WCHAR* strGDFBinPath, 
    __out BOOL* pfV2GDF
    );

HRESULT GameExplorerInstallUsingIGameExplorer(IGameExplorer* pFwGameExplorer, 
                            WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath, 
                            GAME_INSTALL_SCOPE InstallScope);

HRESULT GameExplorerUninstallUsingIGameExplorer(IGameExplorer* pFwGameExplorer, WCHAR* strGDFBinPath);

HRESULT GameExplorerInstallUsingIGameExplorer2(IGameExplorer2* pFwGameExplorer2, 
                            WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath, 
                            GAME_INSTALL_SCOPE InstallScope);

HRESULT GameExplorerUninstallUsingIGameExplorer2(IGameExplorer2* pFwGameExplorer2, WCHAR* strGDFBinPath);

STDAPI GameExplorerUninstallW(WCHAR* strGDFBinPath);
STDAPI GameExplorerInstallW(WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath,
                            GAME_INSTALL_SCOPE InstallScope);

HRESULT GameExplorerUpdateUsingIGameExplorer(IGameExplorer* pFwGameExplorer, WCHAR* strGDFBinPath);

STDAPI GameExplorerUpdateW( WCHAR* strGDFBinPath );
STDAPI GameExplorerUpdateA( CHAR* strGDFBinPath );

//--------------------------------------------------------------------------------------
// This stores the install location
// sets up the CustomActionData properties for the deferred custom actions
//--------------------------------------------------------------------------------------
UINT __stdcall GameExplorerSetMSIProperties(MSIHANDLE hModule)
{
    WCHAR* szInstallDir = NULL;
    WCHAR* szProductCode = GetPropertyFromMSI(hModule, L"ProductCode");
    if (szProductCode != NULL)
    {
        DWORD dwBufferSize = 1024;
        szInstallDir = new WCHAR[dwBufferSize];
        if (szInstallDir != NULL)
        {
            if (ERROR_SUCCESS != MsiGetProductInfo(szProductCode, INSTALLPROPERTY_INSTALLLOCATION, szInstallDir, &dwBufferSize))
            {
                SAFE_DELETE_ARRAY(szInstallDir);
                szInstallDir = GetPropertyFromMSI(hModule, L"TARGETDIR");
            }
        }
        SAFE_DELETE_ARRAY(szProductCode);
    }
    else
    {
        szInstallDir = GetPropertyFromMSI(hModule, L"TARGETDIR");
    }
    
    // See if the install location property is set.  If it is, use that.  
    // Otherwise, get the property from TARGETDIR
    if (szInstallDir != NULL)
    {
        // Set the ARPINSTALLLOCATION property to the install dir so that 
        // the uninstall custom action can have it when getting the INSTALLPROPERTY_INSTALLLOCATION
        MsiSetPropertyW(hModule, L"ARPINSTALLLOCATION", szInstallDir);

        WCHAR szCustomActionData[1024] = {0};
        WCHAR* szRelativePathToGDF = GetPropertyFromMSI(hModule, L"RelativePathToGDF");
        if (szRelativePathToGDF != NULL )
        {
            wcscpy_s(szCustomActionData, 1024, szInstallDir);
            wcscat_s(szCustomActionData, 1024, szRelativePathToGDF);
            wcscat_s(szCustomActionData, 1024, L"|");
            wcscat_s(szCustomActionData, 1024, szInstallDir);
            wcscat_s(szCustomActionData, 1024, L"|");
            
            WCHAR* szALLUSERS = GetPropertyFromMSI(hModule, L"ALLUSERS");
            if (szALLUSERS && (szALLUSERS[0] == '1' || szALLUSERS[0] == '2'))
            {
                wcscat_s(szCustomActionData, 1024, L"3");
            }
            else
            {
                wcscat_s(szCustomActionData, 1024, L"2");
            }

            // Set the CustomActionData property for the deferred custom actions
            MsiSetProperty(hModule, L"GameUXAddAsAdmin", szCustomActionData);
            MsiSetProperty(hModule, L"GameUXAddAsCurUser", szCustomActionData);
            MsiSetProperty(hModule, L"GameUXRollBackRemoveAsAdmin", szCustomActionData);
            MsiSetProperty(hModule, L"GameUXRollBackRemoveAsCurUser", szCustomActionData);

            WCHAR szFullPathToGDF[MAX_PATH] = {0};
            wcscpy_s(szFullPathToGDF, MAX_PATH, szInstallDir);
            wcscat_s(szFullPathToGDF, MAX_PATH, szRelativePathToGDF);

            // Set the CustomActionData property for the deferred custom actions
            MsiSetProperty(hModule, L"GameUXRemoveAsAdmin", szFullPathToGDF);
            MsiSetProperty(hModule, L"GameUXRemoveAsCurUser", szFullPathToGDF);
            MsiSetProperty(hModule, L"GameUXRollBackAddAsAdmin", szFullPathToGDF);
            MsiSetProperty(hModule, L"GameUXRollBackAddAsCurUser", szFullPathToGDF);
            
            if (szALLUSERS != NULL)
            {
                SAFE_DELETE_ARRAY(szALLUSERS);
            }
            SAFE_DELETE_ARRAY(szRelativePathToGDF);
        }
        SAFE_DELETE_ARRAY(szInstallDir);
    }
    
    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// The CustomActionData property must be formated like so:
//      "<path to GDF binary>|<game install path>|<install scope>"
// for example:
//      "C:\MyGame\GameGDF.dll|C:\MyGame|2|"
//--------------------------------------------------------------------------------------
UINT __stdcall GameExplorerInstallUsingMSI(MSIHANDLE hModule)
{
    WCHAR* szCustomActionData = GetPropertyFromMSI(hModule, L"CustomActionData");
    if (szCustomActionData != NULL)
    {
        WCHAR szGDFBinPath[MAX_PATH] = {0};
        WCHAR szGameInstallPath[MAX_PATH] = {0};
        GAME_INSTALL_SCOPE InstallScope = GIS_ALL_USERS;

        WCHAR* pFirstDelim = wcschr(szCustomActionData, '|');
        if (pFirstDelim)
        {
            *pFirstDelim = 0;
            WCHAR* pSecondDelim = wcschr(pFirstDelim + 1, '|');
            if (pSecondDelim)
            {
                *pSecondDelim = 0;
                InstallScope = (GAME_INSTALL_SCOPE)_wtoi(pSecondDelim + 1);
            }
            wcscpy_s(szGameInstallPath, MAX_PATH, pFirstDelim + 1);
        }
        wcscpy_s(szGDFBinPath, MAX_PATH, szCustomActionData);


#ifdef SHOW_S1_DEBUG_MSGBOXES
        HRESULT hr = GameExplorerInstallW(szGDFBinPath, szGameInstallPath, InstallScope);
        WCHAR sz[1024];
        WCHAR szGUID[64] = {0};
        swprintf_s(sz, 1024, L"szGDFBinPath='%s'\nszGameInstallPath='%s'\nInstallScope='%s'\nszGUID='%s'\nhr=0x%0.8x\n",
            szGDFBinPath, szGameInstallPath, (InstallScope == GIS_ALL_USERS) ? L"GIS_ALL_USERS" : L"GIS_CURRENT_USER", szGUID, hr);
        MessageBox(NULL, sz, L"GameExplorerInstallUsingMSI", MB_OK);
#else
        GameExplorerInstallW(szGDFBinPath, szGameInstallPath, InstallScope);
#endif

        SAFE_DELETE_ARRAY(szCustomActionData);
    }
    else
    {
#ifdef SHOW_S1_DEBUG_MSGBOXES
        WCHAR sz[1024];
        swprintf_s(sz, 1024, L"CustomActionData property not found\n");
        MessageBox(NULL, sz, L"GameExplorerInstallUsingMSI", MB_OK);
#endif
    }

    // Ignore success/failure and continue on with install
    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// The CustomActionData property must be formated like so:
//      "<path to GDF binary>"
// for example:
//      "C:\MyGame\GameGDF.dll"
//--------------------------------------------------------------------------------------
UINT __stdcall GameExplorerUninstallUsingMSI(MSIHANDLE hModule)
{
    WCHAR* szCustomActionData = GetPropertyFromMSI(hModule, L"CustomActionData");

    if (szCustomActionData)
    {
        WCHAR szGDFBinPath[MAX_PATH] = {0};
        wcscpy_s(szGDFBinPath, MAX_PATH, szCustomActionData);
        SAFE_DELETE_ARRAY(szCustomActionData);

#ifdef SHOW_S2_DEBUG_MSGBOXES
        HRESULT hr = GameExplorerUninstallW(szGDFBinPath);
        WCHAR sz[1024];
        swprintf_s(sz, 1024, L"szGDFBinPath='%s'\nhr=0x%0.8x", szGDFBinPath, hr);
        MessageBox(NULL, sz, L"GameExplorerUninstallUsingMSI", MB_OK);
#else
        GameExplorerUninstallW(szGDFBinPath);
#endif
    }

    // Ignore success/failure and continue on with uninstall
    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Install a game to the Game Explorer (UNICODE)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerInstallW(WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath,
                            GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);

    if (strGDFBinPath == NULL || strGameInstallPath == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        BOOL fIsV2GDF = FALSE;
        hr = IsV2GDF(strGDFBinPath, &fIsV2GDF);
        if (SUCCEEDED(hr))
        {
            if (fIsV2GDF)
            {
                IGameExplorer2* pFwGameExplorer2 = NULL;
                // Create an instance of the IGameExplorer2 Interface
                hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer2), (void**)&pFwGameExplorer2);
                if (SUCCEEDED(hr) && (pFwGameExplorer2 != NULL))
                {
                    // Windows 7
                    hr = GameExplorerInstallUsingIGameExplorer2(pFwGameExplorer2, strGDFBinPath, strGameInstallPath, InstallScope);
                    pFwGameExplorer2->Release();
                }
                else
                {
                    IGameExplorer* pFwGameExplorer = NULL;
                    // Create an instance of the IGameExplorer Interface
                    hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**)&pFwGameExplorer);
                    if (SUCCEEDED(hr) && (pFwGameExplorer != NULL))
                    {
                        // Windows Vista
                        hr = GameExplorerInstallUsingIGameExplorer(pFwGameExplorer, strGDFBinPath, strGameInstallPath, InstallScope);
                        pFwGameExplorer->Release();
                    }
                    else
                    {
                        // Windows XP
                        hr = GameExplorerInstallToRegistry(strGDFBinPath, strGameInstallPath, InstallScope);
                    }
                }
            }
            else
            {
                IGameExplorer* pFwGameExplorer = NULL;
                // Create an instance of the IGameExplorer Interface
                hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**)&pFwGameExplorer);
                if (SUCCEEDED(hr) && (pFwGameExplorer != NULL))
                {
                    // Windows Vista or Windows 7
                    hr = GameExplorerInstallUsingIGameExplorer(pFwGameExplorer, strGDFBinPath, strGameInstallPath, InstallScope);
                    pFwGameExplorer->Release();
                }
                else
                {
                    // Windows XP
                    hr = GameExplorerInstallToRegistry(strGDFBinPath, strGameInstallPath, InstallScope);
                }
            }
        }
        CoUninitialize();
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Install game to the Game Explorer (ASCII)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerInstallA(CHAR* strGDFBinPath, 
                            CHAR* strGameInstallPath,
                            GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);

    if (strGDFBinPath == NULL || strGameInstallPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    WCHAR wstrBinPath[MAX_PATH] = {0};
    WCHAR wstrInstallPath[MAX_PATH] = {0};

    MultiByteToWideChar(CP_ACP, 0, strGDFBinPath, MAX_PATH, wstrBinPath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, strGameInstallPath, MAX_PATH, wstrInstallPath, MAX_PATH);
    return GameExplorerInstallW(wstrBinPath, wstrInstallPath, InstallScope);
}


//--------------------------------------------------------------------------------------
// Uninstall a game to the Game Explorer (UNICODE)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUninstallW(WCHAR* strGDFBinPath)
{
    assert(strGDFBinPath);

    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {

        BOOL fIsV2GDF = FALSE;
        hr = IsV2GDF(strGDFBinPath, &fIsV2GDF);
        if (SUCCEEDED(hr))
        {
            if (fIsV2GDF)
            {
                IGameExplorer2* pFwGameExplorer2 = NULL;
                // Create an instance of the IGameExplorer2 Interface
                hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer2), (void**)&pFwGameExplorer2);
                if (SUCCEEDED(hr) && (pFwGameExplorer2 != NULL))
                {
                    // Windows 7
                    hr = GameExplorerUninstallUsingIGameExplorer2(pFwGameExplorer2, strGDFBinPath);
                    pFwGameExplorer2->Release();
                }
                else
                {
                    IGameExplorer* pFwGameExplorer = NULL;
                    // Create an instance of the IGameExplorer Interface
                    hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**)&pFwGameExplorer);
                    if (SUCCEEDED(hr) && (pFwGameExplorer != NULL))
                    {
                        // Windows Vista
                        hr = GameExplorerUninstallUsingIGameExplorer(pFwGameExplorer, strGDFBinPath);
                        pFwGameExplorer->Release();
                    }
                    else
                    {
                        // Windows XP
                        hr = GameExplorerUninstallFromRegistry(strGDFBinPath);
                    }
                }
            }
            else
            {
                IGameExplorer* pFwGameExplorer = NULL;
                // Create an instance of the IGameExplorer Interface
                hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**)&pFwGameExplorer);
                if (SUCCEEDED(hr) && (pFwGameExplorer != NULL))
                {
                    // Windows Vista
                    hr = GameExplorerUninstallUsingIGameExplorer(pFwGameExplorer, strGDFBinPath);
                    pFwGameExplorer->Release();
                }
                else
                {
                    // Windows XP
                    hr = GameExplorerUninstallFromRegistry(strGDFBinPath);
                }
            }
        }
        CoUninitialize();
    }
    return hr;
}

//--------------------------------------------------------------------------------------
// Uninstall a game to the Game Explorer (ASCII)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUninstallA(CHAR* strGDFBinPath)
{
    assert(strGDFBinPath);

    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }

    WCHAR wstrBinPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, strGDFBinPath, MAX_PATH, wstrBinPath, MAX_PATH);
    return GameExplorerUninstallW(wstrBinPath);
}

//--------------------------------------------------------------------------------------
// Update a game with the Game Explorer (UNICODE)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUpdateW(WCHAR* strGDFBinPath)
{
    assert(strGDFBinPath);

    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IGameExplorer* pFwGameExplorer = NULL;
        // Create an instance of the IGameExplorer Interface
        hr = CoCreateInstance(__uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, __uuidof(IGameExplorer), (void**)&pFwGameExplorer);
        if (SUCCEEDED(hr) && (pFwGameExplorer != NULL))
        {
            hr = GameExplorerUpdateUsingIGameExplorer(pFwGameExplorer, strGDFBinPath);
            pFwGameExplorer->Release();
        }

        CoUninitialize();
    }
    return hr;
}

//--------------------------------------------------------------------------------------
// Update a game with the Game Explorer (ASCII)
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUpdateA(CHAR* strGDFBinPath)
{
    assert(strGDFBinPath);

    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }

    WCHAR wstrBinPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, strGDFBinPath, MAX_PATH, wstrBinPath, MAX_PATH);
    return GameExplorerUpdateW(wstrBinPath);
}

//--------------------------------------------------------------------------------------
// Adds a game to the Game Explorer for XP
//--------------------------------------------------------------------------------------
HRESULT GameExplorerInstallToRegistry(WCHAR* strGDFBinPath, 
                                 WCHAR* strGameInstallPath,
                                 GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);

    if (strGDFBinPath == NULL || strGameInstallPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr = E_FAIL;
    BSTR bstrGDFBinPath = NULL;
    BSTR bstrGameInstallPath = NULL;
    
    bstrGDFBinPath = SysAllocString(strGDFBinPath);
    bstrGameInstallPath = SysAllocString(strGameInstallPath);
    if (bstrGDFBinPath != NULL && bstrGameInstallPath != NULL)
    {
        // On Windows XP or eariler, write registry keys to known location 
        // so that if the machine is upgraded to Windows Vista or later, these games will 
        // be automatically found.
        // 
        // Depending on GAME_INSTALL_SCOPE, write to:
        //      HKLM\Software\Microsoft\Windows\CurrentVersion\GameUX\GamesToFindOnWindowsUpgrade\{GUID}\
        // or
        //      HKCU\Software\Classes\Software\Microsoft\Windows\CurrentVersion\GameUX\GamesToFindOnWindowsUpgrade\{GUID}\
        // and write there these 2 string values: GDFBinaryPath and GameInstallPath 
        //
        HKEY hKeyGamesToFind = NULL, hKeyGame = NULL;
        LONG lResult;
        DWORD dwDisposition;
        if (InstallScope == GIS_CURRENT_USER)
        {
            lResult = RegCreateKeyEx(HKEY_CURRENT_USER,
                                      L"Software\\Classes\\Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade",
                                      0, NULL, 0, KEY_WRITE, NULL, &hKeyGamesToFind, &dwDisposition);
        }
        else
        {
            lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                      L"Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade",
                                      0, NULL, 0, KEY_WRITE, NULL, &hKeyGamesToFind, &dwDisposition);
        }
        
#ifdef SHOW_S1_DEBUG_MSGBOXES
        WCHAR sz[1024];
        swprintf_s(sz, 1024, L"RegCreateKeyEx lResult=%d", lResult);
        MessageBox(NULL, sz, L"GameExplorerInstallForXP", MB_OK);
#endif
        hr = HRESULT_FROM_WIN32(lResult);
        if (SUCCEEDED(hr))
        {
            GUID InstanceGUID = GUID_NULL;
            RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
            if (InstanceGUID == GUID_NULL)
            {
                GenerateGUID(&InstanceGUID);
            }
            
            WCHAR strGameInstanceGUID[128] = {0};
            if (StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID)) != 0)
            {
                lResult = RegCreateKeyEx(hKeyGamesToFind, strGameInstanceGUID, 0, NULL, 0, KEY_WRITE,
                                          NULL, &hKeyGame, &dwDisposition);
                hr = HRESULT_FROM_WIN32(lResult);
                if (SUCCEEDED(hr))
                {
                    size_t nGDFBinPath = 0, nGameInstallPath = 0;
                    nGDFBinPath = wcsnlen(strGDFBinPath, MAX_PATH);
                    nGameInstallPath = wcsnlen(strGameInstallPath, MAX_PATH);
                    RegSetValueEx(hKeyGame, L"GDFBinaryPath", 0, REG_SZ, (BYTE*)strGDFBinPath, (DWORD)
                                   ((nGDFBinPath + 1) * sizeof(WCHAR)));
                    RegSetValueEx(hKeyGame, L"GameInstallPath", 0, REG_SZ, (BYTE*)strGameInstallPath, (DWORD)
                                   ((nGameInstallPath + 1) * sizeof(WCHAR)));
                    RegCloseKey(hKeyGame);
                }
            }
            RegCloseKey(hKeyGamesToFind);
        }
        SysFreeString(bstrGDFBinPath);
        SysFreeString(bstrGameInstallPath);
    }
    
    // create short cut, play tasks, and support tasks
    if (SUCCEEDED(hr))
    {
        hr = CreateTasks(strGDFBinPath, strGameInstallPath, InstallScope);
    }
    return hr;
}

//--------------------------------------------------------------------------------------
// Adds a game to the Game Explorer for Windows Vista
//--------------------------------------------------------------------------------------
HRESULT GameExplorerInstallUsingIGameExplorer(IGameExplorer* pFwGameExplorer, 
                                    WCHAR* strGDFBinPath, 
                                    WCHAR* strGameInstallPath,
                                    GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);
    assert(pFwGameExplorer);
    
    if (strGDFBinPath == NULL || strGameInstallPath == NULL || pFwGameExplorer == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = E_FAIL;
    BSTR bstrGDFBinPath = NULL;
    BSTR bstrGameInstallPath = NULL;
    
    bstrGDFBinPath = SysAllocString(strGDFBinPath);
    bstrGameInstallPath = SysAllocString(strGameInstallPath);
    if ((bstrGDFBinPath != NULL) && (bstrGameInstallPath != NULL))
    {
        GUID InstanceGUID = GUID_NULL;
        BOOL bHasAccess = FALSE;
        hr = pFwGameExplorer->VerifyAccess(bstrGDFBinPath, &bHasAccess);
        if (SUCCEEDED(hr) && bHasAccess)
        {
            hr = pFwGameExplorer->AddGame(bstrGDFBinPath, bstrGameInstallPath, InstallScope, &InstanceGUID);
        }
        
#ifdef SHOW_S1_DEBUG_MSGBOXES
        WCHAR strUser[256] = {0};
        WCHAR strDomain[256] = {0};
        GetAccountName(strUser, 256, strDomain, 256);
        BOOL bAdmin = IsUserAnAdmin();

        WCHAR sz[1024] = {0};
        WCHAR strGameInstanceGUID[128] = {0};
        StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
        swprintf_s(sz, 1024, L"szGDFBinPath='%s'\nszGameInstallPath='%s'\nInstallScope='%s'\nszGUID='%s'\nAccount=%s\\%s\nAdmin=%d\nhr=0x%0.8x",
                         bstrGDFBinPath, bstrGameInstallPath, (InstallScope == GIS_ALL_USERS) ? L"GIS_ALL_USERS" : L"GIS_CURRENT_USER", strGameInstanceGUID, strDomain, strUser, bAdmin, hr);
        MessageBox(NULL, sz, L"GameExplorerInstallUsingIGameExplorer", MB_OK);
#endif

        SysFreeString(bstrGDFBinPath);
        SysFreeString(bstrGameInstallPath);
    }

    // create short cut, play tasks, and support tasks
    if (SUCCEEDED(hr))
    {
        hr = CreateTasks(strGDFBinPath, strGameInstallPath, InstallScope);
    }
    
    return hr;
}

//--------------------------------------------------------------------------------------
// Adds a game to the Game Explorer
//--------------------------------------------------------------------------------------
HRESULT GameExplorerInstallUsingIGameExplorer2(IGameExplorer2* pFwGameExplorer2, 
                                   WCHAR* strGDFBinPath, 
                                   WCHAR* strGameInstallPath, 
                                   GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);
    assert(pFwGameExplorer2);

    if (strGDFBinPath == NULL || strGameInstallPath == NULL || pFwGameExplorer2 == NULL)
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr = E_FAIL;
    BSTR bstrGDFBinPath = NULL;
    BSTR bstrGameInstallPath = NULL;
    
    bstrGDFBinPath = SysAllocString(strGDFBinPath);
    bstrGameInstallPath = SysAllocString(strGameInstallPath);
    if (bstrGDFBinPath != NULL && bstrGameInstallPath != NULL)
    {
       hr = pFwGameExplorer2->InstallGame(bstrGDFBinPath, bstrGameInstallPath, InstallScope);

#ifdef SHOW_S1_DEBUG_MSGBOXES
       WCHAR strUser[256] = {0};
       WCHAR strDomain[256] = {0};
       GetAccountName(strUser, 256, strDomain, 256);
       BOOL bAdmin = IsUserAnAdmin();

       WCHAR sz[1024] = {0};
       WCHAR strGameInstanceGUID[128] = {0};
       GUID InstanceGUID = GUID_NULL;
       
       RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
       StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
       swprintf_s(sz, 1024, L"szGDFBinPath='%s'\nszGameInstallPath='%s'\nInstallScope='%s'\nszGUID='%s'\nAccount=%s\\%s\nAdmin=%d\nhr=0x%0.8x",
                        bstrGDFBinPath, bstrGameInstallPath, (InstallScope == GIS_ALL_USERS) ? L"GIS_ALL_USERS" : L"GIS_CURRENT_USER", strGameInstanceGUID, strDomain, strUser, bAdmin, hr);
       MessageBox(NULL, sz, L"GameExplorerInstallUsingIGameExplorer2", MB_OK);
#endif

       SysFreeString(bstrGDFBinPath);
       SysFreeString(bstrGameInstallPath);
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Removes a game from the Game Explorer - On Windows XP remove reg keys
//--------------------------------------------------------------------------------------
HRESULT GameExplorerUninstallFromRegistry(WCHAR* strGDFBinPath)
{
    assert(strGDFBinPath);

    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    GUID InstanceGUID = GUID_NULL;
    HRESULT hr = RemoveTasks(strGDFBinPath);
    if (SUCCEEDED(hr))
    {
         hr = RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
         if (SUCCEEDED(hr))
         {
            WCHAR strGameInstanceGUID[128] = {0};
            if (StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID)) != 0)
            {
                WCHAR szKeyPath[1024];
                if (SUCCEEDED(swprintf_s(szKeyPath, 1024,
                                                L"Software\\Classes\\Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade\\%s", strGameInstanceGUID)))
                {
                    SHDeleteKey(HKEY_CURRENT_USER, szKeyPath);
                }

                if (SUCCEEDED(swprintf_s(szKeyPath, 1024,
                                                L"Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade\\%s", strGameInstanceGUID)))
                {
                    SHDeleteKey(HKEY_LOCAL_MACHINE, szKeyPath);
                }
            }
        }
    }

#ifdef SHOW_S1_DEBUG_MSGBOXES
    WCHAR sz[1024];
    WCHAR strGameInstanceGUID[128] = {0};
                
    StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
    swprintf_s(sz, 1024, L"bVistaPath=%d\npInstanceGUID='%s'\nhr=0x%0.8x", false, strGameInstanceGUID, hr);
    MessageBox(NULL, sz, L"GameExplorerUninstallForXP", MB_OK);
#endif

    return hr;
}


//--------------------------------------------------------------------------------------
// Removes a game from the Game Explorer for Windows Vista
//--------------------------------------------------------------------------------------
HRESULT GameExplorerUninstallUsingIGameExplorer(IGameExplorer* pFwGameExplorer, WCHAR* strGDFBinPath)
{
    assert(pFwGameExplorer);
    assert(strGDFBinPath);
    
    if (strGDFBinPath == NULL || pFwGameExplorer == NULL)
    {
        return E_INVALIDARG;
    }

    GUID InstanceGUID = GUID_NULL;
    HRESULT hr = RemoveTasks(strGDFBinPath);
    if (SUCCEEDED(hr))
    {
        hr = RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
        if (SUCCEEDED(hr))
        {
            // Remove the game from the Game Explorer
            hr = pFwGameExplorer->RemoveGame(InstanceGUID);
        }
    }

#ifdef SHOW_S1_DEBUG_MSGBOXES
    WCHAR sz[1024];
    WCHAR strGameInstanceGUID[128] = {0};
    StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
    swprintf_s(sz, 1024, L"\npInstanceGUID='%s'\nhr=0x%0.8x", strGameInstanceGUID, hr);
    MessageBox(NULL, sz, L"GameExplorerUninstallUsingIGameExplorer", MB_OK);
#endif

    return hr;
}


//--------------------------------------------------------------------------------------
// Removes a game from the Game Explorer for Windows 7
//--------------------------------------------------------------------------------------
HRESULT GameExplorerUninstallUsingIGameExplorer2(IGameExplorer2* pFwGameExplorer2, WCHAR* strGDFBinPath)
{
    assert(pFwGameExplorer2);
    assert(strGDFBinPath);
    
    if (strGDFBinPath == NULL || pFwGameExplorer2 == NULL)
    {
        return E_INVALIDARG;
    }

#ifdef SHOW_S1_DEBUG_MSGBOXES
    WCHAR strGameInstanceGUID[128] = {0};
    GUID InstanceGUID = GUID_NULL;
    RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
    StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
#endif

    // Remove the game from the Game Explorer
    HRESULT hr = pFwGameExplorer2->UninstallGame(strGDFBinPath);

#ifdef SHOW_S1_DEBUG_MSGBOXES
    WCHAR sz[1024];
    swprintf_s(sz, 1024, L"\npInstanceGUID='%s'\nhr=0x%0.8x", strGameInstanceGUID, hr);
    MessageBox(NULL, sz, L"GameExplorerUninstallUsingIGameExplorer2", MB_OK);
#endif

    return hr;
}


//--------------------------------------------------------------------------------------
// Updates a game with Game Explorer
//--------------------------------------------------------------------------------------
HRESULT GameExplorerUpdateUsingIGameExplorer(IGameExplorer* pFwGameExplorer, WCHAR* strGDFBinPath)
{
    assert(pFwGameExplorer);
    assert(strGDFBinPath);
    
    if (strGDFBinPath == NULL || pFwGameExplorer == NULL)
    {
        return E_INVALIDARG;
    }

    GUID InstanceGUID = GUID_NULL;
    HRESULT hr = RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID);
    if (SUCCEEDED(hr))
    {
        // Update game using Game Explorer
        hr = pFwGameExplorer->UpdateGame(InstanceGUID);
    }

#ifdef SHOW_S1_DEBUG_MSGBOXES
    WCHAR sz[1024];
    WCHAR strGameInstanceGUID[128] = {0};
    StringFromGUID2(InstanceGUID, strGameInstanceGUID, ARRAYSIZE(strGameInstanceGUID));
    swprintf_s(sz, 1024, L"\npInstanceGUID='%s'\nhr=0x%0.8x", strGameInstanceGUID, hr);
    MessageBox(NULL, sz, L"GameExplorerUpdateUsingIGameExplorer", MB_OK);
#endif

    return hr;
}


//--------------------------------------------------------------------------------------
// Generates a random GUID
//--------------------------------------------------------------------------------------
HRESULT GenerateGUID(GUID* pInstanceGUID)
{
    assert(pInstanceGUID);
    
    if (pInstanceGUID == NULL)
    {
        return E_INVALIDARG;
    }
    
    return CoCreateGuid(pInstanceGUID);
}

//--------------------------------------------------------------------------------------
// Gets a property from MSI.  Deferred custom action can only access the property called
// "CustomActionData"
//--------------------------------------------------------------------------------------
LPWSTR GetPropertyFromMSI(MSIHANDLE hMSI, LPCWSTR szPropName)
{
    DWORD dwSize = 0, dwBufferLen = 0;
    LPWSTR szValue = NULL;

    WCHAR empty[] = L"";
    UINT uErr = MsiGetProperty(hMSI, szPropName, empty, &dwSize);
    if ((ERROR_SUCCESS == uErr) || (ERROR_MORE_DATA == uErr))
    {
        dwSize++; // Add NULL term
        dwBufferLen = dwSize;
        szValue = new WCHAR[ dwBufferLen ];
        if (szValue)
        {
            uErr = MsiGetProperty(hMSI, szPropName, szValue, &dwSize);
            if ((ERROR_SUCCESS != uErr))
            {
                // Cleanup on failure
                SAFE_DELETE_ARRAY(szValue);
            }
            else
            {
                // Make sure buffer is null-terminated
                szValue[ dwBufferLen - 1 ] = '\0';
            }
        }
    }

    return szValue;
}

//-----------------------------------------------------------------------------
// Converts a string to a GUID
//-----------------------------------------------------------------------------
HRESULT ConvertStringToGUID(const WCHAR* strSrc, GUID* pGuidDest)
{
    assert(strSrc);
    assert(pGuidDest);
    
    if (strSrc == NULL || pGuidDest == NULL)
    {
        return E_INVALIDARG;
    }
    
    UINT aiTmp[10];

    if (swscanf_s(strSrc, L"{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}",
                 &pGuidDest->Data1,
                 &aiTmp[0], &aiTmp[1],
                 &aiTmp[2], &aiTmp[3],
                 &aiTmp[4], &aiTmp[5],
                 &aiTmp[6], &aiTmp[7],
                 &aiTmp[8], &aiTmp[9]) != 11)
    {
        ZeroMemory(pGuidDest, sizeof(GUID));
        return E_FAIL;
    }
    else
    {
        pGuidDest->Data2 = (USHORT)aiTmp[0];
        pGuidDest->Data3 = (USHORT)aiTmp[1];
        pGuidDest->Data4[0] = (BYTE)aiTmp[2];
        pGuidDest->Data4[1] = (BYTE)aiTmp[3];
        pGuidDest->Data4[2] = (BYTE)aiTmp[4];
        pGuidDest->Data4[3] = (BYTE)aiTmp[5];
        pGuidDest->Data4[4] = (BYTE)aiTmp[6];
        pGuidDest->Data4[5] = (BYTE)aiTmp[7];
        pGuidDest->Data4[6] = (BYTE)aiTmp[8];
        pGuidDest->Data4[7] = (BYTE)aiTmp[9];
        return S_OK;
    }
}


//-----------------------------------------------------------------------------
HRESULT ConvertGUIDToStringCch(const GUID* pGuidSrc, 
                               WCHAR* strDest, 
                               int cchDestChar)
{
    assert(pGuidSrc);
    assert(strDest);
    
    if (pGuidSrc == NULL || strDest == NULL)
    {
        return E_INVALIDARG;
    }
    
    return swprintf_s(strDest, cchDestChar, L"{%0.8X-%0.4X-%0.4X-%0.2X%0.2X-%0.2X%0.2X%0.2X%0.2X%0.2X%0.2X}",
                           pGuidSrc->Data1, pGuidSrc->Data2, pGuidSrc->Data3,
                           pGuidSrc->Data4[0], pGuidSrc->Data4[1],
                           pGuidSrc->Data4[2], pGuidSrc->Data4[3],
                           pGuidSrc->Data4[4], pGuidSrc->Data4[5],
                           pGuidSrc->Data4[6], pGuidSrc->Data4[7]);
}


//-----------------------------------------------------------------------------
// Enums WinXP registry for GDF upgrade keys, and returns the GUID
// based on the GDF binary path
//-----------------------------------------------------------------------------
bool RetrieveGUIDForApplicationOnWinXP(HKEY hKeyRoot, 
                                       WCHAR* szPathToGDFdll, 
                                       GUID* pGUID)
{
    assert(szPathToGDFdll);
    assert(pGUID);
    
    DWORD iKey = 0;
    WCHAR strRegKeyName[256];
    WCHAR strGDFBinPath[MAX_PATH];
    HKEY hKey = NULL;
    LONG lResult;
    DWORD dwDisposition, dwType, dwSize;
    bool bFound = false;

    for(; ;)
    {
        lResult = RegEnumKey(hKeyRoot, iKey, strRegKeyName, 256);
        if (lResult != ERROR_SUCCESS)
        {
            break;
        }
        lResult = RegCreateKeyEx(hKeyRoot, strRegKeyName, 0, NULL, 0, KEY_READ, NULL, &hKey, &dwDisposition);
        if (lResult == ERROR_SUCCESS)
        {
            dwSize = MAX_PATH * sizeof(WCHAR);
            lResult = RegQueryValueEx(hKey, L"GDFBinaryPath", 0, &dwType, (BYTE*)strGDFBinPath, &dwSize);
            if (lResult == ERROR_SUCCESS)
            {
                if (wcscmp(strGDFBinPath, szPathToGDFdll) == 0)
                {
                    bFound = true;
                    ConvertStringToGUID(strRegKeyName, pGUID);
                }
            }
            RegCloseKey(hKey);
        }

        if (bFound)
        {
            break;
        }
        iKey++;
    }

    return bFound;
}

//-----------------------------------------------------------------------------
HRESULT RetrieveGUIDForApplication(WCHAR* szPathToGDFdll, GUID* pGUID)
{
    assert(szPathToGDFdll);
    assert(pGUID);
    
    if (szPathToGDFdll == NULL || pGUID == NULL)
    {
        return E_INVALIDARG;
    }
    
    bool bFound = false;

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IWbemLocator* pIWbemLocator = NULL;
        hr = CoCreateInstance(__uuidof(WbemLocator), NULL, CLSCTX_INPROC_SERVER,
                               __uuidof(IWbemLocator), (LPVOID*)&pIWbemLocator);
        if (SUCCEEDED(hr) && pIWbemLocator)
        {
            // Using the locator, connect to WMI in the given namespace.
            BSTR pNamespace = SysAllocString(L"\\\\.\\root\\cimv2\\Applications\\Games");

            IWbemServices* pIWbemServices = NULL;
            hr = pIWbemLocator->ConnectServer(pNamespace, NULL, NULL, 0L,
                                               0L, NULL, NULL, &pIWbemServices);
            if (SUCCEEDED(hr) && (pIWbemServices != NULL))
            {
                // Switch security level to IMPERSONATE. 
                CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                                   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0);

                BSTR bstrQueryType = SysAllocString(L"WQL");

                // Double up the '\' marks for the WQL query
                WCHAR szDoubleSlash[2048];
                int iDest = 0, iSource = 0;
                for(; ;)
                {
                    if (szPathToGDFdll[iSource] == 0 || iDest > 2000)
                    {
                        break;
                    }
                    szDoubleSlash[iDest] = szPathToGDFdll[iSource];
                    if (szPathToGDFdll[iSource] == L'\\')
                    {
                        iDest++; szDoubleSlash[iDest] = L'\\';
                    }
                    iDest++;
                    iSource++;
                }
                szDoubleSlash[iDest] = 0;

                WCHAR szQuery[1024];
                swprintf_s(szQuery, 1024, L"SELECT * FROM GAME WHERE GDFBinaryPath = \"%s\"", szDoubleSlash);
                BSTR bstrQuery = SysAllocString(szQuery);

                IEnumWbemClassObject* pEnum = NULL;
                hr = pIWbemServices->ExecQuery(bstrQueryType, bstrQuery,
                                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                                NULL, &pEnum);
                if (SUCCEEDED(hr))
                {
                    IWbemClassObject* pGameClass = NULL;
                    DWORD uReturned = 0;
                    BSTR pPropName = NULL;

                    // Get the first one in the list
                    hr = pEnum->Next(5000, 1, &pGameClass, &uReturned);
                    if (SUCCEEDED(hr) && (uReturned != 0) && (pGameClass != NULL))
                    {
                        VARIANT var;

                        // Get the InstanceID string
                        pPropName = SysAllocString(L"InstanceID");
                        hr = pGameClass->Get(pPropName, 0L, &var, NULL, NULL);
                        if (SUCCEEDED(hr) && (var.vt == VT_BSTR))
                        {
                            bFound = true;
                            if (pGUID) 
                            {
                                ConvertStringToGUID(var.bstrVal, pGUID);
                            }
                        }

                        if (pPropName) 
                        {
                            SysFreeString(pPropName);
                        }
                    }

                    SAFE_RELEASE(pGameClass);
                }

                SAFE_RELEASE(pEnum);
            }

            if (pNamespace) 
            {
                SysFreeString(pNamespace);
            }
            SAFE_RELEASE(pIWbemServices);
        }

        SAFE_RELEASE(pIWbemLocator);
        CoUninitialize();
    }

#ifdef SHOW_S2_DEBUG_MSGBOXES
    WCHAR sz[1024];
    swprintf_s(sz, 1024, L"szPathToGDFdll=%s \nbFound=%d", szPathToGDFdll, bFound);
    MessageBox(NULL, sz, L"RetrieveGUIDForApplication", MB_OK);
#endif

    if (!bFound)
    {
        // Look in WinXP regkey paths
        HKEY hKeyRoot;
        LONG lResult;
        DWORD dwDisposition;
        lResult = RegCreateKeyEx(HKEY_CURRENT_USER,
                                  L"Software\\Classes\\Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade",
                                  0, NULL, 0, KEY_READ, NULL, &hKeyRoot, &dwDisposition);
        if (ERROR_SUCCESS == lResult)
        {
            bFound = RetrieveGUIDForApplicationOnWinXP(hKeyRoot, szPathToGDFdll, pGUID);
            RegCloseKey(hKeyRoot);
        }

        if (!bFound)
        {
            lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                                      L"Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade",
                                      0, NULL, 0, KEY_READ, NULL, &hKeyRoot, &dwDisposition);
            if (ERROR_SUCCESS == lResult)
            {
                bFound = RetrieveGUIDForApplicationOnWinXP(hKeyRoot, szPathToGDFdll, pGUID);
                RegCloseKey(hKeyRoot);
            }
        }
    }

    return (bFound) ? S_OK : E_FAIL;
}

//-----------------------------------------------------------------------------
// Debug function to return account name of calling process
//-----------------------------------------------------------------------------
HRESULT GetAccountName(WCHAR* strUser, 
                       DWORD cchUser, 
                       WCHAR* strDomain, 
                       DWORD cchDomain)
{
    assert(strUser);
    assert(strDomain);
    
    if (strUser == NULL || strDomain == NULL )
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr = E_FAIL;
    WCHAR strMachine[256] = {0};
    DWORD cchMachine = 256;
    GetComputerName(strMachine, &cchMachine);

    WTS_PROCESS_INFOW* pProcessInfo = NULL;
    HANDLE hServer = WTSOpenServer(strMachine);
    DWORD dwCount = 0;
    DWORD dwCurrentProcessId = GetCurrentProcessId();
    if (WTSEnumerateProcesses(hServer, 0, 1, &pProcessInfo, &dwCount))
    {
        for(DWORD n = 0; n < dwCount; n++)
        {
            if (pProcessInfo[n].ProcessId == dwCurrentProcessId)
            {
                SID_NAME_USE eUse;
                BOOL bSuccess = LookupAccountSid(NULL, pProcessInfo[n].pUserSid, strUser, &cchUser,
                                                  strDomain, &cchDomain, &eUse);
                if (bSuccess)
                {
                    hr = S_OK;
                }
                break;
            }
        }
    }
    WTSFreeMemory(pProcessInfo);
    WTSCloseServer(hServer);
    return hr;
}

//--------------------------------------------------------------------------------------
HRESULT GetXMLAttribute(IXMLDOMNode* pNode, 
                        WCHAR* strAttribName, 
                        WCHAR* strValue, 
                        int cchValue)
{
    assert(pNode);
    assert(strAttribName);
    assert(strValue);
    
    if (pNode == NULL || strAttribName == NULL || strValue == NULL)
    {
        return E_INVALIDARG;
    }

    bool bFound = false;
    IXMLDOMNamedNodeMap* pIXMLDOMNamedNodeMap = NULL;
    BSTR bstrAttributeName = ::SysAllocString( strAttribName );
    IXMLDOMNode* pIXMLDOMNode = NULL;

    HRESULT hr;
    VARIANT v;
    hr = pNode->get_attributes( &pIXMLDOMNamedNodeMap );
    if( SUCCEEDED( hr ) && pIXMLDOMNamedNodeMap )
    {
        hr = pIXMLDOMNamedNodeMap->getNamedItem( bstrAttributeName, &pIXMLDOMNode );
        if( SUCCEEDED( hr ) && pIXMLDOMNode )
        {
            pIXMLDOMNode->get_nodeValue( &v );
            if( SUCCEEDED( hr ) && v.vt == VT_BSTR )
            {
                wcscpy_s( strValue, cchValue, v.bstrVal );
                bFound = true;
            }
            VariantClear( &v );
            SAFE_RELEASE( pIXMLDOMNode );
        }
        SAFE_RELEASE( pIXMLDOMNamedNodeMap );
    }

    ::SysFreeString( bstrAttributeName );
    bstrAttributeName = NULL;

    if( !bFound )
    {
        return S_FALSE;
    }
    else
    {
        return S_OK;
    }
}

//-----------------------------------------------------------------------------
HRESULT GetBaseKnownFolderCsidl(WCHAR* strBaseKnownFolder, int* pCsidl)
{
    assert(strBaseKnownFolder);
    assert(pCsidl);
    
    if (strBaseKnownFolder == NULL || pCsidl == NULL )
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr = S_OK;
    
    if (wcscmp(strBaseKnownFolder, L"{905e63b6-c1bf-494e-b29c-65b732d3d21a}") == 0)
    {
           *pCsidl =  CSIDL_PROGRAM_FILES;
    }
    else if (wcscmp(strBaseKnownFolder, L"{F7F1ED05-9F6D-47A2-AAAE-29D317C6F066}") == 0)
    {
           *pCsidl =  CSIDL_PROGRAM_FILES_COMMON;
    }
    else if (wcscmp(strBaseKnownFolder, L"{B4BFCC3A-DB2C-424C-B029-7FE99A87C641}") == 0)
    {
           *pCsidl =  CSIDL_DESKTOP;
    }
    else if (wcscmp(strBaseKnownFolder, L"{FDD39AD0-238F-46AF-ADB4-6C85480369C7}") == 0)
    {
           *pCsidl =  CSIDL_MYDOCUMENTS;
    }
    else if (wcscmp(strBaseKnownFolder, L"{C4AA340D-F20F-4863-AFEF-F87EF2E6BA25}") == 0)
    {
           *pCsidl =  CSIDL_COMMON_DESKTOPDIRECTORY;
    }
    else if (wcscmp(strBaseKnownFolder, L"{ED4824AF-DCE4-45A8-81E2-FC7965083634}") == 0)
    {
           *pCsidl =  CSIDL_COMMON_DOCUMENTS;
    }
    else if (wcscmp(strBaseKnownFolder, L"{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}") == 0)
    {
           *pCsidl =  CSIDL_SYSTEM;
    }
    else if (wcscmp(strBaseKnownFolder, L"{F38BF404-1D43-42F2-9305-67DE0B28FC23}") == 0)
    {
           *pCsidl =  CSIDL_WINDOWS;
    }
    else
    {
        hr = S_FALSE;
    }

    return hr;
}

//-----------------------------------------------------------------------------
HRESULT CreateShortcut(WCHAR* strLaunchPath, 
                       WCHAR* strCommandLineArgs,      // strCommandLineArgs can be NULL
                       WCHAR* strShortcutFilePath, 
                       BOOL bFileTask)
{
    assert(strLaunchPath);
    assert(strShortcutFilePath);
    
    if (strLaunchPath == NULL || strShortcutFilePath == NULL )
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        IShellLink* psl;
        hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                               IID_IShellLink, (LPVOID*)&psl);
        if (SUCCEEDED(hr))
        {
            // Setup shortcut
            psl->SetPath(strLaunchPath);
            if (strCommandLineArgs)
            {
                psl->SetArguments(strCommandLineArgs);
            }
            // These shortcut settings aren't needed for tasks
            // if (strIconPath) psl->SetIconLocation(strIconPath, nIcon);
            // if (wHotkey) psl->SetHotkey(wHotkey);
            // if (nShowCmd) psl->SetShowCmd(nShowCmd);
            // if (strDescription) psl->SetDescription(strDescription);

            if (bFileTask)
            {
                // Set working dir to path of launch exe
                WCHAR strFullPath[512];
                WCHAR* strExePart;
                GetFullPathName(strLaunchPath, 512, strFullPath, &strExePart);
                if (strExePart) 
                {
                    *strExePart = 0;
                }
                psl->SetWorkingDirectory(strFullPath);
            }

            // Save shortcut to file
            IPersistFile* ppf;
            hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
            if (SUCCEEDED(hr))
            {
                hr = ppf->Save(strShortcutFilePath, TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        
        CoUninitialize();
    }

    return hr;
}

//-----------------------------------------------------------------------------
HRESULT SubCreateSingleTask(GAME_INSTALL_SCOPE InstallScope,         // Either GIS_CURRENT_USER or GIS_ALL_USERS 
                   WCHAR* strGDFBinPath,                    // valid GameInstance GUID that was passed to AddGame()
                   WCHAR* strTaskName,                      // Name of task.  Ex "Play"
                   WCHAR* strLaunchPath,                    // Path to exe.  Example: "C:\Program Files\Microsoft\MyGame.exe"
                   WCHAR* strCommandLineArgs,               // Can be NULL.  Example: "-windowed"
                   int nTaskID,                             // ID of task
                   BOOL bSupportTask,                       // if TRUE, this is a support task otherwise it is a play task
                   BOOL bFileTask)                          // if TRUE, this is a file task otherwise it is a URL task
{
    assert(strGDFBinPath);
    assert(strTaskName);
    assert(strLaunchPath);
    
    if (strGDFBinPath == NULL || strTaskName == NULL || strLaunchPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr;
    WCHAR strPath[512];
    WCHAR strGUID[256];
    WCHAR strCommonFolder[MAX_PATH];
    WCHAR strShortcutFilePath[512];

    // Get base path based on install scope
    if (InstallScope == GIS_CURRENT_USER)
    {
        SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strCommonFolder);
    }
    else
    {
        SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, strCommonFolder);
    }

    GUID InstanceGUID = GUID_NULL;
    if (FAILED(hr = RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID)))
    {
        return hr;
    }

    // Convert GUID to string
    if (StringFromGUID2(InstanceGUID, strGUID, ARRAYSIZE(strGUID)) == 0)
    {
        return E_FAIL;
    }
    // Create dir path for shortcut
    swprintf_s(strPath, 512, L"%s\\Microsoft\\Windows\\GameExplorer\\%s\\%s\\%d",
                     strCommonFolder, strGUID, (bSupportTask) ? L"SupportTasks" : L"PlayTasks", nTaskID);

    // Create the directory and all intermediate directories
    SHCreateDirectoryEx(NULL, strPath, NULL);

    // Create full file path to shortcut 
    swprintf_s(strShortcutFilePath, 512, L"%s\\%s.lnk", strPath, strTaskName);

#ifdef SHOW_S2_DEBUG_MSGBOXES
    WCHAR sz[1024];
    swprintf_s(sz, 1024, L"strShortcutFilePath='%s' strTaskName='%s'", strShortcutFilePath, strTaskName);
    MessageBox(NULL, sz, L"CreateTask", MB_OK);
#endif

    // Create shortcut
    hr = CreateShortcut(strLaunchPath, strCommandLineArgs, strShortcutFilePath, bFileTask);

    return hr;
}

//-----------------------------------------------------------------------------
HRESULT RemoveTasks(WCHAR* strGDFBinPath) // valid GameInstance GUID that was passed to AddGame()
{
    assert(strGDFBinPath);
    if (strGDFBinPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    HRESULT hr;
    WCHAR strPath[512] = {0};
    WCHAR strGUID[256];
    WCHAR strLocalAppData[MAX_PATH];
    WCHAR strCommonAppData[MAX_PATH];

    // Get base path based on install scope
    if (FAILED(hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strLocalAppData)))
    {
        return hr;
    }
    if (FAILED(hr = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, strCommonAppData)))
    {
        return hr;
    }
    
    GUID InstanceGUID = GUID_NULL;
    if (FAILED(hr = RetrieveGUIDForApplication(strGDFBinPath, &InstanceGUID)))
    {
        return hr;
    }
    // Convert GUID to string
    if (StringFromGUID2(InstanceGUID, strGUID, ARRAYSIZE(strGUID)) == 0)
        return E_FAIL;

    if (FAILED(hr = swprintf_s(strPath, 512, L"%s\\Microsoft\\Windows\\GameExplorer\\%s", strLocalAppData,
                                      strGUID)))
    {
        return hr;
    }

    SHFILEOPSTRUCT fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = strPath;
    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    SHFileOperation(&fileOp);

    if (FAILED(hr = swprintf_s(strPath, 512, L"%s\\Microsoft\\Windows\\GameExplorer\\%s", strCommonAppData,
                                      strGUID)))
    {
        return hr;
    }

    ZeroMemory(&fileOp, sizeof(SHFILEOPSTRUCT));
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = strPath;
    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    SHFileOperation(&fileOp);

    return S_OK;
}

//-----------------------------------------------------------------------------
HRESULT CreateSingleTask(IXMLDOMNode* pTaskNode, 
                           WCHAR* strGDFBinPath, 
                           WCHAR* strGameInstallPath, 
                           GAME_INSTALL_SCOPE InstallScope,
                           BOOL bPrimaryTask, 
                           BOOL bSupportTask)
{
    assert(pTaskNode);
    assert(strGDFBinPath);
    assert(strGameInstallPath);

    if (pTaskNode == NULL || strGDFBinPath == NULL || strGameInstallPath == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    WCHAR strTaskName[256] = {0};
    if (bPrimaryTask)
    {
        wcscpy_s( strTaskName, 256, L"Play");
    }
    else
    {
        hr = GetXMLAttribute(pTaskNode, L"name", strTaskName, 256);
    }
    if (SUCCEEDED(hr))
    {
        WCHAR strTaskID[256] = {0};
        if (bPrimaryTask)
        {
            wcscpy_s( strTaskID, 256, L"0");
        }
        else
        {
            hr = GetXMLAttribute(pTaskNode, L"index", strTaskID, 256);
        }
        
        if (SUCCEEDED(hr))
        {
            int nTaskID = _wtoi(strTaskID);
            IXMLDOMNode* pFileTaskNode = NULL;
            pTaskNode->selectSingleNode(L"FileTask", &pFileTaskNode);
            if (pFileTaskNode != NULL)
            {
                WCHAR strPath[256] = {0};
                hr = GetXMLAttribute(pFileTaskNode, L"path", strPath, 256);
                if (SUCCEEDED(hr))
                {
                    WCHAR strCommandLineArgs[256] = {0};
                    hr = GetXMLAttribute(pFileTaskNode, L"arguments", strCommandLineArgs, 256);
                    if (SUCCEEDED(hr))
                    {
                        WCHAR strBaseKnownFolderID[256] = {0};
                        hr = GetXMLAttribute(pFileTaskNode, L"baseKnownFolderID", strBaseKnownFolderID, 256);
                        if (hr == S_OK)
                        {
                            int nCsidl;
                            hr = GetBaseKnownFolderCsidl(strBaseKnownFolderID, &nCsidl);
                            if (hr == S_OK)
                            {
                                WCHAR strFolderPath[MAX_PATH];
                                hr = SHGetFolderPath(NULL, nCsidl, NULL, SHGFP_TYPE_CURRENT, strFolderPath);
                                if (SUCCEEDED(hr))
                                {
                                    WCHAR strLaunchPath[MAX_PATH] = {0};
                                    swprintf_s( strLaunchPath, MAX_PATH, L"%s\\%s", strFolderPath, strPath);
                                    hr = SubCreateSingleTask(InstallScope, strGDFBinPath, strTaskName, strLaunchPath, strCommandLineArgs, nTaskID, bSupportTask, true);
                                }
                            }
                        }
                        else if(hr == S_FALSE)
                        {
                            WCHAR strLaunchPath[MAX_PATH];
                            swprintf_s( strLaunchPath, MAX_PATH, L"%s%s", strGameInstallPath, strPath);
                            hr = SubCreateSingleTask(InstallScope, strGDFBinPath, strTaskName, strLaunchPath, strCommandLineArgs, nTaskID, bSupportTask, true);
                        }
                    }
                }
            }
            else
            {
                IXMLDOMNode* pURLTaskNode = NULL;
                pTaskNode->selectSingleNode(L"URLTask", &pURLTaskNode);
                if (pURLTaskNode != NULL)
                {
                    WCHAR strURLPath[256] = {0};
                    hr = GetXMLAttribute(pURLTaskNode, L"Link", strURLPath, 256);
                    if (SUCCEEDED(hr))
                    {
                        hr = SubCreateSingleTask(InstallScope, strGDFBinPath, strTaskName, strURLPath, NULL, nTaskID, bSupportTask, false);
                    }
                }
            }
        }
    }
    
    return hr;
}

//-----------------------------------------------------------------------------
HRESULT CreateTasks(WCHAR* strGDFBinPath, 
                            WCHAR* strGameInstallPath, 
                            GAME_INSTALL_SCOPE InstallScope)
{
    assert(strGDFBinPath);
    assert(strGameInstallPath);

    if (strGDFBinPath == NULL || strGameInstallPath == NULL)
    {
        return E_INVALIDARG;
    }
    
    CGDFParse gdfParse;
    HRESULT hr = gdfParse.ExtractXML( strGDFBinPath );
    if( SUCCEEDED( hr ) )
    {
        IXMLDOMNode* pRootNode = NULL;
        gdfParse.GetXMLRootNode(&pRootNode);
        if (pRootNode != NULL)
        {
            IXMLDOMNode* pPlayTasksNode = NULL;
            pRootNode->selectSingleNode(L"//GameDefinitionFile/GameDefinition/ExtendedProperties/GameTasks/Play", &pPlayTasksNode);
            if (pPlayTasksNode != NULL)
            {
                IXMLDOMNode* pPrimaryPlayTaskNode = NULL;
                pPlayTasksNode->selectSingleNode(L"Primary", &pPrimaryPlayTaskNode);
                if (pPrimaryPlayTaskNode != NULL)
                {
                    hr = CreateSingleTask(pPrimaryPlayTaskNode, strGDFBinPath, strGameInstallPath, InstallScope, true, false);
                    if( SUCCEEDED( hr ) )
                    {
                        // Secondary play tasks
                        IXMLDOMNode* pSecondaryPlayTaskNode;
                        pPrimaryPlayTaskNode->get_nextSibling(&pSecondaryPlayTaskNode);
                        while (pSecondaryPlayTaskNode != NULL)
                        {
                            hr = CreateSingleTask(pSecondaryPlayTaskNode, strGDFBinPath, strGameInstallPath, InstallScope, false, false);
                            pSecondaryPlayTaskNode->get_nextSibling(&pSecondaryPlayTaskNode);
                        }
                    }
                }
               // support tasks
                if (SUCCEEDED(hr))
                {
                    IXMLDOMNode* pSupportTasksNode = NULL;
                    pRootNode->selectSingleNode(L"//GameDefinitionFile/GameDefinition/ExtendedProperties/GameTasks/Support", &pSupportTasksNode);
                    if (pSupportTasksNode != NULL)
                    {
                        IXMLDOMNode* pTaskNode = NULL;
                        pSupportTasksNode->selectSingleNode(L"Task", &pTaskNode);
                        while (pTaskNode != NULL)
                        {
                            hr = CreateSingleTask(pTaskNode, strGDFBinPath, strGameInstallPath, InstallScope, false, true);
                            pTaskNode->get_nextSibling(&pTaskNode);
                        }
                    }
                }
            }
            else
            {
#ifdef SHOW_S1_DEBUG_MSGBOXES 
                WCHAR sz[1024];
                swprintf_s(sz, 1024, L"The game task information is missing! Please check your GDF file and reinstall the game again!");
                MessageBox(NULL, sz, L"GameExplorerInstall", MB_OK);
#endif
                hr = S_FALSE;
            }
        }
    }
    
    return hr;
}

/******************************************************************
 IsV2GDF - test the GDF version
 
********************************************************************/
HRESULT IsV2GDF (
    __in WCHAR* strGDFBinPath, 
    __out BOOL* pfV2GDF
    )
{
    assert(strGDFBinPath);
    assert(pfV2GDF);
#ifdef SHOW_S1_DEBUG_MSGBOXES
        WCHAR sz[1024];
        swprintf_s(sz, 1024, L"IsV2GDF\n");
        MessageBox(NULL, sz, L"IsV2GDF", MB_OK);
#endif

    if (strGDFBinPath == NULL || pfV2GDF == NULL)
    {
        return E_INVALIDARG;
    }
    
    *pfV2GDF = FALSE;
    CGDFParse gdfParse;
    HRESULT hr = gdfParse.ExtractXML( strGDFBinPath );
    if( SUCCEEDED( hr ) )
    {
        IXMLDOMNode* pRootNode = NULL;
        gdfParse.GetXMLRootNode(&pRootNode);
        if (pRootNode != NULL)
        {
            IXMLDOMNode* pPrimaryPlayTasksNode = NULL;
            hr = pRootNode->selectSingleNode(L"//GameDefinitionFile/GameDefinition/ExtendedProperties/GameTasks/Play/Primary", &pPrimaryPlayTasksNode);
            if (S_OK == hr)
            {
                *pfV2GDF = TRUE;
            }
            else if (S_FALSE == hr)
            {
                *pfV2GDF = FALSE;
            }
            else
            {
                hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }
        }
    }
    
    return hr;
}

