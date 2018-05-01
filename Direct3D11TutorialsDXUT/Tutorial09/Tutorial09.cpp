//--------------------------------------------------------------------------------------
// File: Tutorial09.cpp
//
// Mesh loading through DXUT
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct CBNeverChanges
{
    XMFLOAT4 vLightDir;
};

struct CBChangesEveryFrame
{
    XMFLOAT4X4 mWorldViewProj;
    XMFLOAT4X4 mWorld;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11InputLayout*          g_pVertexLayout = nullptr;
CDXUTSDKMesh                g_Mesh;
ID3D11Buffer*               g_pCBNeverChanges = nullptr;
ID3D11Buffer*               g_pCBChangesEveryFrame = nullptr;
ID3D11SamplerState*         g_pSamplerLinear = nullptr;
XMMATRIX                    g_World;
XMMATRIX                    g_View;
XMMATRIX                    g_Projection;

static const XMVECTORF32 s_LightDir = { -0.577f, 0.577f, -0.577f, 0.f };

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Tutorial09.fx", nullptr, "VS", "vs_4_0", dwShaderFlags, 0, &pVSBlob ) );

    // Create the vertex shader
    hr = pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) )
    {    
        SAFE_RELEASE( pVSBlob );
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
    hr = pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(), &g_pVertexLayout );
    SAFE_RELEASE( pVSBlob );
    if( FAILED( hr ) )
        return hr;

    // Set the input layout
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    // Compile the pixel shader
    ID3DBlob* pPSBlob  = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Tutorial09.fx", nullptr, "PS", "ps_4_0", dwShaderFlags, 0, &pPSBlob  ) );

    // Create the pixel shader
    hr = pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    SAFE_RELEASE( pPSBlob );
    if( FAILED( hr ) )
        return hr;

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh" ) );

    // Create the constant buffers
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangesEveryFrame ) );

    bd.ByteWidth = sizeof(CBNeverChanges);
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBNeverChanges ) );

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBNeverChanges, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBNeverChanges*>( MappedResource.pData );
    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pCB->vLightDir ), s_LightDir );
    pCB->vLightDir.w = 1.f;
    pd3dImmediateContext->Unmap( g_pCBNeverChanges , 0 );

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
    static const XMVECTORF32 s_Eye = { 0.0f, 3.0f, -500.0f, 0.f };
    static const XMVECTORF32 s_At = { 0.0f, 1.0f, 0.0f, 0.f };
    static const XMVECTORF32 s_Up = { 0.0f, 1.0f, 0.0f, 0.f };
    g_View = XMMatrixLookAtLH( s_Eye, s_At, s_Up );

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    // Setup the projection parameters
    float fAspect = static_cast<float>( pBackBufferSurfaceDesc->Width ) / static_cast<float>( pBackBufferSurfaceDesc->Height );
    g_Projection = XMMatrixPerspectiveFovLH( XM_PI * 0.25f, fAspect, 0.5f, 1000.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Rotate cube around the origin
    g_World = XMMatrixRotationY( 60.0f * XMConvertToRadians((float)fTime) );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    //
    // Clear the back buffer
    //
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );

    //
    // Clear the depth stencil
    //
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    XMMATRIX mWorldViewProjection = g_World * g_View * g_Projection;

    // Update constant buffer that changes once per frame
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBChangesEveryFrame , 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBChangesEveryFrame*>( MappedResource.pData );
    XMStoreFloat4x4( &pCB->mWorldViewProj, XMMatrixTranspose( mWorldViewProjection ) );
    XMStoreFloat4x4( &pCB->mWorld, XMMatrixTranspose( g_World ) );
    pd3dImmediateContext->Unmap( g_pCBChangesEveryFrame , 0 );

    //
    // Set the Vertex Layout
    //
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    //
    // Render the mesh
    //
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh.GetIB11( 0 ), g_Mesh.GetIBFormat11( 0 ), 0 );

    pd3dImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBNeverChanges );
    pd3dImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );

    pd3dImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamplerLinear );

    for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
    {
        auto pSubset = g_Mesh.GetSubset( 0, subset );

        auto PrimType = g_Mesh.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

        // Ignores most of the material information in them mesh to use only a simple shader
        auto pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pDiffuseRV );

        pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
    }
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    g_Mesh.Destroy();

    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pCBNeverChanges );
    SAFE_RELEASE( g_pCBChangesEveryFrame );
    SAFE_RELEASE( g_pSamplerLinear );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1: // Change as needed                
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here

    DXUTInit( true, true, nullptr ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Tutorial09" );

    // Only require 10-level hardware or later
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}
