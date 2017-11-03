//-----------------------------------------------------------------------------
// File: Scene.h
//
// Desc: Holds a description for a simple scene usend in the Order Independent
//       Transparency sample.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

class CScene
{
public:
    CScene();

    HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pDevice );
    void    D3D11Render( DirectX::CXMMATRIX mWVP, ID3D11DeviceContext* pd3dImmediateContext );
    void    OnD3D11DestroyDevice();

protected:
    struct VS_CB
    {
        DirectX::XMFLOAT4X4 mWorldViewProj;
    };    

    ID3D11VertexShader* m_pVertexShader;
    ID3D11InputLayout*  m_pVertexLayout;
    ID3D11Buffer*       m_pVS_CB;
    ID3D11Buffer*       m_pVB;
};
