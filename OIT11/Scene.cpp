//-----------------------------------------------------------------------------
// File: Scene.cpp
//
// Desc: Holds a description for a simple scene usend in the Order Independent
//       Transparency sample.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "Scene.h"

using namespace DirectX;

struct SCENE_VERTEX
{
    XMFLOAT4 pos;
    XMFLOAT4 color;
};

//--------------------------------------------------------------------------------------
// CScene constructor
//--------------------------------------------------------------------------------------
CScene::CScene() :
    m_pVertexShader(nullptr),
    m_pVertexLayout(nullptr),
    m_pVS_CB(nullptr),
    m_pVB(nullptr)
{
}


//--------------------------------------------------------------------------------------
// Allocate device resources
//--------------------------------------------------------------------------------------
HRESULT CScene::OnD3D11CreateDevice( ID3D11Device* pDevice )
{
    HRESULT hr;
    
    // Create the vertex shader
    ID3DBlob* pBlobVS = nullptr;
    V_RETURN( DXUTCompileFromFile( L"SceneVS.hlsl", nullptr, "SceneVS", "vs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobVS ) );
    V_RETURN( pDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), nullptr, &m_pVertexShader ) );
    DXUT_SetDebugName( m_pVertexShader, "SceneVS" );

    // Create the input layout
    const D3D11_INPUT_ELEMENT_DESC vertexLayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pDevice->CreateInputLayout( vertexLayout, 2, pBlobVS->GetBufferPointer(),
                                             pBlobVS->GetBufferSize(), &m_pVertexLayout ) );
    DXUT_SetDebugName( m_pVertexLayout, "Primary" );
    SAFE_RELEASE( pBlobVS );

    // Set up constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( VS_CB );
    V_RETURN( pDevice->CreateBuffer( &Desc, nullptr, &m_pVS_CB ) );
    DXUT_SetDebugName( m_pVS_CB, "VS_CB" );

    // Set up vertex buffer
    float fRight =  -10.0f;
    float fTop =    -10.0f;
    float fLeft =    10.0f;
    float fLowH =    -5.0f;

    // Fill the vertex buffer
    SCENE_VERTEX pVertex[12];
    pVertex[0].pos = XMFLOAT4( fLeft, fLowH, 50.0f, 1.0f );
    pVertex[1].pos = XMFLOAT4( fLeft, fTop, 50.0f, 1.0f );
    pVertex[2].pos = XMFLOAT4( fRight, fLowH, 50.0f, 1.0f );
    pVertex[3].pos = XMFLOAT4( fRight, fTop, 50.0f, 1.0f );
    
    pVertex[0].color = XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.5f );
    pVertex[1].color = XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.5f );
    pVertex[2].color = XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.5f );
    pVertex[3].color = XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.5f );

    pVertex[4].pos = XMFLOAT4( fLeft, fLowH, 60.0f, 1.0f );
    pVertex[5].pos = XMFLOAT4( fLeft, fTop, 60.0f, 1.0f );
    pVertex[6].pos = XMFLOAT4( fRight, fLowH, 40.0f, 1.0f );
    pVertex[7].pos = XMFLOAT4( fRight, fTop, 40.0f, 1.0f );

    pVertex[4].color = XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.5f );
    pVertex[5].color = XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.5f );
    pVertex[6].color = XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.5f );
    pVertex[7].color = XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.5f );

    pVertex[8].pos = XMFLOAT4( fLeft, fLowH, 40.0f, 1.0f );
    pVertex[9].pos = XMFLOAT4( fLeft, fTop, 40.0f, 1.0f );
    pVertex[10].pos = XMFLOAT4( fRight, fLowH, 60.0f, 1.0f );
    pVertex[11].pos = XMFLOAT4( fRight, fTop, 60.0f, 1.0f );

    pVertex[8].color = XMFLOAT4( 0.0f, 0.0f, 1.0f, 0.5f );
    pVertex[9].color = XMFLOAT4( 0.0f, 0.0f, 1.0f, 0.5f );
    pVertex[10].color = XMFLOAT4( 0.0f, 0.0f, 1.0f, 0.5f );
    pVertex[11].color = XMFLOAT4( 0.0f, 0.0f, 1.0f, 0.5f );

    D3D11_BUFFER_DESC vbdesc;
    vbdesc.ByteWidth = 12 * sizeof( SCENE_VERTEX );
    vbdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdesc.CPUAccessFlags = 0;
    vbdesc.MiscFlags = 0;    

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertex;    
    V( pDevice->CreateBuffer( &vbdesc, &InitData, &m_pVB ) );
    DXUT_SetDebugName( m_pVB, "Vertices") ;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene
//--------------------------------------------------------------------------------------
void CScene::D3D11Render( CXMMATRIX mWVP, ID3D11DeviceContext* pd3dImmediateContext )
{
    HRESULT hr;
    
    pd3dImmediateContext->IASetInputLayout( m_pVertexLayout );

    UINT uStrides = sizeof( SCENE_VERTEX );
    UINT uOffsets = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pVB, &uStrides, &uOffsets );
    pd3dImmediateContext->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, 0 );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pd3dImmediateContext->VSSetShader( m_pVertexShader, nullptr, 0 );

    // Update the constant buffer
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( m_pVS_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVS_CB = reinterpret_cast<VS_CB*>( MappedResource.pData ); 
    XMStoreFloat4x4( &pVS_CB->mWorldViewProj, mWVP );
    pd3dImmediateContext->Unmap( m_pVS_CB, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pVS_CB );

    pd3dImmediateContext->Draw( 4, 0 );

    uOffsets = 4*sizeof( SCENE_VERTEX );
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pVB, &uStrides, &uOffsets );
    pd3dImmediateContext->Draw( 4, 0 );

    uOffsets = 8*sizeof( SCENE_VERTEX );
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pVB, &uStrides, &uOffsets );
    pd3dImmediateContext->Draw( 4, 0 );

}


//--------------------------------------------------------------------------------------
// Release device resources
//--------------------------------------------------------------------------------------
void CScene::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pVertexShader );
    SAFE_RELEASE( m_pVertexLayout );
    SAFE_RELEASE( m_pVB );
    SAFE_RELEASE( m_pVS_CB );
}
