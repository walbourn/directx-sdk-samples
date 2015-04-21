//-----------------------------------------------------------------------------
// File: CustomFormat.cpp
//
// Desc: demonstrates the use of a custom data format for input retrieval from 
// a device which doesn't correspond to one of the predefined mouse, keyboard, 
// or joystick types.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <commctrl.h>
#include <basetsd.h>

#pragma warning(push)
#pragma warning(disable:6000 28251)
#include <dinput.h>
#pragma warning(pop)

#include "resource.h"

#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL          0x80000000
#endif

// Here we define a custom data format to store input from a mouse. In a 
// real program you would almost certainly use either the predefined 
// DIMOUSESTATE or DIMOUSESTATE2 structure to store mouse input, but some 
// input devices such as the Sidewinder GameVoice controller are not well
// described by the provided types and may require custom formats.

struct MouseState
{
    LONG lAxisX;
    LONG lAxisY;
    BYTE abButtons[3];
    BYTE bPadding;       // Structure must be DWORD multiple in size.   
};

// Each device object for which you want to receive input must have an entry
// in this DIOBJECTDATAFORMAT array which is stored in the custom DIDATAFORMAT.
// The DIOBJECTDATAFORMAT maps detected device object to a particular offset
// within MouseState structure declared above. Inside the input routine, a
// MouseState structure is provided to the GetDeviceState method, and
// DirectInput uses this offset to store the input data in the provided
// structure. 
// 
// Any of the elements which are not flagged as DIDFT_OPTIONAL, and
// which describe a device object which is not found on the actual device will
// cause the SetDeviceFormat call to fail. For the format defined below, the
// system mouse must have an x-axis, y-axis, and at least one button. 

DIOBJECTDATAFORMAT g_aObjectFormats[] =
{
    { &GUID_XAxis, FIELD_OFFSET( MouseState, lAxisX ),    // X axis
        DIDFT_AXIS | DIDFT_ANYINSTANCE, 0 },
    { &GUID_YAxis, FIELD_OFFSET( MouseState, lAxisY ),    // Y axis
        DIDFT_AXIS | DIDFT_ANYINSTANCE, 0 },
    { 0, FIELD_OFFSET( MouseState, abButtons[0] ),        // Button 0
        DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0 },
    { 0, FIELD_OFFSET( MouseState, abButtons[1] ),        // Button 1 (optional)
        DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { 0, FIELD_OFFSET( MouseState, abButtons[2] ),        // Button 2 (optional)
        DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 }
};
#define numMouseObjects (sizeof(g_aObjectFormats) / sizeof(DIOBJECTDATAFORMAT))

// Finally, the DIDATAFORMAT is filled with the information defined above for 
// our custom data format. The format also defines whether the returned axis 
// data is absolute or relative. Usually mouse movement is reported in relative 
// coordinates, but our custom format will use absolute coordinates. 

DIDATAFORMAT            g_dfMouse =
{
    sizeof( DIDATAFORMAT ),
    sizeof( DIOBJECTDATAFORMAT ),
    DIDF_ABSAXIS,
    sizeof( MouseState ),
    numMouseObjects,
    g_aObjectFormats
};




//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }

LPDIRECTINPUT8          g_pDI = nullptr; // DirectInput interface       
LPDIRECTINPUTDEVICE8    g_pMouse = nullptr; // Device interface




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT InitDirectInput( HWND hDlg );
VOID FreeDirectInput();
HRESULT UpdateInputState( HWND hDlg );





//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int APIENTRY WinMain( _In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int )
{
    InitCommonControls();

    // Display the main dialog box.
    DialogBox( hInst, MAKEINTRESOURCE( IDD_MOUSE_IMM ), nullptr, MainDlgProc );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MainDialogProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
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
            break;

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
    if( FAILED( hr = DirectInput8Create( GetModuleHandle( nullptr ), DIRECTINPUT_VERSION,
                                         IID_IDirectInput8, ( VOID** )&g_pDI, nullptr ) ) )
        return hr;

    // Retrieve the system mouse
    if( FAILED( g_pDI->CreateDevice( GUID_SysMouse, &g_pMouse, nullptr ) ) )
    {
        MessageBox( nullptr, TEXT( "Mouse not found. The sample will now exit." ),
                    TEXT( "DirectInput Sample" ),
                    MB_ICONERROR | MB_OK );
        EndDialog( hDlg, 0 );
        return S_OK;
    }

    // A data format specifies which controls on a device we are interested in,
    // and how they should be reported. This tells DInput that we will be
    // passing a MouseState structure to IDirectInputDevice::GetDeviceState().
    if( FAILED( hr = g_pMouse->SetDataFormat( &g_dfMouse ) ) )
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    if( FAILED( hr = g_pMouse->SetCooperativeLevel( hDlg, DISCL_NONEXCLUSIVE |
                                                    DISCL_FOREGROUND ) ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
HRESULT UpdateInputState( HWND hDlg )
{
    HRESULT hr;
    TCHAR strText[128] = {0}; // Device state text
    MouseState ms;           // Custom mouse state 

    static POINT pOrigin = {0};           // Initial position
    static BOOL bInitialized = FALSE;    // Indicates offsets are valid

    if( !g_pMouse )
        return S_OK;

    // Poll the device to read the current state
    hr = g_pMouse->Poll();
    if( FAILED( hr ) )
    {
        // DInput is telling us that the input stream has been
        // interrupted. We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done. We
        // just re-acquire and try again.
        hr = g_pMouse->Acquire();
        while( hr == DIERR_INPUTLOST )
            hr = g_pMouse->Acquire();

        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
        return S_OK;
    }

    // Get the input's device state
    if( FAILED( hr = g_pMouse->GetDeviceState( sizeof( MouseState ), &ms ) ) )
        return hr; // The device should have been acquired during the Poll()

    // The initial mouse position should be subracted from the current point. 
    if( !bInitialized )
    {
        bInitialized = TRUE;
        pOrigin.x = ms.lAxisX;
        pOrigin.y = ms.lAxisY;
    }

    // Display state to dialog
    _stprintf_s( strText, 128, TEXT( "%ld" ), ms.lAxisX - pOrigin.x );
    SetWindowText( GetDlgItem( hDlg, IDC_X_AXIS ), strText );
    _stprintf_s( strText, 128, TEXT( "%ld" ), ms.lAxisY - pOrigin.y );
    SetWindowText( GetDlgItem( hDlg, IDC_Y_AXIS ), strText );

    // Fill up text with which buttons are pressed
    strText[0] = 0;
    for( int i = 0; i < 3; i++ )
    {
        if( ms.abButtons[i] & 0x80 )
        {
            TCHAR sz[128];
            _stprintf_s( sz, 128, TEXT( "%02d " ), i );
            _tcscat_s( strText, 128, sz );
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
    if( g_pMouse )
        g_pMouse->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE( g_pMouse );
    SAFE_RELEASE( g_pDI );
}



