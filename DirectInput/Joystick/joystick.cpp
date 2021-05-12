//-----------------------------------------------------------------------------
// File: Joystick.cpp
//
// Desc: Demonstrates an application which receives immediate 
//       joystick data in exclusive mode via a dialog timer.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif

#include <Windows.h>

#include <cassert>
#include <cstdio>
#include <memory>
#include <new>

#include <wrl/client.h>

#include <commctrl.h>
#include <basetsd.h>

#pragma warning(push)
#pragma warning(disable:6000 28251)
#include <dinput.h>
#pragma warning(pop)

#include <dinputd.h>
#include <oleauto.h>
#include <shellapi.h>

#include "resource.h"

//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK    EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext) noexcept;
BOOL CALLBACK    EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) noexcept;
HRESULT InitDirectInput(HWND hDlg) noexcept;
VOID FreeDirectInput() noexcept;
HRESULT UpdateInputState(HWND hDlg) noexcept;

// Stuff to filter out XInput devices
#include <wbemidl.h>
HRESULT SetupForIsXInputDevice() noexcept;
bool IsXInputDevice(const GUID* pGuidProductFromDirectInput) noexcept;
void CleanupForIsXInputDevice() noexcept;

struct XINPUT_DEVICE_NODE
{
    DWORD dwVidPid;
    XINPUT_DEVICE_NODE* pNext;
};

struct DI_ENUM_CONTEXT
{
    DIJOYCONFIG* pPreferredJoyCfg;
    bool bPreferredJoyCfgValid;
};

// Enable this to indicate you want to filter out devices supported by XInput via -noxinput
bool g_bFilterOutXinputDevices = false;

XINPUT_DEVICE_NODE* g_pXInputDeviceList = nullptr;

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=nullptr; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }

LPDIRECTINPUT8          g_pDI = nullptr;
LPDIRECTINPUTDEVICE8    g_pJoystick = nullptr;


using Microsoft::WRL::ComPtr;

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int APIENTRY WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return 1;

    InitCommonControls();

    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW(GetCommandLineW(), &nNumArgs);
    for (int iArg = 1; iArg < nNumArgs; iArg++)
    {
        WCHAR* strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if (*strCmdLine == L'/' || *strCmdLine == L'-')
        {
            strCmdLine++;

            const int nArgLen = (int)wcslen(L"noxinput");
            if (_wcsnicmp(strCmdLine, L"noxinput", nArgLen) == 0 && strCmdLine[nArgLen] == 0)
            {
                g_bFilterOutXinputDevices = true;
                continue;
            }
        }
    }
    LocalFree(pstrArgList);

    // Display the main dialog box.
    DialogBox(hInst, MAKEINTRESOURCE(IDD_JOYST_IMM), nullptr, MainDlgProc);

    CoUninitialize();

    return 0;
}




//-----------------------------------------------------------------------------
// Name: MainDialogProc
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (msg)
    {
    case WM_INITDIALOG:
        if (FAILED(InitDirectInput(hDlg)))
        {
            MessageBoxW(nullptr, L"Error Initializing DirectInput",
                L"DirectInput Sample", MB_ICONERROR | MB_OK);
            EndDialog(hDlg, 0);
        }

        // Set a timer to go off 30 times a second. At every timer message
        // the input device will be read
        SetTimer(hDlg, 0, 1000 / 30, nullptr);
        return TRUE;

    case WM_ACTIVATE:
        if (WA_INACTIVE != wParam && g_pJoystick)
        {
            // Make sure the device is acquired, if we are gaining focus.
            g_pJoystick->Acquire();
        }
        return TRUE;

    case WM_TIMER:
        // Update the input device every timer message
        if (FAILED(UpdateInputState(hDlg)))
        {
            KillTimer(hDlg, 0);
            MessageBoxW(nullptr, L"Error Reading Input State. " \
                L"The sample will now exit.", L"DirectInput Sample",
                MB_ICONERROR | MB_OK);
            EndDialog(hDlg, TRUE);
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }

    case WM_DESTROY:
        // Cleanup everything
        KillTimer(hDlg, 0);
        FreeDirectInput();
        return TRUE;
    }

    return FALSE; // Message not handled 
}




//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput(HWND hDlg) noexcept
{
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer
    // to a IDirectInput interface we can use.
    // Create a DInput object
    if (FAILED(hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
        IID_IDirectInput8, (VOID**)&g_pDI, nullptr)))
        return hr;

    if (g_bFilterOutXinputDevices)
        SetupForIsXInputDevice();

    DIJOYCONFIG PreferredJoyCfg = {};
    DI_ENUM_CONTEXT enumContext;
    enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
    enumContext.bPreferredJoyCfgValid = false;

    ComPtr<IDirectInputJoyConfig8> pJoyConfig;
    if (FAILED(hr = g_pDI->QueryInterface(IID_IDirectInputJoyConfig8, (void**)&pJoyConfig)))
        return hr;

    PreferredJoyCfg.dwSize = sizeof(PreferredJoyCfg);
    if (SUCCEEDED(pJoyConfig->GetConfig(0, &PreferredJoyCfg, DIJC_GUIDINSTANCE)))
    {
        // This function is expected to fail if no joystick is attached
        enumContext.bPreferredJoyCfgValid = true;
    }

    // Look for a simple joystick we can use for this sample program.
    if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
        EnumJoysticksCallback,
        &enumContext, DIEDFL_ATTACHEDONLY)))
        return hr;

    if (g_bFilterOutXinputDevices)
        CleanupForIsXInputDevice();

    // Make sure we got a joystick
    if (!g_pJoystick)
    {
        MessageBoxW(nullptr, L"Joystick not found. The sample will now exit.",
            L"DirectInput Sample",
            MB_ICONERROR | MB_OK);
        EndDialog(hDlg, 0);
        return S_OK;
    }

    // Set the data format to "simple joystick" - a predefined data format 
    //
    // A data format specifies which controls on a device we are interested in,
    // and how they should be reported. This tells DInput that we will be
    // passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
    if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2)))
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    if (FAILED(hr = g_pJoystick->SetCooperativeLevel(hDlg,
        DISCL_EXCLUSIVE | DISCL_FOREGROUND)))
        return hr;

    // Enumerate the joystick objects. The callback function enabled user
    // interface elements for objects that are found, and sets the min/max
    // values property for discovered axes.
    if (FAILED(hr = g_pJoystick->EnumObjects(EnumObjectsCallback,
        (VOID*)hDlg, DIDFT_ALL)))
        return hr;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains 
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it's an XInput device
// Unfortunately this information can not be found by just using DirectInput.
// Checking against a VID/PID of 0x028E/0x045E won't find 3rd party or future 
// XInput devices.
//
// This function stores the list of xinput devices in a linked list 
// at g_pXInputDeviceList, and IsXInputDevice() searchs that linked list
//-----------------------------------------------------------------------------
namespace
{
    struct bstr_deleter { void operator()(void* p) noexcept { SysFreeString(static_cast<BSTR>(p)); } };

    using ScopedBSTR = std::unique_ptr<OLECHAR[], bstr_deleter>;
}

HRESULT SetupForIsXInputDevice() noexcept
{
    // COM needs to be initialized on this thread before the enumeration.

    // So we can call VariantClear() later, even if we never had a successful IWbemClassObject::Get().
    VARIANT var = {};
    VariantInit(&var);

    // Create WMI
    ComPtr<IWbemLocator> pIWbemLocator;
    HRESULT hr = CoCreateInstance(__uuidof(WbemLocator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pIWbemLocator));
    if (FAILED(hr) || pIWbemLocator == nullptr)
        return hr;

    // Create BSTRs for WMI
    ScopedBSTR bstrNamespace(SysAllocString(L"\\\\.\\root\\cimv2"));
    ScopedBSTR bstrClassName(SysAllocString(L"Win32_PNPEntity"));
    ScopedBSTR bstrDeviceID(SysAllocString(L"DeviceID"));
    if (!bstrNamespace || !bstrClassName || !bstrDeviceID)
        return E_OUTOFMEMORY;

    // Connect to WMI 
    ComPtr<IWbemServices> pIWbemServices;
    hr = pIWbemLocator->ConnectServer(bstrNamespace.get(), nullptr, nullptr, 0L,
        0L, nullptr, nullptr, &pIWbemServices);
    if (FAILED(hr) || pIWbemServices == nullptr)
        return hr;

    // Switch security level to IMPERSONATE
    hr = CoSetProxyBlanket(pIWbemServices.Get(),
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE);
    if (FAILED(hr))
        return hr;

    // Get list of Win32_PNPEntity devices
    ComPtr<IEnumWbemClassObject> pEnumDevices;
    hr = pIWbemServices->CreateInstanceEnum(bstrClassName.get(), 0, nullptr, &pEnumDevices);
    if (FAILED(hr) || pEnumDevices == nullptr)
        return hr;

    // Loop over all devices
    IWbemClassObject* pDevices[20] = {};
    for (;;)
    {
        ULONG uReturned = 0;
        hr = pEnumDevices->Next(10000, _countof(pDevices), pDevices, &uReturned);
        if (FAILED(hr))
            return hr;

        if (uReturned == 0)
            break;

        assert(uReturned <= _countof(pDevices));
        _Analysis_assume_(uReturned <= _countof(pDevices));

        for (size_t iDevice = 0; iDevice < uReturned; ++iDevice)
        {
            if (!pDevices[iDevice])
                continue;

            // For each device, get its device ID
            hr = pDevices[iDevice]->Get(bstrDeviceID.get(), 0L, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != nullptr)
            {
                // Check if the device ID contains "IG_".  If it does, then it's an XInput device
                // Unfortunately this information can not be found by just using DirectInput 
                if (wcsstr(var.bstrVal, L"IG_"))
                {
                    // If it does, then get the VID/PID from var.bstrVal
                    DWORD dwPid = 0, dwVid = 0;
                    const WCHAR* strVid = wcsstr(var.bstrVal, L"VID_");
                    if (strVid && swscanf_s(strVid, L"VID_%4X", &dwVid) != 1)
                        dwVid = 0;
                    const WCHAR* strPid = wcsstr(var.bstrVal, L"PID_");
                    if (strPid && swscanf_s(strPid, L"PID_%4X", &dwPid) != 1)
                        dwPid = 0;

                    const DWORD dwVidPid = MAKELONG(dwVid, dwPid);

                    // Add the VID/PID to a linked list
                    auto pNewNode = new (std::nothrow) XINPUT_DEVICE_NODE;
                    if (pNewNode)
                    {
                        pNewNode->dwVidPid = dwVidPid;
                        pNewNode->pNext = g_pXInputDeviceList;
                        g_pXInputDeviceList = pNewNode;
                    }
                }
            }

            VariantClear(&var);
            SAFE_RELEASE(pDevices[iDevice]);
        }
    }

    VariantClear(&var);

    for (size_t iDevice = 0; iDevice < _countof(pDevices); ++iDevice)
        SAFE_RELEASE(pDevices[iDevice]);

    return hr;
}


//-----------------------------------------------------------------------------
// Returns true if the DirectInput device is also an XInput device.
// Call SetupForIsXInputDevice() before, and CleanupForIsXInputDevice() after
//-----------------------------------------------------------------------------
bool IsXInputDevice(const GUID* pGuidProductFromDirectInput) noexcept
{
    // Check each xinput device to see if this device's vid/pid matches
    XINPUT_DEVICE_NODE* pNode = g_pXInputDeviceList;
    while (pNode)
    {
        if (pNode->dwVidPid == pGuidProductFromDirectInput->Data1)
            return true;
        pNode = pNode->pNext;
    }

    return false;
}


//-----------------------------------------------------------------------------
// Cleanup needed for IsXInputDevice()
//-----------------------------------------------------------------------------
void CleanupForIsXInputDevice() noexcept
{
    // Cleanup linked list
    XINPUT_DEVICE_NODE* pNode = g_pXInputDeviceList;
    while (pNode)
    {
        XINPUT_DEVICE_NODE* pDelete = pNode;
        pNode = pNode->pNext;
        SAFE_DELETE(pDelete);
    }
}



//-----------------------------------------------------------------------------
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance,
    VOID* pContext) noexcept
{
    auto pEnumContext = static_cast<DI_ENUM_CONTEXT*>(pContext);

    if (g_bFilterOutXinputDevices && IsXInputDevice(&pdidInstance->guidProduct))
        return DIENUM_CONTINUE;

    // Skip anything other than the perferred joystick device as defined by the control panel.  
    // Instead you could store all the enumerated joysticks and let the user pick.
    if (pEnumContext->bPreferredJoyCfgValid &&
        !IsEqualGUID(pdidInstance->guidInstance, pEnumContext->pPreferredJoyCfg->guidInstance))
        return DIENUM_CONTINUE;

    // Obtain an interface to the enumerated joystick.
    HRESULT hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pJoystick, nullptr);

    // If it failed, then we can't use this joystick. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    if (FAILED(hr))
        return DIENUM_CONTINUE;

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
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi,
    VOID* pContext) noexcept
{
    auto hDlg = static_cast<HWND>(pContext);

    static int nSliderCount = 0;  // Number of returned slider controls
    static int nPOVCount = 0;     // Number of returned POV controls

    // For axes that are returned, set the DIPROP_RANGE property for the
    // enumerated axis in order to scale min/max values.
    if (pdidoi->dwType & DIDFT_AXIS)
    {
        DIPROPRANGE diprg;
        diprg.diph.dwSize = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwHow = DIPH_BYID;
        diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
        diprg.lMin = -1000;
        diprg.lMax = +1000;

        // Set the range for the axis
        if (FAILED(g_pJoystick->SetProperty(DIPROP_RANGE, &diprg.diph)))
            return DIENUM_STOP;

    }


    // Set the UI to reflect what objects the joystick supports
    if (pdidoi->guidType == GUID_XAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_X_AXIS), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_X_AXIS_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_YAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_Y_AXIS), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_Y_AXIS_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_ZAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_Z_AXIS), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_Z_AXIS_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_RxAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_X_ROT), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_X_ROT_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_RyAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_Y_ROT), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_Y_ROT_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_RzAxis)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_Z_ROT), TRUE);
        EnableWindow(GetDlgItem(hDlg, IDC_Z_ROT_TEXT), TRUE);
    }
    if (pdidoi->guidType == GUID_Slider)
    {
        switch (nSliderCount++)
        {
        case 0:
            EnableWindow(GetDlgItem(hDlg, IDC_SLIDER0), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_SLIDER0_TEXT), TRUE);
            break;

        case 1:
            EnableWindow(GetDlgItem(hDlg, IDC_SLIDER1), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_SLIDER1_TEXT), TRUE);
            break;
        }
    }
    if (pdidoi->guidType == GUID_POV)
    {
        switch (nPOVCount++)
        {
        case 0:
            EnableWindow(GetDlgItem(hDlg, IDC_POV0), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_POV0_TEXT), TRUE);
            break;

        case 1:
            EnableWindow(GetDlgItem(hDlg, IDC_POV1), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_POV1_TEXT), TRUE);
            break;

        case 2:
            EnableWindow(GetDlgItem(hDlg, IDC_POV2), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_POV2_TEXT), TRUE);
            break;

        case 3:
            EnableWindow(GetDlgItem(hDlg, IDC_POV3), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_POV3_TEXT), TRUE);
            break;
        }
    }

    return DIENUM_CONTINUE;
}




//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
HRESULT UpdateInputState(HWND hDlg) noexcept
{
    if (!g_pJoystick)
        return S_OK;

    // Poll the device to read the current state
    HRESULT hr = g_pJoystick->Poll();
    if (FAILED(hr))
    {
        // DInput is telling us that the input stream has been
        // interrupted. We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done. We
        // just re-acquire and try again.
        hr = g_pJoystick->Acquire();
        while (hr == DIERR_INPUTLOST)
            hr = g_pJoystick->Acquire();

        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
        return S_OK;
    }

    // Get the input's device state
    DIJOYSTATE2 js;
    if (FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
        return hr; // The device should have been acquired during the Poll()

    // Display joystick state to dialog
    WCHAR strText[512] = {};

    // Axes
    swprintf_s(strText, L"%ld", js.lX);
    SetWindowTextW(GetDlgItem(hDlg, IDC_X_AXIS), strText);
    swprintf_s(strText, L"%ld", js.lY);
    SetWindowTextW(GetDlgItem(hDlg, IDC_Y_AXIS), strText);
    swprintf_s(strText, L"%ld", js.lZ);
    SetWindowTextW(GetDlgItem(hDlg, IDC_Z_AXIS), strText);
    swprintf_s(strText, L"%ld", js.lRx);
    SetWindowTextW(GetDlgItem(hDlg, IDC_X_ROT), strText);
    swprintf_s(strText, L"%ld", js.lRy);
    SetWindowTextW(GetDlgItem(hDlg, IDC_Y_ROT), strText);
    swprintf_s(strText, L"%ld", js.lRz);
    SetWindowTextW(GetDlgItem(hDlg, IDC_Z_ROT), strText);

    // Slider controls
    swprintf_s(strText, L"%ld", js.rglSlider[0]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_SLIDER0), strText);
    swprintf_s(strText, L"%ld", js.rglSlider[1]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_SLIDER1), strText);

    // Points of view
    swprintf_s(strText, L"%lu", js.rgdwPOV[0]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_POV0), strText);
    swprintf_s(strText, L"%lu", js.rgdwPOV[1]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_POV1), strText);
    swprintf_s(strText, L"%lu", js.rgdwPOV[2]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_POV2), strText);
    swprintf_s(strText, L"%lu", js.rgdwPOV[3]);
    SetWindowTextW(GetDlgItem(hDlg, IDC_POV3), strText);


    // Fill up text with which buttons are pressed
    wcscpy_s(strText, L"");
    for (int i = 0; i < 128; i++)
    {
        if (js.rgbButtons[i] & 0x80)
        {
            WCHAR sz[128] = {};
            swprintf_s(sz, L"%02d ", i);
            wcscat_s(strText, sz);
        }
    }

    SetWindowTextW(GetDlgItem(hDlg, IDC_BUTTONS), strText);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput() noexcept
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if (g_pJoystick)
        g_pJoystick->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE(g_pJoystick);
    SAFE_RELEASE(g_pDI);
}
