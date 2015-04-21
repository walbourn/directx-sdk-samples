//--------------------------------------------------------------------------------------
// File: FirewallInstallHelper.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
// The Microsoft Platform SDK or Microsoft Windows SDK is required to compile this sample
#include <netfw.h>
#include <crtdbg.h>
#include <msi.h>
#include <msiquery.h>
#include <strsafe.h>
#include <assert.h>
#include "FirewallInstallHelper.h"

// Uncomment to get a debug messagebox
//#define SHOW_DEBUG_MSGBOXES

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LPTSTR GetPropertyFromMSI( MSIHANDLE hMSI, LPCWSTR szPropName );
INetFwProfile* GetFirewallProfile();


//--------------------------------------------------------------------------------------
// This stores the install location, generates a instance GUID one hasn't been set, and 
// sets up the CustomActionData properties for the deferred custom actions
//--------------------------------------------------------------------------------------
UINT WINAPI SetMSIFirewallProperties( MSIHANDLE hModule )
{
    WCHAR strFullPath[1024] = {0};
    WCHAR szAdd[1024] = {0};
    WCHAR* szInstallDir = NULL;
    WCHAR* szFriendlyName = GetPropertyFromMSI( hModule, L"FriendlyNameForFirewall" );
    WCHAR* szRelativePath = GetPropertyFromMSI( hModule, L"RelativePathToExeForFirewall" );
    WCHAR* szProductCode = GetPropertyFromMSI( hModule, L"ProductCode" );

    // See if the install location property is set.  If it is, use that.  
    // Otherwise, get the property from TARGETDIR
    bool bGotInstallDir = false;
    if( szProductCode )
    {
        DWORD dwBufferSize = 1024;
        szInstallDir = new WCHAR[dwBufferSize];
        if( ERROR_SUCCESS == MsiGetProductInfo( szProductCode, INSTALLPROPERTY_INSTALLLOCATION,
                                                szInstallDir, &dwBufferSize ) )
            bGotInstallDir = true;
    }
    if( !bGotInstallDir )
        szInstallDir = GetPropertyFromMSI( hModule, L"TARGETDIR" );

    if( szFriendlyName && szRelativePath && szInstallDir )
    {
        // Set the ARPINSTALLLOCATION property to the install dir so that 
        // the uninstall custom action can have it when getting the INSTALLPROPERTY_INSTALLLOCATION
        MsiSetPropertyW( hModule, L"ARPINSTALLLOCATION", szInstallDir );

        // Setup the CustomActionData property for the "RollBackAddToFirewall" and 
        // "RemoveFromExceptionListUsingMSI" deferred custom actions.
        // This should be full path to the exe
        StringCchCopy( strFullPath, 1024, szInstallDir );
        StringCchCat( strFullPath, 1024, szRelativePath );
        MsiSetPropertyW( hModule, L"FirewallRollBackAdd", strFullPath );
        MsiSetPropertyW( hModule, L"FirewallRemove", strFullPath );

        // Setup the CustomActionData property for the "AddToExceptionListUsingMSI" deferred custom action.
        // This should be "<full path to the exe>|<friendly name>"
        StringCchCopy( szAdd, 1024, strFullPath );
        StringCchCat( szAdd, 1024, L"|" );
        StringCchCat( szAdd, 1024, szFriendlyName );
        MsiSetPropertyW( hModule, L"FirewallRollbackRemove", szAdd );
        MsiSetPropertyW( hModule, L"FirewallAdd", szAdd );
    }

#ifdef SHOW_DEBUG_MSGBOXES
    WCHAR sz[1024];
    StringCchPrintf( sz, 1024, L"szFriendlyName='%s' szRelativePath='%s' szInstallDir='%s' strFullPath='%s'",
            szFriendlyName, szRelativePath, szInstallDir, strFullPath );
    MessageBox( NULL, sz, L"SetMSIPropertyOnInstall", MB_OK );
#endif

    if( szFriendlyName ) delete [] szFriendlyName;
    if( szRelativePath ) delete [] szRelativePath;
    if( szInstallDir ) delete [] szInstallDir;
    if( szProductCode ) delete [] szProductCode;

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// The CustomActionData property must be formated like so:
//      "<full path to game exe>|<friendly app name>"
// for example:
//      "C:\MyGame\Game.exe|Example Game"
//--------------------------------------------------------------------------------------
UINT WINAPI AddToExceptionListUsingMSI( MSIHANDLE hModule )
{
    HRESULT hr = E_FAIL;
    WCHAR* szCustomActionData = GetPropertyFromMSI( hModule, L"CustomActionData" );

    if( szCustomActionData )
    {
        WCHAR strGameExeFullPath[MAX_PATH] = {0};
        WCHAR strFriendlyAppName[256] = {0};

        WCHAR* pDelim = wcschr( szCustomActionData, '|' );
        if( pDelim )
        {
            *pDelim = 0;
            StringCchCopy( strFriendlyAppName, 256, pDelim + 1 );
        }

        StringCchCopy( strGameExeFullPath, MAX_PATH, szCustomActionData );
        hr = AddApplicationToExceptionListW( strGameExeFullPath, strFriendlyAppName );

#ifdef SHOW_DEBUG_MSGBOXES
        WCHAR sz[1024];
        StringCchPrintf( sz, 1024, L"strFriendlyAppName='%s' strGameExeFullPath='%s'", strFriendlyAppName, strGameExeFullPath );
        MessageBox( NULL, sz, L"AddToExceptionListUsingMSI", MB_OK );
#endif

        delete [] szCustomActionData;
    }

    return ( SUCCEEDED( hr ) ) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}


//--------------------------------------------------------------------------------------
// The CustomActionData property must be formated like so:
//      "<full path to game exe>"
// for example:
//      "C:\MyGame\Game.exe"
//--------------------------------------------------------------------------------------
UINT WINAPI RemoveFromExceptionListUsingMSI( MSIHANDLE hModule )
{
    HRESULT hr = E_FAIL;
    WCHAR* szCustomActionData = GetPropertyFromMSI( hModule, L"CustomActionData" );
    if( szCustomActionData )
    {
        WCHAR strGameExeFullPath[MAX_PATH] = {0};
        StringCchCopy( strGameExeFullPath, MAX_PATH, szCustomActionData );

#ifdef SHOW_DEBUG_MSGBOXES
        WCHAR sz[1024];
        StringCchPrintf( sz, 1024, L"szCustomActionData='%s'", szCustomActionData );
        MessageBox( NULL, sz, L"RemoveFromExceptionListUsingMSI", MB_OK );
#endif

        hr = RemoveApplicationFromExceptionListW( strGameExeFullPath );

        delete [] szCustomActionData;
    }

    return ( SUCCEEDED( hr ) ) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}


//--------------------------------------------------------------------------------------
// Adds application from exception list
//--------------------------------------------------------------------------------------
STDAPI AddApplicationToExceptionListW( WCHAR* strGameExeFullPath, WCHAR* strFriendlyAppName )
{
    HRESULT hr = E_FAIL;
    bool bCleanupCOM = false;
    BSTR bstrFriendlyAppName = NULL;
    BSTR bstrGameExeFullPath = NULL;
    INetFwAuthorizedApplication* pFwApp = NULL;
    INetFwAuthorizedApplications* pFwApps = NULL;
    INetFwProfile* pFwProfile = NULL;

#ifdef SHOW_DEBUG_MSGBOXES
		WCHAR sz[1024];
		StringCchPrintf( sz, 1024, L"strFriendlyAppName='%s' strGameExeFullPath='%s'", strFriendlyAppName, strGameExeFullPath );
		MessageBox( NULL, sz, L"AddApplicationToExceptionListW", MB_OK );
#endif

    if( strGameExeFullPath == NULL || strFriendlyAppName == NULL )
    {
        assert( false );
        return E_INVALIDARG;
    }

    bstrGameExeFullPath = SysAllocString( strGameExeFullPath );
    bstrFriendlyAppName = SysAllocString( strFriendlyAppName );
    if( bstrGameExeFullPath == NULL || bstrFriendlyAppName == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto LCleanup;
    }

    hr = CoInitialize( 0 );
    bCleanupCOM = SUCCEEDED( hr );

    pFwProfile = GetFirewallProfile();
    if( pFwProfile == NULL )
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    hr = pFwProfile->get_AuthorizedApplications( &pFwApps );
    if( FAILED( hr ) )
        goto LCleanup;

    // Create an instance of an authorized application.
    hr = CoCreateInstance( __uuidof( NetFwAuthorizedApplication ), NULL,
                           CLSCTX_INPROC_SERVER, __uuidof( INetFwAuthorizedApplication ), ( void** )&pFwApp );
    if( FAILED( hr ) )
        goto LCleanup;

    // Set the process image file name.
    hr = pFwApp->put_ProcessImageFileName( bstrGameExeFullPath );
    if( FAILED( hr ) )
        goto LCleanup;

    // Set the application friendly name.
    hr = pFwApp->put_Name( bstrFriendlyAppName );
    if( FAILED( hr ) )
        goto LCleanup;

    // Add the application to the collection.
    hr = pFwApps->Add( pFwApp );

LCleanup:
    if( bstrFriendlyAppName ) SysFreeString( bstrFriendlyAppName );
    if( bstrGameExeFullPath ) SysFreeString( bstrGameExeFullPath );
    if( pFwApp ) pFwApp->Release();
    if( pFwApps ) pFwApps->Release();
    if( pFwProfile ) pFwProfile->Release();
    if( bCleanupCOM ) CoUninitialize();

    return hr;
}


//--------------------------------------------------------------------------------------
// Removes application from exception list
//--------------------------------------------------------------------------------------
STDAPI RemoveApplicationFromExceptionListW( WCHAR* strGameExeFullPath )
{
    HRESULT hr = E_FAIL;
    bool bCleanupCOM = false;
    BSTR bstrGameExeFullPath = NULL;
    INetFwAuthorizedApplications* pFwApps = NULL;
    INetFwProfile* pFwProfile = NULL;

#ifdef SHOW_DEBUG_MSGBOXES
		WCHAR sz[1024];
		StringCchPrintf( sz, 1024, L"strGameExeFullPath='%s'", strGameExeFullPath );
		MessageBox( NULL, sz, L"RemoveApplicationFromExceptionListW", MB_OK );
#endif

    if( strGameExeFullPath == NULL )
    {
        assert( false );
        return E_INVALIDARG;
    }

    bstrGameExeFullPath = SysAllocString( strGameExeFullPath );
    if( bstrGameExeFullPath == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto LCleanup;
    }

    hr = CoInitialize( 0 );
    bCleanupCOM = SUCCEEDED( hr );

    pFwProfile = GetFirewallProfile();
    if( pFwProfile == NULL )
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    // Retrieve the authorized application collection.
    hr = pFwProfile->get_AuthorizedApplications( &pFwApps );
    if( FAILED( hr ) )
        goto LCleanup;

    // Remove the application from the collection.
    hr = pFwApps->Remove( bstrGameExeFullPath );

LCleanup:
    if( pFwProfile ) pFwProfile->Release();
    if( bCleanupCOM ) CoUninitialize();
    if( bstrGameExeFullPath ) SysFreeString( bstrGameExeFullPath );

    return hr;
}


//--------------------------------------------------------------------------------------
// Adds application from exception list
//--------------------------------------------------------------------------------------
STDAPI AddApplicationToExceptionListA( CHAR* strGameExeFullPath, CHAR* strFriendlyAppName )
{
    WCHAR wstrPath[MAX_PATH] = {0};
    WCHAR wstrName[MAX_PATH] = {0};

    MultiByteToWideChar( CP_ACP, 0, strGameExeFullPath, MAX_PATH, wstrPath, MAX_PATH );
    MultiByteToWideChar( CP_ACP, 0, strFriendlyAppName, MAX_PATH, wstrName, MAX_PATH );

    return AddApplicationToExceptionListW( wstrPath, wstrName );
}


//--------------------------------------------------------------------------------------
// Removes application from exception list
//--------------------------------------------------------------------------------------
STDAPI RemoveApplicationFromExceptionListA( CHAR* strGameExeFullPath )
{
    WCHAR wstrPath[MAX_PATH] = {0};

    MultiByteToWideChar( CP_ACP, 0, strGameExeFullPath, MAX_PATH, wstrPath, MAX_PATH );

    return RemoveApplicationFromExceptionListW( wstrPath );
}


//--------------------------------------------------------------------------------------
// Returns false if the game is not allowed through the firewall
//--------------------------------------------------------------------------------------
BOOL WINAPI CanLaunchMultiplayerGameW( WCHAR* strGameExeFullPath )
{
    bool bCanLaunch = false;
    HRESULT hr = E_FAIL;
    bool bCleanupCOM = false;
    BSTR bstrGameExeFullPath = NULL;
    VARIANT_BOOL vbFwEnabled;
    VARIANT_BOOL vbNotAllowed = VARIANT_FALSE;
    INetFwAuthorizedApplication* pFwApp = NULL;
    INetFwAuthorizedApplications* pFwApps = NULL;
    INetFwProfile* pFwProfile = NULL;

    if( strGameExeFullPath == NULL )
    {
        assert( false );
        return false;
    }

    bstrGameExeFullPath = SysAllocString( strGameExeFullPath );
    if( bstrGameExeFullPath == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto LCleanup;
    }

    hr = CoInitialize( 0 );
    bCleanupCOM = SUCCEEDED( hr );

    pFwProfile = GetFirewallProfile();
    if( pFwProfile == NULL )
    {
        hr = E_FAIL;
        goto LCleanup;
    }

    hr = pFwProfile->get_ExceptionsNotAllowed( &vbNotAllowed );
    if( SUCCEEDED( hr ) && vbNotAllowed != VARIANT_FALSE )
        goto LCleanup;

    // Retrieve the collection of authorized applications.
    hr = pFwProfile->get_AuthorizedApplications( &pFwApps );
    if( FAILED( hr ) )
        goto LCleanup;

    // Attempt to retrieve the authorized application.
    hr = pFwApps->Item( bstrGameExeFullPath, &pFwApp );
    if( FAILED( hr ) )
        goto LCleanup;

    // Find out if the authorized application is enabled.
    hr = pFwApp->get_Enabled( &vbFwEnabled );
    if( FAILED( hr ) )
        goto LCleanup;

    // Check if authorized application is enabled.
    if( vbFwEnabled != VARIANT_FALSE )
        bCanLaunch = true;

LCleanup:
    if( pFwApp ) pFwApp->Release();
    if( pFwApps ) pFwApps->Release();
    if( bCleanupCOM ) CoUninitialize();
    if( bstrGameExeFullPath ) SysFreeString( bstrGameExeFullPath );

    return bCanLaunch;
}


//--------------------------------------------------------------------------------------
// Returns false if the game is not allowed through the firewall
//--------------------------------------------------------------------------------------
BOOL WINAPI CanLaunchMultiplayerGameA( CHAR* strGameExeFullPath )
{
    WCHAR wstrPath[MAX_PATH] = {0};

    MultiByteToWideChar( CP_ACP, 0, strGameExeFullPath, MAX_PATH, wstrPath, MAX_PATH );

    return CanLaunchMultiplayerGameW( wstrPath );
}


//--------------------------------------------------------------------------------------
// Get the INetFwProfile interface for current profile
//--------------------------------------------------------------------------------------
INetFwProfile* GetFirewallProfile()
{
    HRESULT hr;
    INetFwMgr* pFwMgr = NULL;
    INetFwPolicy* pFwPolicy = NULL;
    INetFwProfile* pFwProfile = NULL;

    // Create an instance of the Firewall settings manager
    hr = CoCreateInstance( __uuidof( NetFwMgr ), NULL, CLSCTX_INPROC_SERVER,
                           __uuidof( INetFwMgr ), ( void** )&pFwMgr );
    if( SUCCEEDED( hr ) )
    {
        hr = pFwMgr->get_LocalPolicy( &pFwPolicy );
        if( SUCCEEDED( hr ) )
        {
            pFwPolicy->get_CurrentProfile( &pFwProfile );
        }
    }

    // Cleanup
    if( pFwPolicy ) pFwPolicy->Release();
    if( pFwMgr ) pFwMgr->Release();

    return pFwProfile;
}


//--------------------------------------------------------------------------------------
// Gets a property from MSI.  Deferred custom action can only access the property called
// "CustomActionData"
//--------------------------------------------------------------------------------------
LPTSTR GetPropertyFromMSI( MSIHANDLE hMSI, LPCWSTR szPropName )
{
    DWORD dwSize = 0, dwBufferLen = 0;
    LPWSTR szValue = NULL;

    UINT uErr = MsiGetProperty( hMSI, szPropName, L"", &dwSize );
    if( ( ERROR_SUCCESS == uErr ) || ( ERROR_MORE_DATA == uErr ) )
    {
        dwSize++; // Add NULL term
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


