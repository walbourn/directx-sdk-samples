//--------------------------------------------------------------------------------------
// File: BC6HEncoderCS10.h
//
// Compute Shader 4.0 Accelerated BC6H Encoder
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#pragma once

class CGPUBC6HEncoder : public EncoderBase
{
public:
    CGPUBC6HEncoder() :
      EncoderBase(),
      m_pTryModeG10CS( nullptr ),
      m_pTryModeLE10CS( nullptr ),
      m_pEncodeBlockCS( nullptr )
    {}

    HRESULT Initialize( ID3D11Device* pDevice, ID3D11DeviceContext* pContext ) override;
    void Cleanup();


protected:
    ID3D11ComputeShader* m_pTryModeG10CS;
    ID3D11ComputeShader* m_pTryModeLE10CS;
    ID3D11ComputeShader* m_pEncodeBlockCS;

    HRESULT GPU_Encode(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
        ID3D11Texture2D* pSrcTexture,
        DXGI_FORMAT dstFormat, ID3D11Buffer** ppDstTextureAsBufOut) override;
};
