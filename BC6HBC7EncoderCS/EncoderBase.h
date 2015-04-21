//--------------------------------------------------------------------------------------
// File: EncoderBase.h
//
// Common base for Compute Shader 4.0 Accelerated BC6H and BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef __ENCODERBASE_H
#define __ENCODERBASE_H

#pragma once

struct BufferBC6HBC7
{
    UINT color[4];
};

#define BLOCK_SIZE_Y			4
#define BLOCK_SIZE_X			4
#define BLOCK_SIZE				(BLOCK_SIZE_Y * BLOCK_SIZE_X)

class EncoderBase
{
public:
    EncoderBase() : 
      m_pDevice(nullptr),
      m_pContext(nullptr)
    {}
    
    virtual
    HRESULT Initialize( ID3D11Device* pDevice, ID3D11DeviceContext* pContext )
    {
        m_pDevice = pDevice;
        m_pContext = pContext;
        return S_OK;
    }

    //--------------------------------------------------------------------------------------
    // Encode the pSourceTexture to BC6H or BC7 format using CS acceleration and save it as file
    //
    // fmtEncode specifies the target texture format to encode to, 
    // must be DXGI_FORMAT_BC6H_SF16 or DXGI_FORMAT_BC6H_UF16 or DXGI_FORMAT_BC7_UNORM 
    //
    // in the case of BC7 encoding, if source texture is in sRGB format, the encoded texture
    // will have DXGI_FORMAT_BC7_UNORM_SRGB format instead of DXGI_FORMAT_BC7_UNORM
    //--------------------------------------------------------------------------------------    
    HRESULT GPU_EncodeAndSave( ID3D11Texture2D* pSourceTexture,
                               DXGI_FORMAT fmtEncode, WCHAR* strDstFilename );
    
protected:
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pContext;

    virtual 
    HRESULT GPU_Encode( ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                        ID3D11Texture2D* pSrcTexture, 
                        DXGI_FORMAT dstFormat, ID3D11Buffer** ppDstTextureAsBufOut ) = 0;
    
    HRESULT GPU_SaveToFile( ID3D11Texture2D* pSrcTexture,
                            WCHAR* strFilename,
                            DXGI_FORMAT dstFormat, std::vector<ID3D11Buffer*>& subTextureAsBufs );    
};

#endif