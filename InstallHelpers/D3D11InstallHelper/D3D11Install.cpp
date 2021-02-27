//-----------------------------------------------------------------------------
// File: D3D11Install.cpp
//
// Desc: Windows code that calls D3D11InstallHelper sample DLL and displays
//       the recommended UI prompts and messages.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#define _WIN32_DCOM

#include "D3D11InstallHelper.h"
#include "resource.h"

#include <stdio.h>
#include <shlobj.h>
#include <process.h>
#include <shellapi.h>

#define MSG_SIZE    1024

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

struct SETTINGS
{
    bool bQuiet;
    bool bPassive;
    bool bMinimal;
    bool bYes;
    bool bWU;
};

struct PROGRESS
{
    CRITICAL_SECTION critSec;
    HANDLE hThread;
    HANDLE hEvent1;
    HANDLE hEvent2;
    HWND hWnd;
    HWND hProgress;
    HWND hStatus;
    UINT phase;
    UINT progress;
};

#if defined(_DEBUG)
#define DEBUG_MSG(sz) OutputDebugString( sz );
#else
#define DEBUG_MSG(sz)
#endif

//#define TEST_LOC

#ifdef TEST_LOC
#pragma warning(disable:4702)
#endif

//-----------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
void DisplayUsage();
unsigned int __stdcall ProgressThread( void* pArg );
void ProgressCallback( UINT phase, UINT progress, void *pContext );

static INT_PTR CALLBACK InfoDialogProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static INT_PTR CALLBACK ProgressDialogProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

static void LocLoadString( UINT uId, LPWSTR lpBuffer, size_t nBufferMax );
static INT_PTR LocDialogBox( UINT uId, DLGPROC lpDialogFunc );
static HWND LocCreateDialog( UINT uId, DLGPROC lpDialogFunc );

//-----------------------------------------------------------------------------
HINSTANCE g_hInstance;
WCHAR g_appName[ 64 ] = { 0 };
WORD g_langId = 1024; // MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT )

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
// Return: Return value 0 = success, not required, not supported, or user aborted
//         Return value 1 = reboot required
//         Return value 2 - error encountered
//-----------------------------------------------------------------------------
int PASCAL WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR strCmdLine, _In_ int nCmdShow )
{
    g_hInstance = hInstance;

    LoadString( hInstance, IDS_APPNAME, g_appName, 64 );

    SETTINGS settings;
    memset( &settings, 0, sizeof( SETTINGS ) );

    if( !ParseCommandLine( &settings ) )
    {
        return 0;
    }

#ifdef TEST_LOC
    // Test dialogs
    UINT dlgs[] =
    {
        IDD_UPDATEDLG, IDD_SPDLG, IDD_NOTFOUNDDLG, IDD_DLFAILDLG, IDD_INSTALLFAILDLG, IDD_WUSRVERROR
    };
    for( int i = 0; i < sizeof(dlgs)/sizeof(UINT); ++i )
    {
        LocDialogBox( dlgs[i], InfoDialogProc );
    }

    // Test progress phases
    PROGRESS prg;
    memset( &prg, 0, sizeof(prg) );
    InitializeCriticalSection( &prg.critSec );
    prg.hEvent1 = CreateEvent( NULL, FALSE, FALSE, NULL );
    prg.hEvent2 = CreateEvent( NULL, FALSE, FALSE, NULL );
    prg.hThread = (HANDLE)_beginthreadex( NULL, 0, ProgressThread, &prg, 0, NULL );

    ProgressCallback( D3D11IH_PROGRESS_BEGIN, 0, &prg );

    ProgressCallback( D3D11IH_PROGRESS_SEARCHING, 0, &prg );
    Sleep(2000);

    ProgressCallback( D3D11IH_PROGRESS_DOWNLOADING, 0, &prg );
    Sleep(2000);

    ProgressCallback( D3D11IH_PROGRESS_INSTALLING, 0, &prg );
    Sleep(2000);

    ProgressCallback( D3D11IH_PROGRESS_END, 0, &prg );

    WaitForSingleObject( prg.hThread, INFINITE );
    CloseHandle( prg.hThread );
    CloseHandle( prg.hEvent1 );
    CloseHandle( prg.hEvent2 );

    if ( prg.hWnd )
        DestroyWindow( prg.hWnd );

    DeleteCriticalSection( &prg.critSec );

    // Test MessageBox strings
    UINT msgs[] =
    {
        IDS_REQUIRE_ADMIN, IDS_FAILED_CHECKSTAT, IDS_ALREADY_PRESENT, IDS_NOT_SUPPORTED,
        IDS_UNKNOWN_CHECK_STATUS, IDS_FAILED_DOUPDATE, IDS_SUCCESS, IDS_SUCCESS_REBOOT,
        IDS_UNKNOWN_UPDATE_RESULT
    };
    for( int i = 0; i < sizeof(msgs)/sizeof(UINT); ++i )
    {
        WCHAR msg[ MSG_SIZE ];
        LocLoadString( msgs[i], msg, MSG_SIZE );
        MessageBox( NULL, msg, g_appName, MB_OK );
    }
    return 0;
#endif

    if( !IsUserAnAdmin() )
    {
        // Manifest will request Administrator rights, but we could be on a Windows XP LUA account
        // or Windows Vista/Windows 7 with UAC disabled so we double-check

        DEBUG_MSG( L"D3D11Install: ERROR - requires admin rights\n" )

        if ( !settings.bQuiet && !settings.bPassive )
        {
            WCHAR msg[ MSG_SIZE ];
            LocLoadString( IDS_REQUIRE_ADMIN, msg, MSG_SIZE );
            MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
        }
        return 2;
    }

    // Check for Direct3D 11 Status

    UINT checkStatus;
    HRESULT hr = CheckDirect3D11Status( &checkStatus );

    if ( FAILED(hr ) )
    {
        DEBUG_MSG( L"D3D11Install: ERROR - CheckDirect3D11Status failed\n" )

        if ( !settings.bQuiet && !settings.bPassive )
        {
            WCHAR msg[ MSG_SIZE ];
            WCHAR fmt[ MSG_SIZE ];
            LocLoadString( IDS_FAILED_CHECKSTAT, fmt, MSG_SIZE );
            swprintf_s( msg, MSG_SIZE, fmt, hr );
            MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
        }
        return 2;
    }

    switch ( checkStatus )
    {
    case D3D11IH_STATUS_INSTALLED:
        DEBUG_MSG( L"D3D11Install: CheckDirect3D11Status returned D3D11IH_STATUS_INSTALLED\n" )

        if ( !settings.bQuiet && !settings.bPassive && !settings.bMinimal )
        {
            WCHAR msg[ MSG_SIZE ];
            LocLoadString( IDS_ALREADY_PRESENT, msg, MSG_SIZE );
            MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONINFORMATION );
        }
        return 0;

    case D3D11IH_STATUS_NOT_SUPPORTED:
        DEBUG_MSG( L"D3D11Install: CheckDirect3D11Status returned D3D11IH_STATUS_NOT_SUPPORTED\n" )

        if ( !settings.bQuiet && !settings.bPassive && !settings.bMinimal )
        {
            WCHAR msg[ MSG_SIZE ];
            LocLoadString( IDS_NOT_SUPPORTED, msg, MSG_SIZE );
            MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONINFORMATION );
        }
        return 0;

    case D3D11IH_STATUS_REQUIRES_UPDATE:
        DEBUG_MSG( L"D3D11Install: CheckDirect3D11Status returned D3D11IH_STATUS_REQUIRES_UPDATE\n" )

        if ( !settings.bQuiet && !settings.bPassive && !settings.bYes )
        {
            if ( LocDialogBox( IDD_UPDATEDLG, InfoDialogProc ) == IDNO )
                return 0;
        }

        // Fall through to update code below...
        break;

    case D3D11IH_STATUS_NEED_LATEST_SP:
        DEBUG_MSG( L"D3D11Install: CheckDirect3D11Status returned D3D11IH_STATUS_NEED_LATEST_SP\n" )\

        if ( !settings.bQuiet && !settings.bPassive )
        {
            LocDialogBox( IDD_SPDLG, InfoDialogProc );
            return 0;
        } 
        else
        {
            return 2;
        }

    default:
        DEBUG_MSG( L"D3D11Install: ERROR - CheckDirect3D11Status returned unknown status\n" )

        if ( !settings.bQuiet && !settings.bPassive )
        {
            WCHAR msg[ MSG_SIZE ];
            WCHAR fmt[ MSG_SIZE ];
            LocLoadString( IDS_UNKNOWN_CHECK_STATUS, fmt, MSG_SIZE );
            swprintf_s( msg, MSG_SIZE, fmt, checkStatus );
            MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
        }
        return 2;
    }

    // Update for Direct3D 11 Support

    for(;;)
    {
        // Setup progress dialog thread
        PROGRESS prg;
        memset( &prg, 0, sizeof(prg) );

        if ( !settings.bQuiet )
        {
            InitializeCriticalSection( &prg.critSec );
            prg.hEvent1 = CreateEvent( NULL, FALSE, FALSE, NULL );
            prg.hEvent2 = CreateEvent( NULL, FALSE, FALSE, NULL );
            prg.hThread = (HANDLE)_beginthreadex( NULL, 0, ProgressThread, &prg, 0, NULL );
        }
    
        // Perform update
        UINT updateResult;
        
        DWORD dwFlags = 0;

        if ( settings.bQuiet || settings.bPassive || settings.bMinimal )
            dwFlags |= D3D11IH_QUIET;

        if ( settings.bWU )
            dwFlags |= D3D11IH_WINDOWS_UPDATE;

        hr = DoUpdateForDirect3D11( dwFlags,
                                    (settings.bQuiet) ? NULL : ProgressCallback,
                                    &prg, &updateResult );

        // Cleanup progress dialog thread and window
        if ( !settings.bQuiet )
        {
            WaitForSingleObject( prg.hThread, INFINITE );
            CloseHandle( prg.hThread );
            CloseHandle( prg.hEvent1 );
            CloseHandle( prg.hEvent2 );

            if ( prg.hWnd )
                DestroyWindow( prg.hWnd );

            DeleteCriticalSection( &prg.critSec );
        }

        if ( FAILED( hr ) )
        {
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 failed\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                WCHAR msg[ MSG_SIZE ];
                WCHAR fmt[ MSG_SIZE ];
                LocLoadString( IDS_FAILED_DOUPDATE, fmt, MSG_SIZE );
                swprintf_s( msg, MSG_SIZE, fmt, hr );
                MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
            }
            return 2;
        }
    
        switch( updateResult )
        {
        case D3D11IH_RESULT_SUCCESS:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_SUCCESS\n" )

            if ( !settings.bQuiet && !settings.bPassive && !settings.bMinimal )
            {
                WCHAR msg[ MSG_SIZE ];
                LocLoadString( IDS_SUCCESS, msg, MSG_SIZE );
                MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONINFORMATION );
            }
            return 0;
    
        case D3D11IH_RESULT_SUCCESS_REBOOT:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_SUCCESS_REBOOT\n" )

            if ( !settings.bQuiet && !settings.bPassive && !settings.bMinimal )
            {
                WCHAR msg[ MSG_SIZE ];
                LocLoadString( IDS_SUCCESS_REBOOT, msg, MSG_SIZE );
                MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONINFORMATION );
            }
            // Return require reboot
            return 1;
    
        case D3D11IH_RESULT_NOT_SUPPORTED:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_NOT_SUPPORTED\n" )

            if ( !settings.bQuiet && !settings.bPassive && !settings.bMinimal )
            {
                // Should have already caught this case above
                WCHAR msg[ MSG_SIZE ];
                LocLoadString( IDS_NOT_SUPPORTED, msg, MSG_SIZE );
                MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONINFORMATION );
            }
            return 0;
    
        case D3D11IH_RESULT_UPDATE_NOT_FOUND:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_UPDATE_NOT_FOUND\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                LocDialogBox( IDD_NOTFOUNDDLG, InfoDialogProc );
            }
            return 2;
    
        case D3D11IH_RESULT_UPDATE_DOWNLOAD_FAILED:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_UPDATE_DOWNLOAD_FAILED\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                LocDialogBox( IDD_DLFAILDLG, InfoDialogProc );
            }
            return 2;
    
        case D3D11IH_RESULT_UPDATE_INSTALL_FAILED:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_UPDATE_INSTALL_FAILED\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                LocDialogBox( IDD_INSTALLFAILDLG, InfoDialogProc );
            }
            return 2;
    
        case D3D11IH_RESULT_WU_SERVICE_ERROR:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned D3D11IH_RESULT_WU_SERVICE_ERROR\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                if ( LocDialogBox( IDD_WUSRVERROR, InfoDialogProc ) == IDCANCEL )
                    return 2;

                // Fallthrough will loop again
            }
            else
            {
                // For silent mode, we just quit...
                return 2;
            }
            break;
    
        default:
            DEBUG_MSG( L"D3D11Install: ERROR - DoUpdateForDirect3D11 returned unknown result\n" )

            if ( !settings.bQuiet && !settings.bPassive )
            {
                WCHAR msg[ MSG_SIZE ];
                WCHAR fmt[ MSG_SIZE ];
                LocLoadString( IDS_UNKNOWN_UPDATE_RESULT, fmt, MSG_SIZE );
                swprintf_s( msg, MSG_SIZE, fmt, updateResult );
                MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
            }
            return 2;
        }
    }
}

//--------------------------------------------------------------------------------------
static BOOL CALLBACK LanguagesCallback( HMODULE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, LONG_PTR lParam )
{
    WCHAR* str = reinterpret_cast<WCHAR*>( lParam );

    wcscat_s( str, MSG_SIZE, L"\t" );

    WCHAR text[ 16 ];
    _itow_s( wIDLanguage, text, 16, 10 );
    wcscat_s( str, MSG_SIZE, text );

    wcscat_s( str, MSG_SIZE, L"\n" );

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.
//--------------------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings )
{
    WCHAR* strCmdLine;

    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( GetCommandLine(), &nNumArgs );
    for( int iArg = 1; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"quiet" ) )
            {
                pSettings->bQuiet = true;
                continue;
            }
            if( IsNextArg( strCmdLine, L"passive" ) )
            {
                pSettings->bPassive = true;
                continue;
            }
            if( IsNextArg( strCmdLine, L"minimal" ) )
            {
                pSettings->bMinimal = true;
                continue;
            }
            if( IsNextArg( strCmdLine, L"y" ) )
            {
                pSettings->bYes = true;
                continue;
            }
            if( IsNextArg( strCmdLine, L"wu" ) )
            {
                pSettings->bWU = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"langid" ) )
            {
                if( iArg + 1 < nNumArgs )
                {
                    WCHAR *strArg = pstrArgList[++iArg];
                    WORD langId = (WORD)_wtoi( strArg );

                    if ( FindResourceEx( g_hInstance, RT_DIALOG, MAKEINTRESOURCE( IDD_UPDATEDLG ), langId ) )
                    {
                       g_langId = langId;

                       // Reload our application name
                       LocLoadString( IDS_APPNAME, g_appName, 64 );
                    }
                    else if ( !pSettings->bQuiet && !pSettings->bPassive && !pSettings->bMinimal )
                    {
                        WCHAR msg[ MSG_SIZE ];
                        wcscpy_s( msg, MSG_SIZE,
                                  L"Unsupported language identifier, using default.\n\n"
                                  L"Supported languages codes:\n");

                        EnumResourceLanguages( g_hInstance, RT_DIALOG, MAKEINTRESOURCE( IDD_UPDATEDLG ), LanguagesCallback, (LONG_PTR)&msg[0] );

                        MessageBox( NULL, msg, g_appName, MB_OK | MB_ICONERROR );
                    }
                }

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
            // Handle non-flag args
            continue;
        }
    }

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
                L"D3D11Install - a command line installation helper for deploying Direct3D 11.\n"
                L"\n"
                L"Usage: D3D11Install.exe [options]\n"
                L"\n"
                L"where:\n"
                L"\n"
                L"  [/quiet]\t\tNo prompts, progress display, or error messages.\n"
                L"  [/passive]\tNo prompts or error messages, but shows progress display.\n"
                L"  [/minimal]\tShows only minimal prompts.\n"
                L"  [/y]\t\tSuppresses prompting to confirm applying the update.\n"
                L"  [/wu]\t\tForces use of the Microsoft Windows Update server rather than the default.\n"
                L"  [/langid <x>]\tForces messages to use language ID given (in decimal).\n"
                , g_appName, MB_OK );
}


//--------------------------------------------------------------------------------------
unsigned int __stdcall ProgressThread( void* pArg )
{
    PROGRESS* prg = (PROGRESS*)pArg;

    EnterCriticalSection( &prg->critSec );

    prg->hWnd = LocCreateDialog( IDD_PROGRESS, ProgressDialogProc );

    ShowWindow( prg->hWnd, SW_SHOW );

    prg->hProgress = GetDlgItem( prg->hWnd, IDC_PROGRESSBAR );
    prg->hStatus = GetDlgItem( prg->hWnd, IDC_STATUS );

    SendMessage( prg->hProgress, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );

    LeaveCriticalSection( &prg->critSec );

    for(;;)
    {
        HANDLE obj[2] = { prg->hEvent1, prg->hEvent2 };

        DWORD wait = WaitForMultipleObjects( 2, obj, FALSE, 100 );
       
        if ( wait == WAIT_OBJECT_0 )
        {
            // Event 1 means we just began a new phase
            switch ( prg->phase )
            {
            case D3D11IH_PROGRESS_BEGIN:
                break;

            case D3D11IH_PROGRESS_SEARCHING:
                {
                    WCHAR msg[ MSG_SIZE ];
                    LocLoadString( IDS_SEARCHING, msg, MSG_SIZE );
                    SetWindowText( prg->hStatus, msg );
        
                    // Since we know this update code will only be run on Windows Vista
                    // we know that the OS has support for the MARQUEE Progress Bar style
                    SetWindowLong( prg->hProgress, GWL_STYLE, GetWindowLong( prg->hProgress, GWL_STYLE ) | PBS_MARQUEE );

                    SendMessage( prg->hProgress, PBM_SETMARQUEE, (WPARAM)TRUE, 0 );
                    InvalidateRect( prg->hWnd, NULL, FALSE );
                }
                break;

            case D3D11IH_PROGRESS_DOWNLOADING:
                {
                    WCHAR msg[ MSG_SIZE ];
                    LocLoadString( IDS_DOWNLOADING, msg, MSG_SIZE );
                    SetWindowText( prg->hStatus, msg );

                    SetWindowLong( prg->hProgress, GWL_STYLE, GetWindowLong( prg->hProgress, GWL_STYLE ) & ~PBS_MARQUEE );

                    SendMessage( prg->hProgress, PBM_SETPOS, 0, 0 );
                    InvalidateRect( prg->hWnd, NULL, FALSE );
                }
                break;

            case D3D11IH_PROGRESS_INSTALLING:
                {
                    WCHAR msg[ MSG_SIZE ];
                    LocLoadString( IDS_INSTALLING, msg, MSG_SIZE );
                    SetWindowText( prg->hStatus, msg );

                    SetWindowLong( prg->hProgress, GWL_STYLE, GetWindowLong( prg->hProgress, GWL_STYLE ) & ~PBS_MARQUEE );

                    SendMessage( prg->hProgress, PBM_SETPOS, 0, 0 );
                    InvalidateRect( prg->hWnd, NULL, FALSE );
                }
                break;

            case D3D11IH_PROGRESS_END:
                return 0;
            }
        }
        else if ( wait == WAIT_OBJECT_0+1 )
        {
            // Event 2 means we are progressing the current phase
            switch ( prg->phase )
            {
            case D3D11IH_PROGRESS_DOWNLOADING:
            case D3D11IH_PROGRESS_INSTALLING:
                SendMessage( prg->hProgress, PBM_SETPOS, prg->progress, 0 );
                InvalidateRect( prg->hWnd, NULL, FALSE );
                break;
            }
        }
        else
        {
            // Message pump to keep the progress dialog responsive
            MSG msg;
            while (PeekMessage( &msg, prg->hWnd, 0, 0, PM_REMOVE) )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }
        }
    }
}

//--------------------------------------------------------------------------------------
void ProgressCallback( UINT phase, UINT progress, void *pContext )
{
    if ( !pContext )
        return;

    PROGRESS *prg = (PROGRESS*)pContext;
    EnterCriticalSection( &prg->critSec );

    switch( phase )
    {
    case D3D11IH_PROGRESS_BEGIN:
    case D3D11IH_PROGRESS_END:
        prg->phase = phase;
        SetEvent( prg->hEvent1 );
        break;

    case D3D11IH_PROGRESS_SEARCHING:
        if ( prg->phase != D3D11IH_PROGRESS_SEARCHING )
        {
            prg->phase = D3D11IH_PROGRESS_SEARCHING;
            prg->progress = 0;
            SetEvent( prg->hEvent1 );
        }
        else
        {
            prg->progress += 1;
            if ( prg->progress > 100 )
                prg->progress = 0;

            SetEvent( prg->hEvent2 );
        }
        break;

    case D3D11IH_PROGRESS_DOWNLOADING:
    case D3D11IH_PROGRESS_INSTALLING:
        if ( prg->phase != phase )
        {
            prg->phase = phase;
            prg->progress = 0;
            SetEvent( prg->hEvent1 );
        }
        else
        {
            prg->progress = progress;
            SetEvent( prg->hEvent2 );
        }
        break;
    }

    LeaveCriticalSection( &prg->critSec );
}


//--------------------------------------------------------------------------------------
static INT_PTR CALLBACK InfoDialogProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
    case WM_INITDIALOG:
        {
            RECT rect;
            GetWindowRect( GetDesktopWindow(), &rect );

            RECT drect;
            GetWindowRect( hwnd, &drect );

            SetWindowPos( hwnd, NULL,
                          (rect.left + rect.right) / 2 - ( drect.right - drect.left) / 2,
                          (rect.top + rect.bottom) / 2 - ( drect.bottom - drect.top) / 2,
                          0, 0, SWP_NOSIZE );

            SetWindowText( hwnd, g_appName );
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDYES:
        case IDNO:
        case IDCANCEL:
        case IDRETRY:
            EndDialog( hwnd, wParam );
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        switch (((LPNMHDR)lParam)->code)
        {
        case NM_CLICK:
        case NM_RETURN:
            {
                PNMLINK pNMLink = (PNMLINK)lParam;
                LITEM item = pNMLink->item;
                HWND hLink1 = GetDlgItem( hwnd, IDC_SYSLINK1 );
                HWND hLink2 = GetDlgItem( hwnd, IDC_SYSLINK2 );
                HWND hFrom = ((LPNMHDR)lParam)->hwndFrom;
                if  ( hFrom == hLink1 || hFrom == hLink2 )
                {
                    ShellExecute(NULL, L"open", item.szUrl, NULL, NULL, SW_SHOW);
                }
                break;
            }
        }
        return TRUE;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
static INT_PTR CALLBACK ProgressDialogProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
    case WM_INITDIALOG:
        {
            RECT rect;
            GetWindowRect( GetDesktopWindow(), &rect );

            RECT drect;
            GetWindowRect( hwnd, &drect );

            SetWindowPos( hwnd, NULL,
                          (rect.left + rect.right) / 2 - ( drect.right - drect.left) / 2,
                          (rect.top + rect.bottom) / 2 - ( drect.bottom - drect.top) / 2,
                          0, 0, SWP_NOSIZE );

            SetWindowText( hwnd, g_appName );
        }
        return TRUE;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
static void LocLoadString( UINT uId, LPWSTR lpBuffer, size_t nBufferMax )
{
    if ( !lpBuffer || !nBufferMax )
        return;

    *lpBuffer = 0;

    // See http://blogs.msdn.com/oldnewthing/archive/2004/01/30/65013.aspx

    HRSRC hrsrc = FindResourceEx( g_hInstance, RT_STRING, MAKEINTRESOURCE(uId / 16 + 1), g_langId );
    if (!hrsrc)
        return;

    HGLOBAL hglob = LoadResource( g_hInstance, hrsrc );
    if (!hglob)
        return;

    LPCWSTR pwsz = reinterpret_cast<LPCWSTR>( LockResource( hglob ) );
    if (!pwsz)
        return;

    for (UINT i = 0; i < (uId & 15); ++i)
        pwsz += 1 + (UINT)*pwsz;

    size_t len = (UINT)*pwsz;
    wcsncpy_s( lpBuffer, nBufferMax, pwsz+1, len );

    if ( len < nBufferMax )
        lpBuffer[ len ] = 0;
}


//--------------------------------------------------------------------------------------
static INT_PTR LocDialogBox( UINT uId, DLGPROC lpDialogFunc )
{
    HRSRC hrsrc = FindResourceEx( g_hInstance, RT_DIALOG, MAKEINTRESOURCE( uId ), g_langId );
    if ( !hrsrc )
        return 1;

    HGLOBAL hglob = LoadResource( g_hInstance, hrsrc );
    if (!hglob)
        return 1;

    LPCDLGTEMPLATE pdlgt = reinterpret_cast<LPCDLGTEMPLATE>( LockResource( hglob ) );
    if (!pdlgt)
        return 1;

    return DialogBoxIndirect( g_hInstance, pdlgt, NULL, lpDialogFunc );
}


//--------------------------------------------------------------------------------------
static HWND LocCreateDialog( UINT uId, DLGPROC lpDialogFunc )
{
    HRSRC hrsrc = FindResourceEx( g_hInstance, RT_DIALOG, MAKEINTRESOURCE( uId ), g_langId );
    if ( !hrsrc )
        return NULL;

    HGLOBAL hglob = LoadResource( g_hInstance, hrsrc );
    if (!hglob)
        return NULL;

    LPCDLGTEMPLATE pdlgt = reinterpret_cast<LPCDLGTEMPLATE>( LockResource( hglob ) );
    if (!pdlgt)
        return NULL;

    return CreateDialogIndirect( g_hInstance, pdlgt, NULL, lpDialogFunc );
}
