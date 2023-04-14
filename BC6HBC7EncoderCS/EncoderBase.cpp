//--------------------------------------------------------------------------------------
// File: EncoderBase.cpp
//
// Common base for Compute Shader 4.0 Accelerated BC6H and BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#include <memory>
#include <vector>

#include <d3d11.h>
#include "DirectXTex.h"
#include "utils.h"
#include "EncoderBase.h"

using namespace DirectX;


//--------------------------------------------------------------------------------------
// Encode the pSourceTexture to BC6H or BC7 format using CS acceleration and save it as file
//
// fmtEncode specifies the target texture format to encode to,
// must be DXGI_FORMAT_BC6H_SF16 or DXGI_FORMAT_BC6H_UF16 or DXGI_FORMAT_BC7_UNORM
//
// in the case of BC7 encoding, if source texture is in sRGB format, the encoded texture
// will have DXGI_FORMAT_BC7_UNORM_SRGB format instead of DXGI_FORMAT_BC7_UNORM
//--------------------------------------------------------------------------------------
HRESULT EncoderBase::GPU_EncodeAndSave( ID3D11Texture2D* pSourceTexture, DXGI_FORMAT fmtEncode, WCHAR* strDstFilename )
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC srcDesc;
    pSourceTexture->GetDesc( &srcDesc );

    D3D11_TEXTURE2D_DESC desc = srcDesc;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.MiscFlags = 0;

    std::vector<ID3D11Buffer*> buffers;

    if ( fmtEncode == DXGI_FORMAT_BC7_TYPELESS || fmtEncode == DXGI_FORMAT_BC7_UNORM || fmtEncode == DXGI_FORMAT_BC7_UNORM_SRGB )
    {
        printf( "\tEncoding to BC7...\n" );
    }
    else if ( fmtEncode == DXGI_FORMAT_BC6H_SF16 || fmtEncode == DXGI_FORMAT_BC6H_UF16 || fmtEncode == DXGI_FORMAT_BC6H_TYPELESS )
    {
        printf( "\tEncoding to BC6H...\n" );
    }
    else
    {
        return E_INVALIDARG;
    }

    UINT srcW = srcDesc.Width, srcH = srcDesc.Height;
    UINT w = srcDesc.Width, h = srcDesc.Height;
    for ( UINT item = 0; item < srcDesc.ArraySize; ++item )
    {
        w = desc.Width = srcW; h = desc.Height = srcH;
        for ( UINT level = 0; level < srcDesc.MipLevels; ++level )
        {
            if ( (desc.Width % 4) != 0 || (desc.Height % 4) != 0 )
                break;

            printf( "\t\tface %d mip %d, %dx%d...", item, level, w, h );

            ID3D11Texture2D* pMipLevel = nullptr;
            m_pDevice->CreateTexture2D( &desc, nullptr, &pMipLevel );

            for ( UINT x = 0; x < desc.Width; x += w )
            {
                for ( UINT y = 0; y < desc.Height; y += h )
                {
                    m_pContext->CopySubresourceRegion( pMipLevel, 0, x, y, 0, pSourceTexture, item * srcDesc.MipLevels + level, nullptr );
                }
            }

            ID3D11Buffer* pBufferMipLevel = nullptr;
            V_GOTO( GPU_Encode( m_pDevice, m_pContext, pMipLevel, fmtEncode, &pBufferMipLevel ) );
            buffers.push_back( pBufferMipLevel );

            SAFE_RELEASE( pMipLevel );

            printf( "done\n" );

            desc.Width >>= 1; if ( desc.Width < 4 ) desc.Width = 4;
            desc.Height >>= 1; if ( desc.Height < 4 ) desc.Height = 4;
            w >>= 1; if ( w < 1 ) w=1;
            h >>= 1; if ( h < 1 ) h=1;
        }
    }

    wprintf( L"\tSaving to %s...", strDstFilename );
    V_GOTO( GPU_SaveToFile( pSourceTexture, strDstFilename, fmtEncode, buffers ) );
    printf( "done\n" );

quit:
    for ( UINT i = 0; i < buffers.size(); ++i )
        SAFE_RELEASE( buffers[i] );

    return hr;
}

//--------------------------------------------------------------------------------------
// Save the encoded texture file
//--------------------------------------------------------------------------------------
HRESULT EncoderBase::GPU_SaveToFile( ID3D11Texture2D* pSrcTexture,
                                     WCHAR* strFilename,
                                     DXGI_FORMAT dstFormat, std::vector<ID3D11Buffer*>& subTextureAsBufs )
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC desc;
    pSrcTexture->GetDesc( &desc );

    if ( (desc.ArraySize * desc.MipLevels) != (UINT)subTextureAsBufs.size() )
        return E_INVALIDARG;

    auto image = std::make_unique<ScratchImage>();
    if ( !image )
    {
        return E_OUTOFMEMORY;
    }
    hr = image->Initialize2D(dstFormat, desc.Width, desc.Height, desc.ArraySize, desc.MipLevels);
    if (FAILED(hr))
        return hr;

    UINT srcW = desc.Width, srcH = desc.Height;
    for ( UINT item = 0; item < desc.ArraySize; ++item )
    {
        desc.Width = srcW; desc.Height = srcH;
        for ( UINT level = 0; level < desc.MipLevels; ++level )
        {
            ID3D11Buffer* pReadbackbuf = CreateAndCopyToCPUBuf( m_pDevice, m_pContext, subTextureAsBufs[item * desc.MipLevels + level] );
            if ( !pReadbackbuf )
            {
                hr = E_OUTOFMEMORY;
                return hr;
            }

            D3D11_MAPPED_SUBRESOURCE mappedSrc;
#pragma warning (push)
#pragma warning (disable:6387)
            m_pContext->Map( pReadbackbuf, 0, D3D11_MAP_READ, 0, &mappedSrc );
            memcpy( image->GetImage(level, item, 0)->pixels, mappedSrc.pData, desc.Height * desc.Width * sizeof(BufferBC6HBC7) / BLOCK_SIZE );
            m_pContext->Unmap( pReadbackbuf, 0 );
#pragma warning (pop)

            SAFE_RELEASE( pReadbackbuf );

            desc.Width >>= 1; if (desc.Width < 4) desc.Width = 4;
            desc.Height >>= 1; if (desc.Height < 4) desc.Height = 4;
        }
    }

    TexMetadata info;
    info = image->GetMetadata();
    info.miscFlags = desc.MiscFlags;                                    // handle the case if TEX_MISC_TEXTURECUBE is present
    if ( IsSRGB(desc.Format) && dstFormat == DXGI_FORMAT_BC7_UNORM )    // input is sRGB, so save the encoded file also as sRGB format
    {
        info.format = DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    hr = SaveToDDSFile( image->GetImages(), image->GetImageCount(), info, DDS_FLAGS_NONE, strFilename );

    return hr;
}
