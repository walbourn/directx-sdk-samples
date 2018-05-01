//-----------------------------------------------------------------------------
// File: FFConst.cpp
//
// Desc: Demonstrates an application which sets a force feedback constant force 
//       determined by the user.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <commctrl.h>
#include <basetsd.h>
#include <mmsystem.h>

#pragma warning(push)
#pragma warning(disable:6000 28251)
#include <dinput.h>
#pragma warning(pop)

#include <math.h>
#include "resource.h"
#if defined(DEBUG) | defined(_DEBUG)
#include <crtdbg.h>
#endif





//-----------------------------------------------------------------------------
// Function prototypes 
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK EnumFFDevicesCallback( const DIDEVICEINSTANCE* pInst, VOID* pContext );
BOOL CALLBACK EnumAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext );
HRESULT InitDirectInput( HWND hDlg );
VOID FreeDirectInput();
VOID OnPaint( HWND hDlg );
HRESULT OnMouseMove( HWND hDlg, INT x, INT y, UINT keyFlags );
VOID OnLeftButtonDown( HWND hDlg, INT x, INT y, UINT keyFlags );
VOID OnLeftButtonUp( HWND hDlg, INT x, INT y, UINT keyFlags );
INT CoordToForce( INT x );
HRESULT SetDeviceForcesXY();




//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }

#define FEEDBACK_WINDOW_X       20
#define FEEDBACK_WINDOW_Y       60
#define FEEDBACK_WINDOW_WIDTH   200

LPDIRECTINPUT8          g_pDI = nullptr;
LPDIRECTINPUTDEVICE8    g_pDevice = nullptr;
LPDIRECTINPUTEFFECT     g_pEffect = nullptr;
BOOL                    g_bActive = TRUE;
DWORD                   g_dwNumForceFeedbackAxis = 0;
INT                     g_nXForce;
INT                     g_nYForce;
DWORD                   g_dwLastEffectSet; // Time of the previous force feedback effect set




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( _In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ INT )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    InitCommonControls();

    // Display the main dialog box.
    DialogBox( hInst, MAKEINTRESOURCE( IDD_FORCE_FEEDBACK ), nullptr, MainDlgProc );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MainDlgProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
            if( FAILED( InitDirectInput( hDlg ) ) )
            {
                MessageBox( nullptr, _T( "Error Initializing DirectInput " )
                                  _T( "The sample will now exit." ),
                                  _T( "FFConst" ), MB_ICONERROR | MB_OK );
                EndDialog( hDlg, 0 );
            }

            // Init the time of the last force feedback effect
            g_dwLastEffectSet = timeGetTime();
            break;

        case WM_MOUSEMOVE:
            if( FAILED( OnMouseMove( hDlg, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), ( UINT )wParam ) ) )
            {
                MessageBox( nullptr, _T( "Error setting effect parameters. " )
                                  _T( "The sample will now exit." ),
                                  _T( "FFConst" ), MB_ICONERROR | MB_OK );
                EndDialog( hDlg, 0 );
            }
            break;

        case WM_LBUTTONDOWN:
            OnLeftButtonDown( hDlg, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), ( UINT )wParam );
            break;

        case WM_LBUTTONUP:
            OnLeftButtonUp( hDlg, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), ( UINT )wParam );
            break;

        case WM_PAINT:
            OnPaint( hDlg );
            break;

        case WM_ACTIVATE:
            if( WA_INACTIVE != wParam && g_pDevice )
            {
                // Make sure the device is acquired, if we are gaining focus.
                g_pDevice->Acquire();

                if( g_pEffect )
                    g_pEffect->Start( 1, 0 ); // Start the effect
            }
            break;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDCANCEL:
                    EndDialog( hDlg, 0 );
                    break;

                default:
                    return FALSE; // Message not handled 
            }
            break;

        case WM_DESTROY:
            // Cleanup everything
            KillTimer( hDlg, 0 );
            FreeDirectInput();
            break;

        default:
            return FALSE; // Message not handled 
    }

    return TRUE; // Message handled 
}




//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput( HWND hDlg )
{
    DIPROPDWORD dipdw;
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer
    // to a IDirectInput interface we can use.
    if( FAILED( hr = DirectInput8Create( GetModuleHandle( nullptr ), DIRECTINPUT_VERSION,
                                         IID_IDirectInput8, ( VOID** )&g_pDI, nullptr ) ) )
    {
        return hr;
    }

    // Look for a force feedback device we can use
    if( FAILED( hr = g_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL,
                                         EnumFFDevicesCallback, nullptr,
                                         DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK ) ) )
    {
        return hr;
    }

    if( !g_pDevice )
    {
        MessageBox( nullptr, _T( "Force feedback device not found. " )
                          _T( "The sample will now exit." ),
                          _T( "FFConst" ), MB_ICONERROR | MB_OK );
        EndDialog( hDlg, 0 );
        return S_OK;
    }

    // Set the data format to "simple joystick" - a predefined data format. A
    // data format specifies which controls on a device we are interested in,
    // and how they should be reported.
    //
    // This tells DirectInput that we will be passing a DIJOYSTATE structure to
    // IDirectInputDevice8::GetDeviceState(). Even though we won't actually do
    // it in this sample. But setting the data format is important so that the
    // DIJOFS_* values work properly.
    if( FAILED( hr = g_pDevice->SetDataFormat( &c_dfDIJoystick ) ) )
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    // Exclusive access is required in order to perform force feedback.
    if( FAILED( hr = g_pDevice->SetCooperativeLevel( hDlg,
                                                     DISCL_EXCLUSIVE |
                                                     DISCL_FOREGROUND ) ) )
    {
        return hr;
    }

    // Since we will be playing force feedback effects, we should disable the
    // auto-centering spring.
    dipdw.diph.dwSize = sizeof( DIPROPDWORD );
    dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = FALSE;

    if( FAILED( hr = g_pDevice->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph ) ) )
        return hr;

    // Enumerate and count the axes of the joystick 
    if( FAILED( hr = g_pDevice->EnumObjects( EnumAxesCallback,
                                             ( VOID* )&g_dwNumForceFeedbackAxis, DIDFT_AXIS ) ) )
        return hr;

    // This simple sample only supports one or two axis joysticks
    if( g_dwNumForceFeedbackAxis > 2 )
        g_dwNumForceFeedbackAxis = 2;

    // This application needs only one effect: Applying raw forces.
    DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
    LONG rglDirection[2] = { 0, 0 };
    DICONSTANTFORCE cf = { 0 };

    DIEFFECT eff = {};
    eff.dwSize = sizeof( DIEFFECT );
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwSamplePeriod = 0;
    eff.dwGain = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.dwTriggerRepeatInterval = 0;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rgdwAxes = rgdwAxes;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Create the prepared effect
    if( FAILED( hr = g_pDevice->CreateEffect( GUID_ConstantForce,
                                              &eff, &g_pEffect, nullptr ) ) )
    {
        return hr;
    }

    if( !g_pEffect )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EnumAxesCallback()
// Desc: Callback function for enumerating the axes on a joystick and counting
//       each force feedback enabled axis
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                VOID* pContext )
{
    auto pdwNumForceFeedbackAxis = reinterpret_cast<DWORD*>( pContext );

    if( ( pdidoi->dwFlags & DIDOI_FFACTUATOR ) != 0 )
        ( *pdwNumForceFeedbackAxis )++;

    return DIENUM_CONTINUE;
}




//-----------------------------------------------------------------------------
// Name: EnumFFDevicesCallback()
// Desc: Called once for each enumerated force feedback device. If we find
//       one, create a device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumFFDevicesCallback( const DIDEVICEINSTANCE* pInst,
                                     VOID* pContext )
{
    LPDIRECTINPUTDEVICE8 pDevice;
    HRESULT hr;

    // Obtain an interface to the enumerated force feedback device.
    hr = g_pDI->CreateDevice( pInst->guidInstance, &pDevice, nullptr );

    // If it failed, then we can't use this device for some
    // bizarre reason.  (Maybe the user unplugged it while we
    // were in the middle of enumerating it.)  So continue enumerating
    if( FAILED( hr ) )
        return DIENUM_CONTINUE;

    // We successfully created an IDirectInputDevice8.  So stop looking 
    // for another one.
    g_pDevice = pDevice;

    return DIENUM_STOP;
}




//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if( g_pDevice )
        g_pDevice->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pDevice );
    SAFE_RELEASE( g_pDI );
}




//-----------------------------------------------------------------------------
// Name: OnPaint()
// Desc: Handles the WM_PAINT window message
//-----------------------------------------------------------------------------
VOID OnPaint( HWND hDlg )
{
    PAINTSTRUCT ps;
    HDC hDC;
    HPEN hpenOld;
    HPEN hpenBlack;
    HBRUSH hbrOld;
    HBRUSH hbrBlack;
    INT x;
    INT y;

    hDC = BeginPaint( hDlg, &ps );
    if( !hDC )
        return;

    // Everything is scaled to the size of the window.
    hpenBlack = GetStockPen( BLACK_PEN );
    hpenOld = SelectPen( hDC, hpenBlack );

    // Draw force feedback bounding rect
    MoveToEx( hDC, FEEDBACK_WINDOW_X, FEEDBACK_WINDOW_Y, nullptr );

    LineTo( hDC, FEEDBACK_WINDOW_X,
            FEEDBACK_WINDOW_Y + FEEDBACK_WINDOW_WIDTH );
    LineTo( hDC, FEEDBACK_WINDOW_X + FEEDBACK_WINDOW_WIDTH,
            FEEDBACK_WINDOW_Y + FEEDBACK_WINDOW_WIDTH );
    LineTo( hDC, FEEDBACK_WINDOW_X + FEEDBACK_WINDOW_WIDTH,
            FEEDBACK_WINDOW_Y );
    LineTo( hDC, FEEDBACK_WINDOW_X,
            FEEDBACK_WINDOW_Y );

    // Calculate center of feedback window for center marker
    x = FEEDBACK_WINDOW_X + FEEDBACK_WINDOW_WIDTH / 2;
    y = FEEDBACK_WINDOW_Y + FEEDBACK_WINDOW_WIDTH / 2;

    // Draw center marker
    MoveToEx( hDC, x, y - 10, nullptr );
    LineTo( hDC, x, y + 10 + 1 );
    MoveToEx( hDC, x - 10, y, nullptr );
    LineTo( hDC, x + 10 + 1, y );

    hbrBlack = GetStockBrush( BLACK_BRUSH );
    hbrOld = SelectBrush( hDC, hbrBlack );

    x = MulDiv( FEEDBACK_WINDOW_WIDTH,
                g_nXForce + DI_FFNOMINALMAX,
                2 * DI_FFNOMINALMAX );

    y = MulDiv( FEEDBACK_WINDOW_WIDTH,
                g_nYForce + DI_FFNOMINALMAX,
                2 * DI_FFNOMINALMAX );

    x += FEEDBACK_WINDOW_X;
    y += FEEDBACK_WINDOW_Y;

    Ellipse( hDC, x - 5, y - 5, x + 6, y + 6 );

    SelectBrush( hDC, hbrOld );
    SelectPen( hDC, hpenOld );

    EndPaint( hDlg, &ps );
}




//-----------------------------------------------------------------------------
// Name: OnMouseMove()
// Desc: If the mouse button is down, then change the direction of
//       the force to match the new location.
//-----------------------------------------------------------------------------
HRESULT OnMouseMove( HWND hDlg, INT x, INT y, UINT keyFlags )
{
    HRESULT hr;
    DWORD dwCurrentTime;

    if( !g_pEffect )
        return S_OK;

    if( keyFlags & MK_LBUTTON )
    {
        dwCurrentTime = timeGetTime();

        if( dwCurrentTime - g_dwLastEffectSet < 100 )
        {
            // Don't allow setting effect more often than
            // 100ms since every time an effect is set, the
            // device will jerk.
            //
            // Note: This is not neccessary, and is specific to this sample
            return S_OK;
        }

        g_dwLastEffectSet = dwCurrentTime;

        x -= FEEDBACK_WINDOW_X;
        y -= FEEDBACK_WINDOW_Y;

        g_nXForce = CoordToForce( x );
        g_nYForce = CoordToForce( y );

        InvalidateRect( hDlg, 0, TRUE );
        UpdateWindow( hDlg );

        if( FAILED( hr = SetDeviceForcesXY() ) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLeftButtonDown()
// Desc: Capture the mouse so we can follow it, and start updating the
//       force information.
//-----------------------------------------------------------------------------
VOID OnLeftButtonDown( HWND hDlg, INT x, INT y, UINT keyFlags )
{
    SetCapture( hDlg );
    OnMouseMove( hDlg, x, y, MK_LBUTTON );
}




//-----------------------------------------------------------------------------
// Name: OnLeftButtonUp()
// Desc: Stop capturing the mouse when the button goes up.
//-----------------------------------------------------------------------------
VOID OnLeftButtonUp( HWND hDlg, INT x, INT y, UINT keyFlags )
{
    ReleaseCapture();
}




//-----------------------------------------------------------------------------
// Name: CoordToForce()
// Desc: Convert a coordinate 0 <= nCoord <= FEEDBACK_WINDOW_WIDTH 
//       to a force value in the range -DI_FFNOMINALMAX to +DI_FFNOMINALMAX.
//-----------------------------------------------------------------------------
INT CoordToForce( INT nCoord )
{
    INT nForce = MulDiv( nCoord, 2 * DI_FFNOMINALMAX, FEEDBACK_WINDOW_WIDTH )
        - DI_FFNOMINALMAX;

    // Keep force within bounds
    if( nForce < -DI_FFNOMINALMAX )
        nForce = -DI_FFNOMINALMAX;

    if( nForce > +DI_FFNOMINALMAX )
        nForce = +DI_FFNOMINALMAX;

    return nForce;
}




//-----------------------------------------------------------------------------
// Name: SetDeviceForcesXY()
// Desc: Apply the X and Y forces to the effect we prepared.
//-----------------------------------------------------------------------------
HRESULT SetDeviceForcesXY()
{
    // Modifying an effect is basically the same as creating a new one, except
    // you need only specify the parameters you are modifying
    LONG rglDirection[2] = { 0, 0 };

    DICONSTANTFORCE cf;

    if( g_dwNumForceFeedbackAxis == 1 )
    {
        // If only one force feedback axis, then apply only one direction and 
        // keep the direction at zero
        cf.lMagnitude = g_nXForce;
        rglDirection[0] = 0;
    }
    else
    {
        // If two force feedback axis, then apply magnitude from both directions 
        rglDirection[0] = g_nXForce;
        rglDirection[1] = g_nYForce;
        cf.lMagnitude = ( DWORD )sqrt( ( double )g_nXForce * ( double )g_nXForce +
                                       ( double )g_nYForce * ( double )g_nYForce );
    }

    DIEFFECT eff = {};
    eff.dwSize = sizeof( DIEFFECT );
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Now set the new parameters and start the effect immediately.
    return g_pEffect->SetParameters( &eff, DIEP_DIRECTION |
                                     DIEP_TYPESPECIFICPARAMS |
                                     DIEP_START );
}



