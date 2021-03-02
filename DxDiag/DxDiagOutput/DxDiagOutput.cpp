//----------------------------------------------------------------------------
// File: DxDiagOutput.cpp
//
// Desc: Sample app to read info from dxdiagn.dll by enumeration
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-----------------------------------------------------------------------------
#define INITGUID
#include <Windows.h>
#include <stdio.h>
#include <assert.h>
#include <initguid.h>
#include <dxdiag.h>

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;


//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
HRESULT PrintContainerAndChildren(WCHAR* wszParentName, IDxDiagContainer* pDxDiagContainer);




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point for the application.  We use just the console window
//-----------------------------------------------------------------------------
int main()
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return 1;

    // CoCreate a IDxDiagProvider*
    ComPtr<IDxDiagProvider> pDxDiagProvider;
    hr = CoCreateInstance(CLSID_DxDiagProvider,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IDxDiagProvider,
        (LPVOID*)&pDxDiagProvider);
    if (FAILED(hr))
        return 1;

    // Fill out a DXDIAG_INIT_PARAMS struct and pass it to IDxDiagContainer::Initialize
    // Passing in TRUE for bAllowWHQLChecks, allows dxdiag to check if drivers are
    // digital signed as logo'd by WHQL which may connect via internet to update
    // WHQL certificates.
    DXDIAG_INIT_PARAMS dxDiagInitParam = {};
    dxDiagInitParam.dwSize = sizeof(DXDIAG_INIT_PARAMS);
    dxDiagInitParam.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
    dxDiagInitParam.bAllowWHQLChecks = TRUE;
    dxDiagInitParam.pReserved = nullptr;

    hr = pDxDiagProvider->Initialize(&dxDiagInitParam);
    if (FAILED(hr))
        return 1;

    ComPtr<IDxDiagContainer> pDxDiagRoot;
    hr = pDxDiagProvider->GetRootContainer(&pDxDiagRoot);
    if (FAILED(hr))
        return 1;

    // This function will recursivly print the properties
    // the root node and all its child.
    hr = PrintContainerAndChildren(nullptr, pDxDiagRoot.Get());
    if (FAILED(hr))
        return 1;

    CoUninitialize();

    return 0;
}




//-----------------------------------------------------------------------------
// Name: PrintContainerAndChildren()
// Desc: Recursivly print the properties the root node and all its child
//       to the console window
//-----------------------------------------------------------------------------
HRESULT PrintContainerAndChildren(WCHAR* wszParentName, IDxDiagContainer* pDxDiagContainer)
{
    HRESULT hr;

    DWORD dwPropCount;
    DWORD dwPropIndex;
    WCHAR wszPropName[512];
    VARIANT var;
    WCHAR wszPropValue[512];

    DWORD dwChildCount;
    DWORD dwChildIndex;
    WCHAR wszChildName[512];
    ComPtr<IDxDiagContainer> pChildContainer;

    VariantInit(&var);

    hr = pDxDiagContainer->GetNumberOfProps(&dwPropCount);
    if (SUCCEEDED(hr))
    {
        // Print each property in this container
        for (dwPropIndex = 0; dwPropIndex < dwPropCount; dwPropIndex++)
        {
            hr = pDxDiagContainer->EnumPropNames(dwPropIndex, wszPropName, 512);
            if (SUCCEEDED(hr))
            {
                hr = pDxDiagContainer->GetProp(wszPropName, &var);
                if (SUCCEEDED(hr))
                {
                    // Switch off the type.  There's 4 different types:
                    switch (var.vt)
                    {
                    case VT_UI4:
                        swprintf_s(wszPropValue, 512, L"%d", var.ulVal);
                        break;
                    case VT_I4:
                        swprintf_s(wszPropValue, 512, L"%d", var.lVal);
                        break;
                    case VT_BOOL:
                        wcscpy_s(wszPropValue, 512, (var.boolVal) ? L"true" : L"false");
                        break;
                    case VT_BSTR:
                        wcscpy_s(wszPropValue, 512, var.bstrVal);
                        break;
                    }

                    // Add the parent name to the front if there's one, so that
                    // its easier to read on the screen
                    if (wszParentName)
                        wprintf(L"%ls.%ls = %ls\n", wszParentName, wszPropName, wszPropValue);
                    else
                        wprintf(L"%ls = %ls\n", wszPropName, wszPropValue);

                    // Clear the variant (this is needed to free BSTR memory)
                    VariantClear(&var);
                }
            }
        }
    }

    // Recursivly call this function for each of its child containers
    hr = pDxDiagContainer->GetNumberOfChildContainers(&dwChildCount);
    if (SUCCEEDED(hr))
    {
        for (dwChildIndex = 0; dwChildIndex < dwChildCount; dwChildIndex++)
        {
            hr = pDxDiagContainer->EnumChildContainerNames(dwChildIndex, wszChildName, 512);
            if (SUCCEEDED(hr))
            {
                hr = pDxDiagContainer->GetChildContainer(wszChildName, &pChildContainer);
                if (SUCCEEDED(hr))
                {
                    // wszFullChildName isn't needed but is used for text output
                    WCHAR wszFullChildName[512];
                    if (wszParentName)
                        swprintf_s(wszFullChildName, 512, L"%ls.%ls", wszParentName, wszChildName);
                    else
                        wcscpy_s(wszFullChildName, 512, wszChildName);
                    PrintContainerAndChildren(wszFullChildName, pChildContainer.Get());
                }
            }
        }
    }

    return S_OK;
}
