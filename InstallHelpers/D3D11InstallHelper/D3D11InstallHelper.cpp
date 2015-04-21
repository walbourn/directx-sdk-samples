//--------------------------------------------------------------------------------------
// File: D3D11InstallHelper.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#define _WIN32_DCOM

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <crtdbg.h>
#include <assert.h>
#include <stdio.h>

#include <shlobj.h>
#include <shellapi.h>
#include <wuapi.h>

#include "D3D11InstallHelper.h"

#if defined(_DEBUG)
#define DEBUG_MSG(a,b) { WCHAR sz[1024]; swprintf_s( sz, 1024, a, b); OutputDebugString( sz ); }
#else
#define DEBUG_MSG(a,b)
#endif

//Used to debug MSI custom actions
//#define SHOW_DEBUG_MSGBOXES

//--------------------------------------------------------------------------------------
// Checks the system for the current status of the Direct3D 11 Runtime.
//--------------------------------------------------------------------------------------
STDAPI CheckDirect3D11Status( UINT *pStatus )
{
    if ( !pStatus )
        return E_INVALIDARG;

    // OS Version check tells us most of what we need to know
    OSVERSIONINFOEX osinfo;
    osinfo.dwOSVersionInfoSize = sizeof(osinfo);
    if ( !GetVersionEx( (OSVERSIONINFO*)&osinfo ) )
    {
        HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
        DEBUG_MSG( L"CheckDirect3D11Status: GetOsVersionEx failed with HRESULT %x\n", hr )
        return hr;
    }

    if ( osinfo.dwMajorVersion > 6
         || ( osinfo.dwMajorVersion == 6 && osinfo.dwMinorVersion >= 1 ) )
    {
        // Windows 7/Server 2008 R2 (6.1) and later versions of OS already have Direct3D 11
        *pStatus = D3D11IH_STATUS_INSTALLED;
        return S_OK;
    }

    if ( osinfo.dwMajorVersion < 6 )
    {
        // Windows XP, Windows Server 2003, and earlier versions of OS do not support Direct3D 11
        *pStatus = D3D11IH_STATUS_NOT_SUPPORTED;
        return S_OK;
    }

    // We should only get here for version number 6.0

    if ( osinfo.dwBuildNumber > 6002 )
    {
        // Windows Vista/Server 2008 Service Packs after SP2 should already include Direct3D 11
        *pStatus = D3D11IH_STATUS_INSTALLED;
        return S_OK;
    }

    if ( osinfo.dwBuildNumber < 6002 )
    {
        // Windows Vista/Server 2008 SP2 is a prerequisite
        *pStatus = D3D11IH_STATUS_NEED_LATEST_SP;
        return S_OK;
    }

    // Should only get here for Windows Vista or Windows Server 2008 SP2 (6.0.6002)

    HMODULE hd3d = LoadLibrary( L"D3D11.DLL" );
    if ( hd3d )
    {
        FreeLibrary( hd3d );

        // If we find D3D11, we'll assume the Direct3D 11 Runtime is installed
        // (incl. Direct3D 11, DXGI 1.1, WARP10, 10level9, Direct2D, DirectWrite, updated Direct3D 10.1)

        *pStatus = D3D11IH_STATUS_INSTALLED;
        return S_OK;
    }
    else
    {
        // Did not find the D3D11.DLL, so we need KB971644

        // Verify it is a supported architecture for KB971644
        SYSTEM_INFO sysinfo;
        GetSystemInfo( &sysinfo );
    
        switch( sysinfo.wProcessorArchitecture )
        {
        case PROCESSOR_ARCHITECTURE_INTEL:
        case PROCESSOR_ARCHITECTURE_AMD64:
            *pStatus = D3D11IH_STATUS_REQUIRES_UPDATE;
            break;

        default:
            *pStatus = D3D11IH_STATUS_NOT_SUPPORTED;
            break;
        }

        return S_OK;
    }
}


//--------------------------------------------------------------------------------------
// Default update progress callback (if NULL is passed to DoUpdateForDirect3D11
//--------------------------------------------------------------------------------------
static void D3D11UpdateProgressCBDefault( UINT phase, UINT progress, void *pContext )
{
#ifdef _DEBUG
    switch( phase )
    {
    case D3D11IH_PROGRESS_BEGIN:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress Begin %u\n", progress );
        break;
    case D3D11IH_PROGRESS_SEARCHING:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress Searching %u\n", progress );
        break;
    case D3D11IH_PROGRESS_DOWNLOADING:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress Downloading %u\n", progress );
        break;
    case D3D11IH_PROGRESS_INSTALLING:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress Installing %u\n", progress );
        break;
    case D3D11IH_PROGRESS_END:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress End %u\n", progress );
        break;
    default:
        DEBUG_MSG( L"DoUpdateForDirect3D11: Progress Unknown (%u)\n", phase );
        break;
    }
#endif
}


//--------------------------------------------------------------------------------------
// COM callback interfaces for working with Windows Update Agent API
//--------------------------------------------------------------------------------------
template <class I> class IUNK : public I
{
public:
    IUNK( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) : 
        _pfnProgress( pfnProgress ), _pContext( pContext ) {}

    STDMETHODIMP QueryInterface( REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject)
    {
        if ( ppvObject == NULL )
            return E_POINTER;

        if ( riid == __uuidof(IUnknown) || riid == __uuidof(I) )
        {
            *ppvObject = this;
            return S_OK;
        }

        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(void) { return 1; }
    STDMETHODIMP_(ULONG) Release(void) { return 1; }

    D3D11UPDATEPROGRESSCB   _pfnProgress;
    void *                  _pContext;
};

class ISCC : public IUNK<ISearchCompletedCallback> {
public:
    ISCC( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) :
        IUNK<ISearchCompletedCallback>( pfnProgress, pContext )
    {
        _Event = CreateEvent( NULL, FALSE, FALSE, NULL );
    }

    ~ISCC()
    {
        CloseHandle( _Event );
    }

    STDMETHODIMP Invoke( _In_opt_ ISearchJob* job, _In_opt_ ISearchCompletedCallbackArgs* args)
    {
        _pfnProgress( D3D11IH_PROGRESS_SEARCHING, 100, _pContext );
        SetEvent( _Event );
        return S_OK;
    }

    HANDLE _Event;
};

class IDPC : public IUNK<IDownloadProgressChangedCallback> {
public:
    IDPC( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) :
        IUNK<IDownloadProgressChangedCallback>( pfnProgress, pContext ) {}

    STDMETHODIMP Invoke( _In_opt_ IDownloadJob* job, _In_opt_ IDownloadProgressChangedCallbackArgs* args)
    {
        IDownloadProgress* prg = NULL;
        if ( args && SUCCEEDED( args->get_Progress( &prg ) ) )
        {
            long percent = 0;
            prg->get_PercentComplete( &percent );

            _pfnProgress( D3D11IH_PROGRESS_DOWNLOADING, percent, _pContext );

            prg->Release();
        }
        return S_OK;
    }
};

class IDCC : public IUNK<IDownloadCompletedCallback> {
public:
    IDCC( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) :
        IUNK<IDownloadCompletedCallback>( pfnProgress, pContext )
    {
        _Event = CreateEvent( NULL, FALSE, FALSE, NULL );
    }

    ~IDCC()
    {
        CloseHandle( _Event );
    }

    STDMETHODIMP Invoke( _In_opt_ IDownloadJob* job, _In_opt_ IDownloadCompletedCallbackArgs* args)
    {
        _pfnProgress( D3D11IH_PROGRESS_DOWNLOADING, 100, _pContext );
        SetEvent( _Event );
        return S_OK;
    }

    HANDLE _Event;
};

class IIPC : public IUNK<IInstallationProgressChangedCallback> {
public:
    IIPC( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) :
        IUNK<IInstallationProgressChangedCallback>( pfnProgress, pContext ) {}

    STDMETHODIMP Invoke( _In_opt_ IInstallationJob* job, _In_opt_ IInstallationProgressChangedCallbackArgs* args)
    {
        IInstallationProgress* prg = NULL;
        if ( args && SUCCEEDED( args->get_Progress( &prg ) ) )
        {
            long percent = 0;
            prg->get_PercentComplete( &percent );

            _pfnProgress( D3D11IH_PROGRESS_INSTALLING, percent, _pContext );

            prg->Release();
        }
        return S_OK;
    }
};

class IICC : public IUNK<IInstallationCompletedCallback> {
public:
    IICC( D3D11UPDATEPROGRESSCB pfnProgress, void* pContext ) :
        IUNK<IInstallationCompletedCallback>( pfnProgress, pContext )
    {
        _Event = CreateEvent( NULL, FALSE, FALSE, NULL );
    }

    ~IICC()
    {
        CloseHandle( _Event );
    }

    STDMETHODIMP Invoke( _In_opt_ IInstallationJob* job, _In_opt_ IInstallationCompletedCallbackArgs* args)
    {
        _pfnProgress( D3D11IH_PROGRESS_INSTALLING, 100, _pContext );
        SetEvent( _Event );
        return S_OK;
    }

    HANDLE _Event;
};


//--------------------------------------------------------------------------------------
// Performs Windows Update operations to apply the Direct3D 11 Runtime update
// if available. This function requires administrator rights.
//--------------------------------------------------------------------------------------
STDAPI DoUpdateForDirect3D11( DWORD dwFlags, D3D11UPDATEPROGRESSCB pfnProgress,
                              void *pContext, UINT *pResult )
{
    if ( !pResult )
        return E_INVALIDARG;

    if ( pfnProgress == NULL )
    {
        pfnProgress = D3D11UpdateProgressCBDefault;
    }

    // Verify it is a supported architecture
    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );

    switch( sysinfo.wProcessorArchitecture )
    {
    case PROCESSOR_ARCHITECTURE_INTEL:
    case PROCESSOR_ARCHITECTURE_AMD64:
        break;
    
    default:
        *pResult = D3D11IH_RESULT_NOT_SUPPORTED;
        return S_OK;
    }

    // Verify we have Windows Vista/Server 2008 SP2 already installed
    OSVERSIONINFOEX osinfo;
    osinfo.dwOSVersionInfoSize = sizeof(osinfo);
    if ( !GetVersionEx( (OSVERSIONINFO*)&osinfo ) )
    {
        HRESULT hr = HRESULT_FROM_WIN32( GetLastError() );
        DEBUG_MSG( L"DoUpdateForDirect3D11: GetOsVersionEx failed with HRESULT %x\n", hr )
        return hr;
    }

    if ( osinfo.dwMajorVersion != 6 || osinfo.dwMinorVersion != 0 )
    {
        *pResult = D3D11IH_RESULT_NOT_SUPPORTED;
        return S_OK;
    }

    if ( osinfo.dwBuildNumber != 6002 )
    {
        *pResult = D3D11IH_RESULT_NOT_SUPPORTED;
        return S_OK;
    }

    // Requires administrator rights to apply updates
    if( !IsUserAnAdmin() )
    {
        return E_ACCESSDENIED;
    }

    // Initialize Windows Update Agent API
    HRESULT hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if ( FAILED(hr) )
    {
        DEBUG_MSG( L"DoUpdateForDirect3D11: CoInitializeEx failed with HRESULT %x\n", hr )
        return hr;
    }

    IUpdateSession* pWUSession = NULL;
    hr = CoCreateInstance( CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (void**)&pWUSession );
    if ( FAILED(hr) )
    {
        DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to create update session: %x\n", hr )
        CoUninitialize();
        return hr;
    }

    IUpdateCollection* pWUColl = NULL;
    hr = CoCreateInstance( CLSID_UpdateCollection, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateCollection, (void**)&pWUColl );
    if ( FAILED(hr) )
    {
        DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to create update collection: %x\n", hr )
        SAFE_RELEASE( pWUSession )
        CoUninitialize();
        return hr;
    }

    pfnProgress( D3D11IH_PROGRESS_BEGIN, 0, pContext );

    // Search for the update...
    bool update_found = false;
    bool already_installed = false;
    IUpdateSearcher* pWUSearcher = NULL;
    hr = pWUSession->CreateUpdateSearcher( &pWUSearcher );

    if ( SUCCEEDED(hr) )
    {
        if ( dwFlags & D3D11IH_WINDOWS_UPDATE )
        {
            pWUSearcher->put_ServerSelection( ssWindowsUpdate );
        }

        BSTR bstrCriteria = SysAllocString( L"( CategoryIDs contains 'cb090352-c615-4c0f-a2ab-e86220921a2e' )" );

        ISearchResult* pWUResult = NULL;
        ISearchJob* pJob = NULL;
        VARIANT pVar = { 0 };
        ISCC completeCB( pfnProgress, pContext );
        hr = pWUSearcher->BeginSearch( bstrCriteria, &completeCB, pVar, &pJob );

        // Wait for Async search to complete
        if ( SUCCEEDED(hr) )
        {
            pfnProgress( D3D11IH_PROGRESS_SEARCHING, 0, pContext );

            WaitForSingleObject( completeCB._Event, INFINITE );

            hr = pWUSearcher->EndSearch( pJob, &pWUResult );
            pJob->Release();
        }

        SysFreeString( bstrCriteria );

        if ( SUCCEEDED(hr) )
        {
            OperationResultCode code;
            pWUResult->get_ResultCode( &code );

            if ( code == orcSucceeded || code == orcSucceededWithErrors )
            {
                IUpdateCollection* pWUUpdates = NULL;
                hr = pWUResult->get_Updates( &pWUUpdates );
                if ( SUCCEEDED(hr) )
                {
                    LONG count = 0;
                    pWUUpdates->get_Count( &count );

                    for(LONG i = 0; i < count; ++i)
                    {
                        IUpdate *pUpdate = NULL;
                        pWUUpdates->get_Item( i, &pUpdate );

                        VARIANT_BOOL vbool;
                        pUpdate->get_IsInstalled( &vbool );

                        if ( vbool == VARIANT_TRUE )
                        {
                            already_installed = true;
                        }
                        else
                        {
                            LONG ret;
                            pWUColl->Add( pUpdate, &ret );
                            update_found = true;
                        }

                        SAFE_RELEASE( pUpdate )
                    }

                    SAFE_RELEASE( pWUUpdates )
                }
                else
                {
                    DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to get update list from search results: %x\n", hr )
                }
            }
            else
            {
                DEBUG_MSG( L"DoUpdateForDirect3D11: Failed search with operation code: %d\n", code )
                hr = E_FAIL;
            }
        }
        else
        {
            DEBUG_MSG( L"DoUpdateForDirect3D11: Search failed: %x\n", hr ) 
        }

        SAFE_RELEASE( pWUResult )
        SAFE_RELEASE( pWUSearcher )
    }
    else
    {
        DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to create update searcher: %x\n", hr ) 
    }

    // Deal with EULAs...
    if ( update_found && ( dwFlags & D3D11IH_QUIET ) )
    {
        LONG count = 0;
        pWUColl->get_Count( &count );

        for(LONG i = 0; i < count; ++i )
        {
            IUpdate *pUpdate = NULL;
            pWUColl->get_Item( i, &pUpdate );

            pUpdate->AcceptEula();
                        
            SAFE_RELEASE( pUpdate )
        }
    }

    // Download the update...
    bool update_downloaded = false;

    if ( update_found )
    {
        IUpdateDownloader* pWUDownloader = NULL;
        hr = pWUSession->CreateUpdateDownloader( &pWUDownloader );
         
        if ( SUCCEEDED(hr) )
        {
            hr = pWUDownloader->put_Updates( pWUColl );

            if ( SUCCEEDED(hr) )
            {
                IDownloadResult *pWUResult = NULL;
                IDownloadJob* pJob = NULL;
                VARIANT pVar = { 0 };
                IDPC progressCB( pfnProgress, pContext );
                IDCC completeCB( pfnProgress, pContext );
                hr = pWUDownloader->BeginDownload( &progressCB, &completeCB, pVar, &pJob );

                // Wait for Async download to complete
                if ( SUCCEEDED(hr) )
                {
                    pfnProgress( D3D11IH_PROGRESS_DOWNLOADING, 0, pContext );

                    WaitForSingleObject( completeCB._Event, INFINITE );
        
                    hr = pWUDownloader->EndDownload( pJob, &pWUResult );
                    pJob->Release();
                }

                if ( SUCCEEDED(hr) )
                {
                    OperationResultCode code;
                    pWUResult->get_ResultCode( &code );

                    if ( code == orcSucceeded || code == orcSucceededWithErrors )
                    {
                        update_downloaded = true;
                    }
                }
                else
                {
                    DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to download update: %x\n", hr )
                }

                SAFE_RELEASE( pWUResult )
            }
            else
            {
                DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to assign update to downloader: %x\n", hr )
            }

            SAFE_RELEASE( pWUDownloader )
        }
        else
        {
            DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to create update downloader: %x\n", hr )
        }
    }

    // Install the update...
    bool update_installed = false;
    bool reboot_required = false;

    if ( update_downloaded )
    {
        IUpdateInstaller* pWUInstaller = NULL;
        hr = pWUSession->CreateUpdateInstaller( &pWUInstaller );

        if ( SUCCEEDED(hr) )
        {
            hr = pWUInstaller->put_Updates( pWUColl );

            if ( SUCCEEDED(hr) )
            {
                IUpdateInstaller2* pWUInstaller2 = NULL;
                if ( dwFlags & D3D11IH_QUIET )
                {
                    hr = pWUInstaller->QueryInterface( __uuidof( IUpdateInstaller2 ), ( LPVOID* )&pWUInstaller2 );
                    if ( SUCCEEDED(hr) )
                    {
                        // This can cause some kinds of updates to fail to install because
                        // they disallow force quiet
                        pWUInstaller2->put_ForceQuiet( VARIANT_TRUE );
                    }
                }
    
                IInstallationResult *pWUResult = NULL;
                IInstallationJob* pJob = NULL;
                VARIANT pVar = { 0 };
                IIPC progressCB( pfnProgress, pContext );
                IICC completeCB( pfnProgress, pContext );
                hr = pWUInstaller->BeginInstall( &progressCB, &completeCB, pVar, &pJob );

                // Wait for Async download to complete
                if ( SUCCEEDED(hr) )
                {
                    pfnProgress( D3D11IH_PROGRESS_INSTALLING, 0, pContext );

                    WaitForSingleObject( completeCB._Event, INFINITE );
        
                    hr = pWUInstaller->EndInstall( pJob, &pWUResult );
                    pJob->Release();
                }

                if ( SUCCEEDED(hr) )
                {
                    OperationResultCode code;
                    pWUResult->get_ResultCode( &code );

                    if ( code == orcSucceeded || code == orcSucceededWithErrors )
                    {
                        update_installed = true;

                        VARIANT_BOOL vbool;
                        pWUResult->get_RebootRequired( &vbool );

                        if ( vbool == VARIANT_TRUE )
                        {
                            reboot_required = true;
                        }
                    }
                }
                else
                {
                    DEBUG_MSG( L"DoUpdateforDirect3D11: Failed to install update: %x\n", hr )
                }

                SAFE_RELEASE( pWUResult );
                SAFE_RELEASE( pWUInstaller2 )
            }
            else
            {
                DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to assign update to installer: %x\n", hr )
            }

            SAFE_RELEASE( pWUInstaller )
        }
        else
        {
            DEBUG_MSG( L"DoUpdateForDirect3D11: Failed to create update installer: %x\n", hr )
        }
    }

    pfnProgress( D3D11IH_PROGRESS_END, 0, pContext );

    SAFE_RELEASE( pWUColl )
    SAFE_RELEASE( pWUSession )
    CoUninitialize();

    if ( FAILED(hr) )
    {
        switch (hr)
        {
        // We use 'magic numbers' here to avoid reliance on an updated Windows SDK header
        // wuerror.h is included in the Windows SDK 7.0A or later
        case 0x8024402C: // WU_E_PT_WINHTTP_NAME_NOT_RESOLVED
        case 0x80244016: // WU_E_PT_HTTP_STATUS_BAD_REQUEST 
        case 0x80244017: // WU_E_PT_HTTP_STATUS_DENIED  
        case 0x80244018: // WU_E_PT_HTTP_STATUS_FORBIDDEN 
        case 0x80244019: // WU_E_PT_HTTP_STATUS_NOT_FOUND
        case 0x8024401A: // WU_E_PT_HTTP_STATUS_BAD_METHOD 
        case 0x8024401B: // WU_E_PT_HTTP_STATUS_PROXY_AUTH_REQ
        case 0x8024401C: // WU_E_PT_HTTP_STATUS_REQUEST_TIMEOUT 
        case 0x8024401D: // WU_E_PT_HTTP_STATUS_CONFLICT
        case 0x8024401E: // WU_E_PT_HTTP_STATUS_GONE
        case 0x8024401F: // WU_E_PT_HTTP_STATUS_SERVER_ERROR 
        case 0x80244020: // WU_E_PT_HTTP_STATUS_NOT_SUPPORTED 
        case 0x80244021: // WU_E_PT_HTTP_STATUS_BAD_GATEWAY 
        case 0x80244022: // WU_E_PT_HTTP_STATUS_SERVICE_UNAVAIL 
        case 0x80244023: // WU_E_PT_HTTP_STATUS_GATEWAY_TIMEOUT
        case 0x80244024: // WU_E_PT_HTTP_STATUS_VERSION_NOT_SUP 

        case 0x8024f004: // WU_E_SERVER_BUSY 
        case 0x8024001E: // WU_E_SERVICE_STOP 
        case 0x8024001F: // WU_E_NO_CONNECTION
        case 0x80240021: // WU_E_TIME_OUT
        case 0x80246005: // WU_E_DM_NONETWORK
        case 0x80240009: // WU_E_OPERATIONINPROGRESS

            DEBUG_MSG( L"DoUpdateForDirect3D11: Failed with network/server error code %x, returning WU_SERVICE_ERROR result. Retry is possible\n", hr );
            *pResult = D3D11IH_RESULT_WU_SERVICE_ERROR;
            return S_OK;
        
        default:
            return hr;
        }
    }

    if ( update_found )
    {
        if ( update_downloaded )
        {
            if ( update_installed )
            {
                *pResult = (reboot_required) ? D3D11IH_RESULT_SUCCESS_REBOOT : D3D11IH_RESULT_SUCCESS;
            }
            else
            {
                *pResult = D3D11IH_RESULT_UPDATE_INSTALL_FAILED;
            }
        }
        else
        {
            *pResult = D3D11IH_RESULT_UPDATE_DOWNLOAD_FAILED;
        }
    }
    else if ( already_installed )
    {
        *pResult = D3D11IH_RESULT_SUCCESS;
    }
    else
    {
        *pResult = D3D11IH_RESULT_UPDATE_NOT_FOUND;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Simplified entry-points for InstallShield integration
//--------------------------------------------------------------------------------------
extern "C" int __cdecl CheckDirect3D11StatusIS()
{
    UINT status;
    HRESULT hr = CheckDirect3D11Status( &status );
    if ( SUCCEEDED( hr ) )
    {
        return static_cast<int>(status);
    }
    else
    {
        DEBUG_MSG( L"CheckDirect3D11StatusIS: Failure code %x, returning -1\n", hr );
        return -1;
    }
}

extern "C" int __cdecl DoUpdateForDirect3D11IS( BOOL bQuiet )
{
    UINT result;
    HRESULT hr = DoUpdateForDirect3D11( bQuiet ? D3D11IH_QUIET : 0, NULL, NULL, &result );
    if ( SUCCEEDED( hr ) )
    {
        return static_cast<int>(result);
    }
    else
    {
        DEBUG_MSG( L"DoUpdateForDirect3D11IS: Failure code %x, returning -1\n", hr );
        return -1;
    }
}


//--------------------------------------------------------------------------------------
// Gets a property from MSI.
// Deferred custom action can only access the property called "CustomActionData"
//--------------------------------------------------------------------------------------
static LPWSTR GetPropertyFromMSI( MSIHANDLE hMSI, LPCWSTR szPropName )
{
    DWORD dwSize = 0, dwBufferLen = 0;
    LPWSTR szValue = NULL;

    WCHAR empty[] = L"";
    UINT uErr = MsiGetProperty( hMSI, szPropName, empty, &dwSize );
    if( ( ERROR_SUCCESS == uErr ) || ( ERROR_MORE_DATA == uErr ) )
    {
        ++dwSize; // Add NULL term
        dwBufferLen = dwSize;
        szValue = new WCHAR[ dwBufferLen ];
        if( szValue )
        {
            uErr = MsiGetProperty( hMSI, szPropName, szValue, &dwSize );
            if( ( ERROR_SUCCESS != uErr ) )
            {
                // Cleanup on failure
                delete[] szValue;
                szValue = NULL;
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

#define TOSTRING(a) case a: szStatus = L#a; break;


//--------------------------------------------------------------------------------------
// MSI integration entry-points
//--------------------------------------------------------------------------------------
UINT WINAPI SetD3D11InstallMSIProperties( MSIHANDLE hModule )
{
    WCHAR szCustomActionData[1024] = {0};

    WCHAR* szSourceDir = GetPropertyFromMSI( hModule, L"SourceDir" );   
    assert( szSourceDir != NULL );
    wcscpy_s( szCustomActionData, 1024, szSourceDir );

    WCHAR* szRelativePathToD3D11IH = GetPropertyFromMSI( hModule, L"RelativePathToD3D11IH" );
    if ( szRelativePathToD3D11IH != NULL )
    {
        wcscat_s( szCustomActionData, 1024, szRelativePathToD3D11IH );
    }

    size_t nLength = wcsnlen( szCustomActionData, 1024 );
    if( nLength > 0 && nLength < 1024 && szCustomActionData[nLength - 1] != L'\\' )
        wcscat_s( szCustomActionData, 1024, L"\\" );

    // Set the CustomActionData property for the deferred custom action
    MsiSetProperty( hModule, L"Direct3D11DoInstall", szCustomActionData );

    // Set a property for other MSI actions to use to reference the check status
    WCHAR* szStatus = L"ERROR";
    UINT status;
    if ( SUCCEEDED( CheckDirect3D11Status( &status ) ) )
    {
        switch( status )
        {
        TOSTRING(D3D11IH_STATUS_INSTALLED)
        TOSTRING(D3D11IH_STATUS_NOT_SUPPORTED)
        TOSTRING(D3D11IH_STATUS_REQUIRES_UPDATE)
        TOSTRING(D3D11IH_STATUS_NEED_LATEST_SP)
        }
    }

    MsiSetProperty( hModule, L"D3D11IH_STATUS", szStatus );

#ifdef SHOW_DEBUG_MSGBOXES
    WCHAR sz[1024];
    swprintf_s( sz, 1024, L"RelativePathToD3D11IH ='%s' szSourceDir=%s szCustomActionData=%s, D3D11IH_STATUS=%s\n",
                            szRelativePathToD3D11IH, szSourceDir, szCustomActionData, szStatus );
    MessageBox( NULL, sz, L"SetD3D11InstallMSIProperties", MB_OK );
#endif

    delete [] szRelativePathToD3D11IH;
    delete [] szSourceDir;

    return ERROR_SUCCESS;
}

UINT WINAPI DoD3D11InstallUsingMSI( MSIHANDLE hModule )
{
    WCHAR* szCustomActionData = GetPropertyFromMSI( hModule, L"CustomActionData" );

    if( szCustomActionData )
    {
#ifdef SHOW_DEBUG_MSGBOXES
        WCHAR sz[1024];
        swprintf_s( sz, 1024, L"szCustomActionData=%s\n",
                    szCustomActionData );
        MessageBox( NULL, sz, L"DoD3D11InstallUsingMSI", MB_OK );
#endif

        WCHAR szPathToD3D11IH[MAX_PATH] = {0};
        wcscpy_s( szPathToD3D11IH, MAX_PATH, szCustomActionData );

        WCHAR szD3D11IH[MAX_PATH];
        wcscpy_s( szD3D11IH, MAX_PATH,szPathToD3D11IH );
        wcscat_s( szD3D11IH, MAX_PATH, L"D3D11Install.exe" );

        UINT status = (UINT)-1;
        CheckDirect3D11Status( &status );

        if ( status == D3D11IH_STATUS_NEED_LATEST_SP
             || status == D3D11IH_STATUS_REQUIRES_UPDATE )
        {
            SHELLEXECUTEINFO info;
            memset( &info, 0, sizeof(info) );
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC | SEE_MASK_NOCLOSEPROCESS;
            info.lpVerb = L"open";
            info.lpFile = szD3D11IH;
            info.lpParameters = L"/minimal /y";
            info.lpDirectory = szPathToD3D11IH;
            info.nShow = SW_SHOW;

            if ( ShellExecuteEx( &info ) )
            {
                // Wait for process to finish
                WaitForSingleObject( info.hProcess, INFINITE );

                DWORD ecode = 0;
                GetExitCodeProcess( info.hProcess, &ecode );

                if (ecode == 1)                 
                {
                    // No other way for a deferred custom action to return data
                    GlobalAddAtomW( L"D3D11InstallHelperNeedsReboot" );
                }
            }
#ifdef SHOW_DEBUG_MSGBOXES
            else
            {
                WCHAR sz[1024];
                swprintf_s( sz, 1024, L"Failed to launch=%s\n", szD3D11IH );
                MessageBox( NULL, sz, L"DoD3D11InstallUsingMSI", MB_OK );
            }
#endif
        }
#ifdef SHOW_DEBUG_MSGBOXES
        else
        {
            WCHAR sz[1024];
            swprintf_s( sz, 1024, L"No need to launch=%s\n", szD3D11IH );
            MessageBox( NULL, sz, L"DoD3D11InstallUsingMSI", MB_OK );
        }
#endif

        delete [] szCustomActionData;
    }
#ifdef SHOW_DEBUG_MSGBOXES
    else
    {
        WCHAR sz[1024];
        swprintf_s( sz, 1024, L"CustomActionData property not found\n" );
        MessageBox( NULL, sz, L"DoD3D11InstallUsingMSI", MB_OK );
    }
#endif

    // Ignore success/failure and continue on with install
    return ERROR_SUCCESS;

}

UINT WINAPI FinishD3D11InstallUsingMSI( MSIHANDLE hModule )
{
    ATOM atomReboot = GlobalFindAtomW(L"D3D11InstallHelperNeedsReboot");
    if (atomReboot != 0)
    {
#ifdef SHOW_DEBUG_MSGBOXES
        MessageBox( NULL, L"Reboot is required", L"FinishD3D11InstallUsingMSI", MB_OK );
#endif
        MsiSetMode(hModule, MSIRUNMODE_REBOOTATEND, TRUE);

        // We do not delete the atom as we do not have admin rights, and
        // other installers might need the signal until we actually reboot
    }
#ifdef SHOW_DEBUG_MSGBOXES
    else
    { 
        MessageBox( NULL, L"Reboot is NOT required", L"FinishD3D11InstallUsingMSI", MB_OK );
    }
#endif
    return ERROR_SUCCESS;
}
