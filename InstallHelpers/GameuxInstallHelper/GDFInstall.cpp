//-----------------------------------------------------------------------------
// File: GDFInstall.cpp
//
// Desc: Windows code that calls GameuxInstallHelper sample dll and displays the results.
// The Microsoft Platform SDK or Microsoft Windows SDK is required to compile this sample
//
// (C) Copyright Microsoft Corp.  All rights reserved.
//-----------------------------------------------------------------------------
#define _WIN32_DCOM
#define _CRT_SECURE_NO_DEPRECATE
#include <rpcsal.h>
#include <gameux.h>
#include "GameuxInstallHelper.h"
#include <stdio.h>
#include <shlobj.h>
#include <wbemidl.h>
#include <objbase.h>
#define NO_SHLWAPI_STRFCNS
#include <shellapi.h>
#include <shlwapi.h>

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

struct SETTINGS
{
    WCHAR   strInstallPath[MAX_PATH];
    WCHAR   strGDFBinPath[MAX_PATH];
    bool bEnumMode;
    bool bUninstall;
    bool bUpdate;
    bool bAllUsers;
    bool bSilent;
};

HRESULT EnumAndRemoveGames();
bool ParseCommandLine( SETTINGS* pSettings );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
void DisplayUsage();

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything, and pops
//       up a message box with the results of the GameuxInstallHelper calls
//-----------------------------------------------------------------------------
int PASCAL WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR strCmdLine, _In_ int nCmdShow )
{
    HRESULT hr;
    WCHAR szMsg[512];
    bool bFailure = false;

    SETTINGS settings;
    memset( &settings, 0, sizeof( SETTINGS ) );

    // Set defaults
    WCHAR strSysDir[MAX_PATH];
    GetSystemDirectory( strSysDir, MAX_PATH );
    GetCurrentDirectory( MAX_PATH, settings.strInstallPath );
    PathCombine( settings.strGDFBinPath, settings.strInstallPath, L"GDFExampleBinary.dll");

    if( !ParseCommandLine( &settings ) )
    {
        return 0;
    }

    if( settings.bEnumMode )
    {
        EnumAndRemoveGames();
        return 0;
    }

    if( !IsUserAnAdmin() && settings.bAllUsers && !settings.bSilent )
    {
        MessageBox( NULL,
                    L"Warning: GDFInstall.exe does not have administrator privileges.  Installing for all users will fail.\n\n"
                    L"To correct, right click on GDFInstall.exe and run it as an administrator.", L"GDFInstall",
                    MB_OK );
    }

    if( !settings.bUninstall && !settings.bUpdate )
    {
        // Installing

        GAME_INSTALL_SCOPE installScope = ( settings.bAllUsers ) ? GIS_ALL_USERS : GIS_CURRENT_USER;
        // Upon install do this
        // Change the paths in this call to be correct
        hr = GameExplorerInstall( settings.strGDFBinPath, settings.strInstallPath, installScope );
        if( FAILED( hr ) )
        {
            swprintf_s( szMsg, 512, L"Adding game failed: 0x%0.8x\nGDF binary: %s\nGDF Install path: %s\nAll users: %d\n\nNote: This will fail if the game has already been added.  Make sure the game is removed first.",
                hr, settings.strGDFBinPath, settings.strInstallPath, settings.bAllUsers );
            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerInstall" ), MB_OK | MB_ICONINFORMATION );
            bFailure = true;
        }
        else 
        {
            swprintf_s( szMsg, 512, L"GDF binary: %s\nGDF Install path: %s\nAll users: %d\n\n",
                             settings.strGDFBinPath, settings.strInstallPath, settings.bAllUsers );

            wcscat_s( szMsg, 512, L"Adding GDF binary succeeded\n" );
            wcscat_s( szMsg, 512, L"\nGDFInstall.exe /? for a list of options" );

            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerInstall" ), MB_OK | MB_ICONINFORMATION );
        }
    }
    else if ( settings.bUpdate )
    {
        // Updating
        hr = GameExplorerUpdate( settings.strGDFBinPath );
        if( FAILED( hr ) )
        {
            swprintf_s( szMsg, 256, L"Updating game failed: 0x%0.8x", hr );
            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerUpdate" ), MB_OK | MB_ICONINFORMATION );
            bFailure = true;
        }
        else 
        {
            swprintf_s( szMsg, 256, L"Update of '%s' succeeded\n", settings.strGDFBinPath );
            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerUpdate" ), MB_OK | MB_ICONINFORMATION );
        }
    }
    else if ( settings.bUninstall )
    {
        // Uninstalling
        hr = GameExplorerUninstall( settings.strGDFBinPath );
        if( FAILED( hr ) )
        {
            swprintf_s( szMsg, 256, L"Removing game failed: 0x%0.8x", hr );
            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerUninstall" ), MB_OK | MB_ICONINFORMATION );
            bFailure = true;
        }
        else 
        {
            swprintf_s( szMsg, 256, L"Uninstall of '%s' succeeded\n", settings.strGDFBinPath );
            if( !settings.bSilent )
                MessageBox( NULL, szMsg, TEXT( "GameExplorerUninstall" ), MB_OK | MB_ICONINFORMATION );
        }
    }

    return 0;
}


//-----------------------------------------------------------------------------
// Converts a string to a GUID
//-----------------------------------------------------------------------------
BOOL ConvertStringToGUID( const WCHAR* strIn, GUID* pGuidOut )
{
    UINT aiTmp[10];

    if( swscanf( strIn, L"{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}",
                 &pGuidOut->Data1,
                 &aiTmp[0], &aiTmp[1],
                 &aiTmp[2], &aiTmp[3],
                 &aiTmp[4], &aiTmp[5],
                 &aiTmp[6], &aiTmp[7],
                 &aiTmp[8], &aiTmp[9] ) != 11 )
    {
        ZeroMemory( pGuidOut, sizeof( GUID ) );
        return FALSE;
    }
    else
    {
        pGuidOut->Data2 = ( USHORT )aiTmp[0];
        pGuidOut->Data3 = ( USHORT )aiTmp[1];
        pGuidOut->Data4[0] = ( BYTE )aiTmp[2];
        pGuidOut->Data4[1] = ( BYTE )aiTmp[3];
        pGuidOut->Data4[2] = ( BYTE )aiTmp[4];
        pGuidOut->Data4[3] = ( BYTE )aiTmp[5];
        pGuidOut->Data4[4] = ( BYTE )aiTmp[6];
        pGuidOut->Data4[5] = ( BYTE )aiTmp[7];
        pGuidOut->Data4[6] = ( BYTE )aiTmp[8];
        pGuidOut->Data4[7] = ( BYTE )aiTmp[9];
        return TRUE;
    }
}

//-----------------------------------------------------------------------------
HRESULT EnumAndRemoveGames()
{
    IWbemLocator* pIWbemLocator = NULL;
    IWbemServices* pIWbemServices = NULL;
    BSTR pNamespace = NULL;
    IEnumWbemClassObject* pEnum = NULL;
    GUID guid;
    WCHAR strGameName[256];
    WCHAR strGameGUID[256];
    WCHAR strGDFBinaryPath[256];

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance( __uuidof( WbemLocator ), NULL, CLSCTX_INPROC_SERVER,
                               __uuidof( IWbemLocator ), ( LPVOID* )&pIWbemLocator );
        if( SUCCEEDED( hr ) && pIWbemLocator )
        {
            // Using the locator, connect to WMI in the given namespace.
            pNamespace = SysAllocString( L"\\\\.\\root\\cimv2\\Applications\\Games" );

            hr = pIWbemLocator->ConnectServer( pNamespace, NULL, NULL, 0L,
                                               0L, NULL, NULL, &pIWbemServices );
            if( SUCCEEDED( hr ) && pIWbemServices != NULL )
            {
                // Switch security level to IMPERSONATE. 
                CoSetProxyBlanket( pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                                   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0 );

                BSTR bstrQueryType = SysAllocString( L"WQL" );

                WCHAR szQuery[1024];
                wcscpy_s( szQuery, 1024, L"SELECT * FROM GAME" );
                BSTR bstrQuery = SysAllocString( szQuery );

                hr = pIWbemServices->ExecQuery( bstrQueryType, bstrQuery,
                                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                                NULL, &pEnum );
                if( SUCCEEDED( hr ) )
                {
                    IWbemClassObject* pGameClass = NULL;
                    DWORD uReturned = 0;
                    BSTR pPropName = NULL;

                    // Get the first one in the list
                    for(; ; )
                    {
                        hr = pEnum->Next( 5000, 1, &pGameClass, &uReturned );
                        if( SUCCEEDED( hr ) && uReturned != 0 && pGameClass != NULL )
                        {
                            VARIANT var;

                            // Get the InstanceID string
                            pPropName = SysAllocString( L"InstanceID" );
                            hr = pGameClass->Get( pPropName, 0L, &var, NULL, NULL );
                            if( SUCCEEDED( hr ) && var.vt == VT_BSTR )
                            {
                                wcscpy_s( strGameGUID, 256, var.bstrVal );
                                ConvertStringToGUID( var.bstrVal, &guid );
                            }
                            if( pPropName ) SysFreeString( pPropName );

                            // Get the InstanceID string
                            pPropName = SysAllocString( L"Name" );
                            hr = pGameClass->Get( pPropName, 0L, &var, NULL, NULL );
                            if( SUCCEEDED( hr ) && var.vt == VT_BSTR )
                            {
                                wcscpy_s( strGameName, 256, var.bstrVal );
                            }
                            if( pPropName ) SysFreeString( pPropName );

                            // Get the InstanceID string
                            pPropName = SysAllocString( L"GDFBinaryPath" );
                            hr = pGameClass->Get( pPropName, 0L, &var, NULL, NULL );
                            if( SUCCEEDED( hr ) && var.vt == VT_BSTR )
                            {
                                wcscpy_s( strGDFBinaryPath, 256, var.bstrVal );
                            }
                            if( pPropName ) SysFreeString( pPropName );

                            WCHAR szMsg[256];
                            swprintf_s( szMsg, 256, L"Remove %s [%s] [%s]?", strGameName, strGDFBinaryPath,
                                             strGameGUID );
                            if( IDYES == MessageBox( NULL, szMsg, L"GDFInstall", MB_YESNO ) )
                            {
                                GameExplorerUninstall( strGDFBinaryPath );
                            }
                            SAFE_RELEASE( pGameClass );
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                SAFE_RELEASE( pEnum );
            }

            if( pNamespace ) SysFreeString( pNamespace );
            SAFE_RELEASE( pIWbemServices );
        }

        SAFE_RELEASE( pIWbemLocator );
        CoUninitialize();
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings )
{
    WCHAR* strCmdLine;
    WCHAR* strArg;

    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW( GetCommandLine(), &nNumArgs );
    for( int iArg = 1; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"enum" ) )
            {
                pSettings->bEnumMode = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"u" ) )
            {
                pSettings->bUninstall = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"r" ) )
            {
                pSettings->bUpdate = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"allusers" ) )
            {
                pSettings->bAllUsers = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"installpath" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    strArg = pstrArgList[++iArg];
                    wcscpy_s( pSettings->strInstallPath, MAX_PATH, strArg );
                    continue;
                }

                if( !pSettings->bSilent )
                    MessageBox( NULL, L"Incorrect flag usage: /installpath\n", L"GDFInstall", MB_OK );
                continue;
            }

            if( IsNextArg( strCmdLine, L"silent" ) )
            {
                pSettings->bSilent = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"?" ) )
            {
                DisplayUsage();
                return false;
            }
        }
        else
        {
            // Handle non-flag args as seperate input files
            PathCombine( pSettings->strGDFBinPath, pSettings->strInstallPath, strCmdLine );
            continue;
        }
    }
    LocalFree( pstrArgList );
    return true;
}


//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
    int nArgLen = ( int )wcslen( strArg );
    if( _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && strCmdLine[nArgLen] == 0 )
        return true;

    return false;
}


//--------------------------------------------------------------------------------------
void DisplayUsage()
{
    MessageBox( NULL,
                L"GDFInstall - a command line sample to show how to register with Game Explorer\n"
                L"\n"
                L"Usage: GDFInstall.exe [options] <gdf binary>\n"
                L"\n"
                L"where:\n"
                L"\n"
                L"  [/silent]\t\tSilent mode.  No message boxes\n"
                L"  [/enum]\t\tEnters enum mode where each installed GDF is enumerated\n"
                L"  \t\tand the user is prompted to uninstalled. Other arguments are ignored.\n"
                L"  [/u]\t\tUninstalls the game instead of installing\n"
                L"  [/r]\t\tRefresh/update the game instead of installing or uninstalling\n"
                L"  [/allusers]\tInstalls the game for all users.  Defaults to current user\n"
                L"  \t\tNote: This requires the process have adminstrator privledges\n"
                L"  [/installpath x]\tSets the install path for the game. Defaults to the current working directory\n"
                L"  <gdf binary>\tThe path to the GDF binary to install or remove.\n"
                L"  \t\tDefaults to GDFExampleBinary.dll in current working directory.\n"
                L"  \t\tGDFExampleBinary.dll is a sample GDF binary in the DXSDK.\n"
                , L"GDFInstall", MB_OK );

}
