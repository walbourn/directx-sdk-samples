//-----------------------------------------------------------------------------
// File: RumbleController.cpp
//
// Simple use of XInput rumble force-feedback
//
// Note: This sample works with all versions of XInput (1.4, 1.3, and 9.1.0)
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <commdlg.h>
#include <basetsd.h>
#include <objbase.h>

#ifdef USE_DIRECTX_SDK
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\include\xinput.h>
#pragma comment(lib,"xinput.lib")
#elif (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <XInput.h>
#pragma comment(lib,"xinput.lib")
#else
#include <XInput.h>
#pragma comment(lib,"xinput9_1_0.lib")
#endif

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT UpdateControllerState();
void RenderFrame();


//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define MAX_CONTROLLERS     4


//-----------------------------------------------------------------------------
// Struct to hold xinput state
//-----------------------------------------------------------------------------
struct CONTROLLER_STATE
{
    XINPUT_STATE lastState;
    XINPUT_STATE state;
    DWORD dwResult;
    bool bLockVibration;
    XINPUT_VIBRATION vibration;
};

CONTROLLER_STATE g_Controllers[MAX_CONTROLLERS];
WCHAR g_szMessage[4][1024] = {0};
HWND g_hWnd;


//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int )
{
    // Initialize COM
    HRESULT hr;
    if( FAILED( hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED) ) )
        return 1;

    // Register the window class
    HBRUSH hBrush = CreateSolidBrush( 0xFF0000 );
    WNDCLASSEX wc =
    {
        sizeof( WNDCLASSEX ), 0, MsgProc, 0L, 0L, hInstance, nullptr,
        LoadCursor( nullptr, IDC_ARROW ), hBrush,
        nullptr, L"XInputSample", nullptr
    };
    RegisterClassEx( &wc );

    // Create the application's window
    g_hWnd = CreateWindow( L"XInputSample", L"XInput Sample: RumbleController",
                           WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
                           nullptr, nullptr, hInstance, nullptr );

    // Init state
    ZeroMemory( g_Controllers, sizeof( CONTROLLER_STATE ) * MAX_CONTROLLERS );

    // Enter the message loop
    bool bGotMsg;
    MSG msg;
    msg.message = WM_NULL;

    while( WM_QUIT != msg.message )
    {
        // Use PeekMessage() so we can use idle time to render the scene and call pEngine->DoWork()
        bGotMsg = ( PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) != 0 );

        if( bGotMsg )
        {
            // Translate and dispatch the message
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            UpdateControllerState();
            RenderFrame();
        }
    }

    // Clean up 
    UnregisterClass( L"XInputSample", nullptr );

    CoUninitialize();

    return 0;
}


//-----------------------------------------------------------------------------
HRESULT UpdateControllerState()
{
    for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        g_Controllers[i].lastState = g_Controllers[i].state;
        g_Controllers[i].dwResult = XInputGetState( i, &g_Controllers[i].state );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
void RenderFrame()
{
    bool bRepaint = false;

    WCHAR sz[4][1024];
    for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
    {
        if( g_Controllers[i].dwResult == ERROR_SUCCESS )
        {
            if( !g_Controllers[i].bLockVibration )
            {
                // Map bLeftTrigger's 0-255 to wLeftMotorSpeed's 0-65535
                if( g_Controllers[i].state.Gamepad.bLeftTrigger > 0 )
                    g_Controllers[i].vibration.wLeftMotorSpeed = ( ( g_Controllers[i].state.Gamepad.bLeftTrigger +
                                                                     1 ) * 256 ) - 1;
                else
                    g_Controllers[i].vibration.wLeftMotorSpeed = 0;

                // Map bRightTrigger's 0-255 to wRightMotorSpeed's 0-65535
                if( g_Controllers[i].state.Gamepad.bRightTrigger > 0 )
                    g_Controllers[i].vibration.wRightMotorSpeed = ( ( g_Controllers[i].state.Gamepad.bRightTrigger +
                                                                      1 ) * 256 ) - 1;
                else
                    g_Controllers[i].vibration.wRightMotorSpeed = 0;
            }

            if( ( g_Controllers[i].state.Gamepad.wButtons ) &&
                ( g_Controllers[i].lastState.Gamepad.wButtons == 0 ) )
            {
                if( !( !g_Controllers[i].bLockVibration && g_Controllers[i].vibration.wRightMotorSpeed == 0 &&
                       g_Controllers[i].vibration.wLeftMotorSpeed == 0 ) )
                    g_Controllers[i].bLockVibration = !g_Controllers[i].bLockVibration;
            }

            XInputSetState( i, &g_Controllers[i].vibration );

            swprintf_s( sz[i], 1024,
                              L"Controller %u: Connected\n"
                              L"  Left Motor Speed: %u\n"
                              L"  Right Motor Speed: %u\n"
                              L"  Rumble Lock: %d\n", i,
                              g_Controllers[i].vibration.wLeftMotorSpeed,
                              g_Controllers[i].vibration.wRightMotorSpeed,
                              g_Controllers[i].bLockVibration );

        }
        else if( g_Controllers[i].dwResult == ERROR_DEVICE_NOT_CONNECTED )
        {
            swprintf_s( sz[i], 1024,
                             L"Controller %u: Not connected", i );
        }
        else
        {
            swprintf_s( sz[i], 1024, L"Controller %u: Generic error", i );
        }

        if( wcscmp( sz[i], g_szMessage[i] ) != 0 )
        {
            wcscpy_s( g_szMessage[i], 1024, sz[i] );
            bRepaint = true;
        }
    }

    if( bRepaint )
    {
        // Repaint the window if needed 
        InvalidateRect( g_hWnd, nullptr, TRUE );
        UpdateWindow( g_hWnd );
    }

    // This sample doesn't use Direct3D.  Instead, it just yields CPU time to other 
    // apps but this is not typically done when rendering
    Sleep( 10 );
}


//-----------------------------------------------------------------------------
// Window message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_ACTIVATEAPP:
        {
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) || defined(USE_DIRECTX_SDK)

            //
            // XInputEnable is implemented by XInput 1.3 and 1.4, but not 9.1.0
            //

            if( wParam == TRUE )
            {
                // App is now active, so re-enable XInput
                XInputEnable( TRUE );
            }
            else
            {
                // App is now inactive, so disable XInput to prevent
                // user input from effecting application and to 
                // disable rumble. 
                XInputEnable( FALSE );
            }

#endif
            break;
        }

        case WM_PAINT:
        {
            // Paint some simple explanation text
            PAINTSTRUCT ps;
            HDC hDC = BeginPaint( hWnd, &ps );
            SetBkColor( hDC, 0xFF0000 );
            SetTextColor( hDC, 0xFFFFFF );
            RECT rect;
            GetClientRect( hWnd, &rect );

            rect.top = 20;
            rect.left = 20;
            DrawText( hDC,
                      L"Use the controller's left/right trigger to adjust the speed of the left/right rumble motor.\n"
                      L"Press any controller button to lock or unlock at the current rumble speed.\n",
                      -1, &rect, 0 );

            for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
            {
                rect.top = i * 80 + 90;
                rect.left = 20;
                DrawText( hDC, g_szMessage[i], -1, &rect, 0 );
            }

            EndPaint( hWnd, &ps );
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
            break;
        }
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}



