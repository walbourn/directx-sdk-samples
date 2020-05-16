//-----------------------------------------------------------------------------
// File: Joystick.cpp
//
// Desc: Demonstrates an application which receives immediate 
//       joystick data in exclusive mode via a dialog timer.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800
#define _CRT_SECURE_NO_DEPRECATE
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <commctrl.h>
#include <basetsd.h>

#pragma warning(push)
#pragma warning(disable:6000 28251)
#include <dinput.h>
#pragma warning(pop)

#include <dinputd.h>
#include <assert.h>
#include <oleauto.h>
#include <shellapi.h>
#include "resource.h"




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK    EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext );
BOOL CALLBACK    EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext );
HRESULT InitDirectInput( HWND hDlg );
VOID FreeDirectInput();
HRESULT UpdateInputState( HWND hDlg );
VOID FillJoystickInfo(HWND hDlg);

bool IsXInputDevice( LPDIRECTINPUTDEVICE8 pJoystick );

struct DI_ENUM_CONTEXT
{
    DIJOYCONFIG* pPreferredJoyCfg;
    bool bPreferredJoyCfgValid;
};

bool                    g_bFilterOutXinputDevices = false;




//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }

LPDIRECTINPUT8          g_pDI = nullptr;
LPDIRECTINPUTDEVICE8    g_pJoystick = nullptr;



//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int APIENTRY WinMain( _In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int )
{
    InitCommonControls();

    WCHAR* strCmdLine;
    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW( GetCommandLineW(), &nNumArgs );
    for( int iArg = 1; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            int nArgLen = ( int )wcslen( L"noxinput" );
            if( _wcsnicmp( strCmdLine, L"noxinput", nArgLen ) == 0 && strCmdLine[nArgLen] == 0 )
            {
                g_bFilterOutXinputDevices = true;
                continue;
            }
        }
    }
    LocalFree( pstrArgList );

    // Display the main dialog box.
    DialogBox( hInst, MAKEINTRESOURCE( IDD_JOYST_IMM ), nullptr, MainDlgProc );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MainDialogProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    UNREFERENCED_PARAMETER( lParam );

    switch( msg )
    {
        case WM_INITDIALOG:
            if( FAILED( InitDirectInput( hDlg ) ) )
            {
                MessageBox( nullptr, TEXT( "Error Initializing DirectInput" ),
                            TEXT( "DirectInput Sample" ), MB_ICONERROR | MB_OK );
                EndDialog( hDlg, 0 );
            }

            // Set a timer to go off 30 times a second. At every timer message
            // the input device will be read
            SetTimer( hDlg, 0, 1000 / 30, nullptr );
            return TRUE;

        case WM_ACTIVATE:
            if( WA_INACTIVE != wParam && g_pJoystick )
            {
                // Make sure the device is acquired, if we are gaining focus.
                g_pJoystick->Acquire();
            }
            return TRUE;

        case WM_TIMER:
            // Update the input device every timer message
            if( FAILED( UpdateInputState( hDlg ) ) )
            {
                KillTimer( hDlg, 0 );
                MessageBox( nullptr, TEXT( "Error Reading Input State. " ) \
                            TEXT( "The sample will now exit." ), TEXT( "DirectInput Sample" ),
                            MB_ICONERROR | MB_OK );
                EndDialog( hDlg, TRUE );
            }
            return TRUE;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDCANCEL:
                    EndDialog( hDlg, 0 );
                    return TRUE;
            }

        case WM_DESTROY:
            // Cleanup everything
            KillTimer( hDlg, 0 );
            FreeDirectInput();
            return TRUE;
    }

    return FALSE; // Message not handled 
}




//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput( HWND hDlg )
{
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer
    // to a IDirectInput interface we can use.
    // Create a DInput object
    if( FAILED( hr = DirectInput8Create( GetModuleHandle( nullptr ), DIRECTINPUT_VERSION,
                                         IID_IDirectInput8, ( VOID** )&g_pDI, nullptr ) ) )
        return hr;


    DIJOYCONFIG PreferredJoyCfg = {0};
    DI_ENUM_CONTEXT enumContext;
    enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
    enumContext.bPreferredJoyCfgValid = false;

    IDirectInputJoyConfig8* pJoyConfig = nullptr;
    if( FAILED( hr = g_pDI->QueryInterface( IID_IDirectInputJoyConfig8, ( void** )&pJoyConfig ) ) )
        return hr;

    PreferredJoyCfg.dwSize = sizeof( PreferredJoyCfg );
    if( SUCCEEDED( pJoyConfig->GetConfig( 0, &PreferredJoyCfg, DIJC_GUIDINSTANCE ) ) ) // This function is expected to fail if no joystick is attached
        enumContext.bPreferredJoyCfgValid = true;
    SAFE_RELEASE( pJoyConfig );

    // Look for a simple joystick we can use for this sample program.
    if( FAILED( hr = g_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL,
                                         EnumJoysticksCallback,
                                         &enumContext, DIEDFL_ATTACHEDONLY ) ) )
        return hr;

    // Make sure we got a joystick
    if( !g_pJoystick )
    {
        MessageBox( nullptr, TEXT( "Joystick not found. The sample will now exit." ),
                    TEXT( "DirectInput Sample" ),
                    MB_ICONERROR | MB_OK );
        EndDialog( hDlg, 0 );
        return S_OK;
    }

    // Set the data format to "simple joystick" - a predefined data format 
    //
    // A data format specifies which controls on a device we are interested in,
    // and how they should be reported. This tells DInput that we will be
    // passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
    if( FAILED( hr = g_pJoystick->SetDataFormat( &c_dfDIJoystick2 ) ) )
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    if( FAILED( hr = g_pJoystick->SetCooperativeLevel( hDlg, DISCL_EXCLUSIVE |
                                                       DISCL_FOREGROUND ) ) )
        return hr;

    // Enumerate the joystick objects. The callback function enabled user
    // interface elements for objects that are found, and sets the min/max
    // values property for discovered axes.
    if( FAILED( hr = g_pJoystick->EnumObjects( EnumObjectsCallback,
                                               ( VOID* )hDlg, DIDFT_ALL ) ) )
        return hr;

    FillJoystickInfo( hDlg );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Returns true if the DirectInput device is also an XInput device.
// Checks if device ID contains "IG_" (ex. "\\?\HID#VID_045E&PID_02A1&IG_00").
// If it does, then it's an XInput device.
//-----------------------------------------------------------------------------
bool IsXInputDevice( LPDIRECTINPUTDEVICE8 pJoystick )
{
    DIPROPGUIDANDPATH dip;
    dip.diph.dwSize = sizeof( DIPROPGUIDANDPATH );
    dip.diph.dwHeaderSize = sizeof( DIPROPHEADER );
    dip.diph.dwObj = 0;
    dip.diph.dwHow = DIPH_DEVICE;

    if ( FAILED( pJoystick->GetProperty( DIPROP_GUIDANDPATH, &dip.diph ) ) )
        return false;

    _wcsupr( dip.wszPath );

    return wcsstr( dip.wszPath, L"IG_" );
}

VOID FillJoystickInfo( HWND hDlg )
{
    TCHAR strJoysInfo[512] = { 0 };

    DIDEVICEINSTANCE pdidInstance;
    pdidInstance.dwSize = sizeof(pdidInstance);

    if ( FAILED ( g_pJoystick->GetDeviceInfo( &pdidInstance ) ) )
        return;

    DIPROPDWORD dipdw;
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;

    if ( FAILED( g_pJoystick->GetProperty( DIPROP_VIDPID, &dipdw.diph ) ) )
        return;

    WORD wVendorID = LOWORD( dipdw.dwData );
    WORD wProductID = HIWORD( dipdw.dwData );

    DIPROPGUIDANDPATH dip;
    dip.diph.dwSize = sizeof(DIPROPGUIDANDPATH);
    dip.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dip.diph.dwObj = 0;
    dip.diph.dwHow = DIPH_DEVICE;

    if (FAILED( g_pJoystick->GetProperty( DIPROP_GUIDANDPATH, &dip.diph ) ) )
        return;

    WCHAR* wszPath = dip.wszPath;

    TCHAR strGuidProduct[64] = { 0 };
    TCHAR strGuidInstance[64] = { 0 };
    TCHAR strGuidClass[64] = { 0 };

    StringFromGUID2(pdidInstance.guidProduct, strGuidProduct, 64);
    StringFromGUID2(pdidInstance.guidInstance, strGuidInstance, 64);
    StringFromGUID2(dip.guidClass, strGuidClass, 64);

    _stprintf_s(strJoysInfo, 512,
        L"Product Name: %s\n"
        L"Instance Name: %s\n"
        L"Vendor ID: 0x%04x\n"
        L"Product ID: 0x%04x\n"
        L"Product GUID: %s\n"
        L"Instance GUID: %s\n"
        L"HID Class GUID: %s\n"
        L"HID Usage Page: 0x%04x\n"
        L"HID Usage ID: 0x%04x\n"
        L"HID Path: %s",
        pdidInstance.tszProductName,
        pdidInstance.tszInstanceName,
        wVendorID,
        wProductID,
        strGuidProduct,
        strGuidInstance,
        strGuidClass,
        pdidInstance.wUsagePage,
        pdidInstance.wUsage,
        dip.wszPath);

    EnableWindow(GetDlgItem(hDlg, IDC_INFO), TRUE);
    SetWindowText(GetDlgItem(hDlg, IDC_INFO), strJoysInfo);
}

//-----------------------------------------------------------------------------
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance,
                                     VOID* pContext )
{
    auto pEnumContext = reinterpret_cast<DI_ENUM_CONTEXT*>( pContext );
    HRESULT hr;

    // Skip anything other than the perferred joystick device as defined by the control panel.  
    // Instead you could store all the enumerated joysticks and let the user pick.
    if( pEnumContext->bPreferredJoyCfgValid &&
        !IsEqualGUID( pdidInstance->guidInstance, pEnumContext->pPreferredJoyCfg->guidInstance ) )
        return DIENUM_CONTINUE;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice( pdidInstance->guidInstance, &g_pJoystick, nullptr );

    // If it failed, then we can't use this joystick. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    if( FAILED( hr ) )
        return DIENUM_CONTINUE;

    if (g_bFilterOutXinputDevices && IsXInputDevice( g_pJoystick ))
    {
        SAFE_RELEASE( g_pJoystick );
        return DIENUM_CONTINUE;
    }

    // Stop enumeration. Note: we're just taking the first joystick we get. You
    // could store all the enumerated joysticks and let the user pick.
    return DIENUM_STOP;
}




//-----------------------------------------------------------------------------
// Name: EnumObjectsCallback()
// Desc: Callback function for enumerating objects (axes, buttons, POVs) on a 
//       joystick. This function enables user interface elements for objects
//       that are found to exist, and scales axes min/max values.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                   VOID* pContext )
{
    HWND hDlg = ( HWND )pContext;

    static int nSliderCount = 0;  // Number of returned slider controls
    static int nPOVCount = 0;     // Number of returned POV controls

    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg;
        diprg.diph.dwSize = sizeof( DIPROPRANGE );
        diprg.diph.dwHeaderSize = sizeof( DIPROPHEADER );
        diprg.diph.dwHow = DIPH_BYID;
        diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
        diprg.lMin = -1000;
        diprg.lMax = +1000;

        // Set the range for the axis
        if( FAILED( g_pJoystick->SetProperty( DIPROP_RANGE, &diprg.diph ) ) )
            return DIENUM_STOP;

    }


    // Set the UI to reflect what objects the joystick supports
    if( pdidoi->guidType == GUID_XAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_X_AXIS ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_X_AXIS_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_YAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_Y_AXIS ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_Y_AXIS_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_ZAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_Z_AXIS ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_Z_AXIS_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_RxAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_X_ROT ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_X_ROT_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_RyAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_Y_ROT ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_Y_ROT_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_RzAxis )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_Z_ROT ), TRUE );
        EnableWindow( GetDlgItem( hDlg, IDC_Z_ROT_TEXT ), TRUE );
    }
    if( pdidoi->guidType == GUID_Slider )
    {
        switch( nSliderCount++ )
        {
            case 0 :
                EnableWindow( GetDlgItem( hDlg, IDC_SLIDER0 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_SLIDER0_TEXT ), TRUE );
                break;

            case 1 :
                EnableWindow( GetDlgItem( hDlg, IDC_SLIDER1 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_SLIDER1_TEXT ), TRUE );
                break;
        }
    }
    if( pdidoi->guidType == GUID_POV )
    {
        switch( nPOVCount++ )
        {
            case 0 :
                EnableWindow( GetDlgItem( hDlg, IDC_POV0 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_POV0_TEXT ), TRUE );
                break;

            case 1 :
                EnableWindow( GetDlgItem( hDlg, IDC_POV1 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_POV1_TEXT ), TRUE );
                break;

            case 2 :
                EnableWindow( GetDlgItem( hDlg, IDC_POV2 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_POV2_TEXT ), TRUE );
                break;

            case 3 :
                EnableWindow( GetDlgItem( hDlg, IDC_POV3 ), TRUE );
                EnableWindow( GetDlgItem( hDlg, IDC_POV3_TEXT ), TRUE );
                break;
        }
    }

    return DIENUM_CONTINUE;
}




//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
HRESULT UpdateInputState( HWND hDlg )
{
    HRESULT hr;
    TCHAR strText[512] = {0}; // Device state text
    DIJOYSTATE2 js;           // DInput joystick state 

    if( !g_pJoystick )
        return S_OK;

    // Poll the device to read the current state
    hr = g_pJoystick->Poll();
    if( FAILED( hr ) )
    {
        // DInput is telling us that the input stream has been
        // interrupted. We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done. We
        // just re-acquire and try again.
        hr = g_pJoystick->Acquire();
        while( hr == DIERR_INPUTLOST )
            hr = g_pJoystick->Acquire();

        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
        return S_OK;
    }

    // Get the input's device state
    if( FAILED( hr = g_pJoystick->GetDeviceState( sizeof( DIJOYSTATE2 ), &js ) ) )
        return hr; // The device should have been acquired during the Poll()

    // Display joystick state to dialog

    // Axes
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lX );
    SetWindowText( GetDlgItem( hDlg, IDC_X_AXIS ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lY );
    SetWindowText( GetDlgItem( hDlg, IDC_Y_AXIS ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lZ );
    SetWindowText( GetDlgItem( hDlg, IDC_Z_AXIS ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lRx );
    SetWindowText( GetDlgItem( hDlg, IDC_X_ROT ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lRy );
    SetWindowText( GetDlgItem( hDlg, IDC_Y_ROT ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.lRz );
    SetWindowText( GetDlgItem( hDlg, IDC_Z_ROT ), strText );

    // Slider controls
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.rglSlider[0] );
    SetWindowText( GetDlgItem( hDlg, IDC_SLIDER0 ), strText );
    _stprintf_s( strText, 512, TEXT( "%ld" ), js.rglSlider[1] );
    SetWindowText( GetDlgItem( hDlg, IDC_SLIDER1 ), strText );

    // Points of view
    _stprintf_s( strText, 512, TEXT( "%lu" ), js.rgdwPOV[0] );
    SetWindowText( GetDlgItem( hDlg, IDC_POV0 ), strText );
    _stprintf_s( strText, 512, TEXT( "%lu" ), js.rgdwPOV[1] );
    SetWindowText( GetDlgItem( hDlg, IDC_POV1 ), strText );
    _stprintf_s( strText, 512, TEXT( "%lu" ), js.rgdwPOV[2] );
    SetWindowText( GetDlgItem( hDlg, IDC_POV2 ), strText );
    _stprintf_s( strText, 512, TEXT( "%lu" ), js.rgdwPOV[3] );
    SetWindowText( GetDlgItem( hDlg, IDC_POV3 ), strText );


    // Fill up text with which buttons are pressed
    _tcscpy_s( strText, 512, TEXT( "" ) );
    for( int i = 0; i < 128; i++ )
    {
        if( js.rgbButtons[i] & 0x80 )
        {
            TCHAR sz[128];
            _stprintf_s( sz, 128, TEXT( "%02d " ), i );
            _tcscat_s( strText, 512, sz );
        }
    }

    SetWindowText( GetDlgItem( hDlg, IDC_BUTTONS ), strText );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if( g_pJoystick )
        g_pJoystick->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE( g_pJoystick );
    SAFE_RELEASE( g_pDI );
}



