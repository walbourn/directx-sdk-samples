//-----------------------------------------------------------------------------
// File: skybox11.cpp
//
// Desc: Encapsulation of skybox geometry and textures
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "skybox11.h"

using namespace DirectX;

struct SKYBOX_VERTEX
{
    XMFLOAT4 pos;
};

const D3D11_INPUT_ELEMENT_DESC g_aVertexLayout[] =
{
    { "POSITION",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};


//--------------------------------------------------------------------------------------
CSkybox11::CSkybox11() :
    m_pEnvironmentMap11(nullptr),
    m_pEnvironmentRV11(nullptr),
     m_pd3dDevice11(nullptr),
    m_pVertexShader(nullptr),
    m_pPixelShader(nullptr),
    m_pSam(nullptr),
    m_pVertexLayout11(nullptr),
    m_pcbVSPerObject(nullptr),
    m_pVB11(nullptr),
    m_pDepthStencilState11(nullptr),
    m_fSize(1.0f)
{
}


//--------------------------------------------------------------------------------------
HRESULT CSkybox11::OnD3D11CreateDevice( ID3D11Device* pd3dDevice, float fSize, 
                                        ID3D11Texture2D* pCubeTexture, ID3D11ShaderResourceView* pCubeRV )
{
    HRESULT hr;
    
    m_pd3dDevice11 = pd3dDevice;
    m_fSize = fSize;
    m_pEnvironmentMap11 = pCubeTexture;
    m_pEnvironmentRV11 = pCubeRV;

    ID3DBlob* pBlobVS = nullptr;
    ID3DBlob* pBlobPS = nullptr;

    // Create the shaders
    V_RETURN( DXUTCompileFromFile( L"skybox11.hlsl", nullptr, "SkyboxVS", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobVS ) );
    V_RETURN( DXUTCompileFromFile( L"skybox11.hlsl", nullptr, "SkyboxPS", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPS ) );

    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), nullptr, &m_pVertexShader ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), nullptr, &m_pPixelShader ) );

    DXUT_SetDebugName( m_pVertexShader, "SkyboxVS" );
    DXUT_SetDebugName( m_pPixelShader, "SkyboxPS" );

    // Create an input layout
    V_RETURN( pd3dDevice->CreateInputLayout( g_aVertexLayout, 1, pBlobVS->GetBufferPointer(),
                                             pBlobVS->GetBufferSize(), &m_pVertexLayout11 ) );
    DXUT_SetDebugName( m_pVertexLayout11, "Primary" );

    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );

    // Query support for linear filtering on DXGI_FORMAT_R32G32B32A32
    UINT FormatSupport = 0;
    V_RETURN( pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R32G32B32A32_FLOAT, &FormatSupport ) );

    // Setup linear or point sampler according to the format Query result
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = ( FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE ) > 0 ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &m_pSam ) );  
    DXUT_SetDebugName( m_pSam, "Primary" );

    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &m_pcbVSPerObject ) );
    DXUT_SetDebugName( m_pcbVSPerObject, "CB_VS_PER_OBJECT" );
    
    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC DSDesc = {};
    DSDesc.DepthEnable = FALSE;
    DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DSDesc.StencilEnable = FALSE;
    V_RETURN( pd3dDevice->CreateDepthStencilState( &DSDesc, &m_pDepthStencilState11 ) );
    DXUT_SetDebugName( m_pDepthStencilState11, "DepthStencil" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
void CSkybox11::OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr;

    if ( !m_pd3dDevice11 )
        return;

    // Fill the vertex buffer
    SKYBOX_VERTEX* pVertex = new SKYBOX_VERTEX[4];
    if ( !pVertex )
        return;

    // Map texels to pixels 
    float fHighW = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fHighH = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    float fLowW = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fLowH = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    
    pVertex[0].pos = XMFLOAT4( fLowW, fLowH, 1.0f, 1.0f );
    pVertex[1].pos = XMFLOAT4( fLowW, fHighH, 1.0f, 1.0f );
    pVertex[2].pos = XMFLOAT4( fHighW, fLowH, 1.0f, 1.0f );
    pVertex[3].pos = XMFLOAT4( fHighW, fHighH, 1.0f, 1.0f );

    UINT uiVertBufSize = 4 * sizeof( SKYBOX_VERTEX );
    //Vertex Buffer
    D3D11_BUFFER_DESC vbdesc;
    vbdesc.ByteWidth = uiVertBufSize;
    vbdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdesc.CPUAccessFlags = 0;
    vbdesc.MiscFlags = 0;    

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertex;    
    V( m_pd3dDevice11->CreateBuffer( &vbdesc, &InitData, &m_pVB11 ) );
    DXUT_SetDebugName( m_pVB11, "SkyBox" );
    SAFE_DELETE_ARRAY( pVertex ); 
}


//--------------------------------------------------------------------------------------
void CSkybox11::D3D11Render( CXMMATRIX mWorldViewProj, ID3D11DeviceContext* pd3dImmediateContext )
{
    HRESULT hr;
    
    pd3dImmediateContext->IASetInputLayout( m_pVertexLayout11 );

    UINT uStrides = sizeof( SKYBOX_VERTEX );
    UINT uOffsets = 0;
    ID3D11Buffer* pBuffers[1] = { m_pVB11 };
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, &uStrides, &uOffsets );
    pd3dImmediateContext->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, 0 );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pd3dImmediateContext->VSSetShader( m_pVertexShader, nullptr, 0 );
    pd3dImmediateContext->PSSetShader( m_pPixelShader, nullptr, 0 );

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( m_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData ); 
    XMMATRIX m = XMMatrixInverse( nullptr, mWorldViewProj );
    XMStoreFloat4x4( &pVSPerObject->m_WorldViewProj, m );
    pd3dImmediateContext->Unmap( m_pcbVSPerObject, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pcbVSPerObject );

    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSam );
    pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pEnvironmentRV11 );

    ID3D11DepthStencilState* pDepthStencilStateStored11 = nullptr;
    UINT StencilRef;
    pd3dImmediateContext->OMGetDepthStencilState( &pDepthStencilStateStored11, &StencilRef );
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilState11, 0 );

    pd3dImmediateContext->Draw( 4, 0 );

    pd3dImmediateContext->OMSetDepthStencilState( pDepthStencilStateStored11, StencilRef );
}


//--------------------------------------------------------------------------------------
void CSkybox11::OnD3D11ReleasingSwapChain()
{
    SAFE_RELEASE( m_pVB11 );
}


//--------------------------------------------------------------------------------------
void CSkybox11::OnD3D11DestroyDevice()
{
    m_pd3dDevice11 = nullptr;
    SAFE_RELEASE( m_pEnvironmentMap11 );
    SAFE_RELEASE( m_pEnvironmentRV11 );
    SAFE_RELEASE( m_pSam );
    SAFE_RELEASE( m_pVertexShader );
    SAFE_RELEASE( m_pPixelShader );
    SAFE_RELEASE( m_pVertexLayout11 );
    SAFE_RELEASE( m_pcbVSPerObject );
    SAFE_RELEASE( m_pDepthStencilState11 );
}
