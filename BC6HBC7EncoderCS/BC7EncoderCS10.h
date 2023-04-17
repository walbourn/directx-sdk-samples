//--------------------------------------------------------------------------------------
// File: BC7EncoderCS10.h
//
// Compute Shader 4.0 Accelerated BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#pragma once

class CGPUBC7Encoder : public EncoderBase
{
public:
    CGPUBC7Encoder() :
      EncoderBase(),
      m_pTryMode456CS( nullptr ),
      m_pTryMode137CS( nullptr ),
      m_pTryMode02CS( nullptr ),
      m_pEncodeBlockCS( nullptr ),
      m_fAlphaWeight( 1.0f )
    {}

    HRESULT Initialize( ID3D11Device* pDevice, ID3D11DeviceContext* pContext ) override;
    void Cleanup();
    void SetAlphaWeight( const float fWeight ) { m_fAlphaWeight = fWeight; }


protected:
    ID3D11ComputeShader* m_pTryMode456CS;
    ID3D11ComputeShader* m_pTryMode137CS;
    ID3D11ComputeShader* m_pTryMode02CS;
    ID3D11ComputeShader* m_pEncodeBlockCS;

    float                m_fAlphaWeight;

    HRESULT GPU_Encode(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
        ID3D11Texture2D* pSrcTexture,
        DXGI_FORMAT dstFormat, ID3D11Buffer** ppDstTextureAsBufOut) override;
};
