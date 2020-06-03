//--------------------------------------------------------------------------------------
// File: BC6HBC7EncoderCS.cpp
//
// The main file of the Compute Shader Accelerated BC6H BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include "EncoderBase.h"
#include "BC6HEncoderCS10.h"
#include "BC7EncoderCS10.h"
#include "DirectXTex.h"
#include "utils.h"

using namespace DirectX;

ID3D11Device*               g_pDevice = nullptr;
ID3D11DeviceContext*        g_pContext = nullptr;
ID3D11Texture2D*            g_pSourceTexture = nullptr;

CGPUBC6HEncoder             g_GPUBC6HEncoder;
CGPUBC7Encoder              g_GPUBC7Encoder;

struct CommandLineOptions 
{
    enum Mode
    {
        MODE_ENCODE_BC6HS,
        MODE_ENCODE_BC6HU,
        MODE_ENCODE_BC7,        
        MODE_NOT_SET
    } mode;

    BOOL bNoMips;
    BOOL bSRGB;
    DWORD dwFilter;
    float fBC7AlphaWeight;

    CommandLineOptions() :
        mode(MODE_NOT_SET),
        bNoMips(FALSE),
        bSRGB(FALSE),
        dwFilter(TEX_FILTER_DEFAULT),
        fBC7AlphaWeight(1.0f)
    {
    }

    BOOL SetMode( Mode mode )
    {
        // Three modes are mutually exclusive
        if ( this->mode == MODE_NOT_SET )
        {
            this->mode = mode;
            return TRUE;
        }

        printf( "Only one of the /bc6hs, /bc6hu and /bc7 options can be set at a time\n" );
        return FALSE;
    }

} g_CommandLineOptions;

struct SValue
{
    LPCWSTR pName;
    DWORD dwValue;
};

SValue g_pFilters[] = 
{
    { L"POINT",         TEX_FILTER_POINT                                },
    { L"LINEAR",        TEX_FILTER_LINEAR                               },
    { L"CUBIC",         TEX_FILTER_CUBIC                                },
    { L"FANT",          TEX_FILTER_FANT                                 },
    { L"POINT_DITHER",  TEX_FILTER_POINT | TEX_FILTER_DITHER_DIFFUSION  },
    { L"LINEAR_DITHER", TEX_FILTER_LINEAR | TEX_FILTER_DITHER_DIFFUSION },
    { L"CUBIC_DITHER",  TEX_FILTER_CUBIC | TEX_FILTER_DITHER_DIFFUSION  },
    { L"FANT_DITHER",   TEX_FILTER_FANT  | TEX_FILTER_DITHER_DIFFUSION  },
    { nullptr,          TEX_FILTER_DEFAULT                              }
};

//--------------------------------------------------------------------------------------
// Prints out the list of names in the array of name-value pairs
//--------------------------------------------------------------------------------------
void PrintList(SValue* pValue)
{
    while ( pValue->pName )
    {
        wprintf( L"\t/%s\n", pValue->pName );        
        pValue++;
    }    
}

//--------------------------------------------------------------------------------------
// Look up the value of the name-value pair in its array
//--------------------------------------------------------------------------------------
DWORD LookupByName(const WCHAR* pName, const SValue* pArray)
{
    while ( pArray->pName )
    {
        if ( !_wcsicmp(pName, pArray->pName) )
        {
            return pArray->dwValue;
        }

        pArray++;
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Encode the source texture to BC6H or BC7 and save the encoded texture as file
//--------------------------------------------------------------------------------------
HRESULT Encode( WCHAR* strSrcFilename, ID3D11Texture2D* pSourceTexture, DXGI_FORMAT fmtEncode, EncoderBase* pEncoder )
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC srcTexDesc;
    pSourceTexture->GetDesc( &srcTexDesc );

    if ( (srcTexDesc.Width % 4) != 0 || (srcTexDesc.Height % 4) != 0 )
    {
        printf("\tERROR: Input source image size %d by %d must be a multiple of 4\n", srcTexDesc.Width, srcTexDesc.Height );
        return E_FAIL;
    }

    std::wstring fname = strSrcFilename;

    INT pos = (INT)fname.rfind( '.' );
    if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HS ||
         g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HU )
    {
        fname.insert( pos, L"_BC6" );
    }
    else
    {
        fname.insert( pos, L"_BC7" );
    }
    pos = (INT)fname.rfind( '.' );
    fname.erase( pos + 1, fname.length() );
    fname += std::wstring( L"dds" );

    V_RETURN( pEncoder->GPU_EncodeAndSave( pSourceTexture, fmtEncode, &fname[0] ) );

    return hr;
}

//--------------------------------------------------------------------------------------
// Cleanup before exit
//--------------------------------------------------------------------------------------
void Cleanup()
{    
    g_GPUBC6HEncoder.Cleanup();
    g_GPUBC7Encoder.Cleanup();    
    SAFE_RELEASE( g_pSourceTexture );
    SAFE_RELEASE( g_pContext );
    SAFE_RELEASE( g_pDevice );
}

//--------------------------------------------------------------------------------------
// Simple helper to test whether a string can be interpreted as a float
//--------------------------------------------------------------------------------------
#include <sstream>
bool isFloat( std::wstring myString ) 
{
    std::wistringstream  iss(myString);
    float f;
    iss >> std::noskipws >> f;    
    return iss.eof() && !iss.fail(); 
}

//--------------------------------------------------------------------------------------
// Parse command line options
//--------------------------------------------------------------------------------------
BOOL ParseCommandLine( int argc, WCHAR* argv[] )
{
    DWORD dw;

    for ( int i = 1; i < argc; ++i )
    {
        if ( wcscmp( argv[i], L"/bc6hs" ) == 0 )
        {
            if ( !g_CommandLineOptions.SetMode( CommandLineOptions::MODE_ENCODE_BC6HS ) )
            {
                return FALSE;
            }
        } else if ( wcscmp( argv[i], L"/bc6hu" ) == 0 )
        {
            if ( !g_CommandLineOptions.SetMode( CommandLineOptions::MODE_ENCODE_BC6HU ) )
            {
                return FALSE;
            }
        } else if ( wcscmp( argv[i], L"/bc7" ) == 0 )
        {
            if ( !g_CommandLineOptions.SetMode( CommandLineOptions::MODE_ENCODE_BC7 ) )
            {
                return FALSE;
            }
        } else if ( wcscmp( argv[i], L"/nomips" ) == 0 )
        {
            g_CommandLineOptions.bNoMips = TRUE;
        } else if ( wcscmp( argv[i], L"/srgb" ) == 0 )
        {
            g_CommandLineOptions.bSRGB = TRUE;
        } else if ( wcscmp( argv[i], L"/aw" ) == 0 )
        {
            if ( isFloat( argv[i+1] ) )
            {
                g_CommandLineOptions.fBC7AlphaWeight = static_cast<float>( _wtof( argv[i+1] ) );
                i += 1; // skip the next cmd line parameter
            } else
            {
                return FALSE;
            }
        } else if
            ( argv[i][0] == L'/' )
        {
            if ( (dw = LookupByName( argv[i] + 1, g_pFilters )) != 0 )
            {
                g_CommandLineOptions.dwFilter = dw;
            } else
            {
                wprintf( L"Unknown option %s\n", argv[i] );
                return FALSE;
            }                        
        }
    }

    if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_NOT_SET )
    {
        return FALSE;
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Entry point
//--------------------------------------------------------------------------------------
int __cdecl wmain(int argc, WCHAR* argv[])
{
    int nReturn = 0;
    
    printf( "Microsoft (R) Direct3D11 DirectCompute Accelerated BC6H BC7 Encoder\n\n" );

    if ( argc < 3 )
    {                
        printf( "Usage: BC6HBC7EncoderCS.exe (options) (filter) Filename0 Filename1 Filename2...\n\n" );

        printf( "\tWhere (options) can be the following:\n\n" );

        printf( "\t/bc6hs\t\tEncode to BC6H_SF16 and save the encoded texture\n" );
        printf( "\t/bc6hu\t\tEncode to BC6H_UF16 and save the encoded texture\n" );
        printf( "\t/bc7\t\tEncode to BC7 and save the encoded texture\n\n" );

        printf( "\tOne and only one of the above options must be present, the following options are optional:\n\n" );

        printf( "\t/nomips\t\tDo not generate mip levels\n" );
        printf( "\t/srgb\t\tSave to sRGB format, only available when encoding to BC7\n" );
        printf( "\t/aw weight\tSet the weight of alpha channel during BC7 encoding. Weight is a float number, its default is 1, meaning alpha channel receives the same weight as each of R, G and B channel.\n\n" );

        printf( "\t(filter) is also optional, it selects the filter being used when generating mips and/or converting formats and can be one of the following:\n\n");

        PrintList( g_pFilters );

        printf( "\n\tIf the input texture already has mip chain, that mip chain is used directly. If it doesn't have a mip chain and /nomips is not specified, mip chain is generated.\n\n" );

        printf( "\tOnce a certain operation is chosen by the options above, the same operation will be performed on all input Filename[i]\n\n" );

        return 1;
    }

    if ( !ParseCommandLine( argc, argv ) )
    {
        printf( "Invalid command line parameter(s)\n" );
        return 1;
    }

    // Initialize COM
    if ( FAILED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) ) )
    {
        printf( "Failed to initialize COM\n" );
        return 1;
    }

    // Create the hardware device with the highest possible feature level
    printf( "Creating device..." );
    if ( FAILED( CreateDevice( &g_pDevice, &g_pContext ) ) )
    {
        return 1;
    }
    printf( "done\n" );

    // Check for Compute Shader 4.x support    
    {
        printf( "Checking CS4x capability..." );
        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
        g_pDevice->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
        if ( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
        {
            printf( "Sorry your driver and/or video card doesn't support DirectCompute 4.x\n" );
            nReturn = 1;
            Cleanup();
            return  nReturn;
        }

        // CS4x capability found, initialize our CS accelerated encoders
        printf( "Using CS Accelerated Encoder\n" );        
        if ( FAILED( g_GPUBC7Encoder.Initialize( g_pDevice, g_pContext ) ) )
        {
            nReturn = 1;
            Cleanup();
            return  nReturn;
        }
        g_GPUBC7Encoder.SetAlphaWeight( g_CommandLineOptions.fBC7AlphaWeight );

        if ( FAILED( g_GPUBC6HEncoder.Initialize( g_pDevice, g_pContext ) ) )
        {
            nReturn = 1;
            Cleanup();
            return  nReturn;
        }
    }

    // Process the input files
    for ( int i = 1; i < argc; ++i )
    {
        // Skip all command line options
        if ( argv[i][0] == L'/' )
            continue;

        if ( !FileExists( argv[i] ) )
        {
            wprintf( L"\nFile not found: %s\n", argv[i] );
            continue;
        } 

        wprintf( L"\nProcessing source texture %s...\n", argv[i] );

        DXGI_FORMAT fmtLoadAs = DXGI_FORMAT_UNKNOWN;
        if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HS ||
             g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HU )
        {
            fmtLoadAs = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
        else if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC7 )
        {
            fmtLoadAs = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
                
        SAFE_RELEASE( g_pSourceTexture );
        if ( FAILED( LoadTextureFromFile( g_pDevice, argv[i], fmtLoadAs,
            g_CommandLineOptions.bNoMips, static_cast<TEX_FILTER_FLAGS>(g_CommandLineOptions.dwFilter), &g_pSourceTexture ) ) )
        {
            printf( "error reading source texture file, it must exist and be in uncompressed texture2D format(texture array and cube map are supported but texture3D is not currently supported)\n" );
            continue;
        }

        if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC7 )
        {
            // Encode to BC7
            if ( g_CommandLineOptions.bSRGB )
            {
                if ( FAILED( Encode( argv[i], g_pSourceTexture, DXGI_FORMAT_BC7_UNORM_SRGB, &g_GPUBC7Encoder ) ) )  
                {
                    printf("\nFailed BC7 SRGB encoding %S\n", argv[i] );
                    nReturn = 1;
                    continue; 
                }
            } 
            else
            {
                if ( FAILED( Encode( argv[i], g_pSourceTexture, DXGI_FORMAT_BC7_UNORM, &g_GPUBC7Encoder ) ) )  
                {
                    printf("\nFailed BC7 encoding %S\n", argv[i] );
                    nReturn = 1;
                    continue; 
                }
            }
        } else if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HU )
        {
            // Encode to BC6HU
            if ( FAILED( Encode( argv[i], g_pSourceTexture, DXGI_FORMAT_BC6H_UF16, &g_GPUBC6HEncoder ) ) )
            {
                printf("\nFailed BC6HU encoding %S\n", argv[i] );
                nReturn = 1;
                continue; 
            }
        } else if ( g_CommandLineOptions.mode == CommandLineOptions::MODE_ENCODE_BC6HS )
        {
            // Encode to BC6HS
            if ( FAILED( Encode( argv[i], g_pSourceTexture, DXGI_FORMAT_BC6H_SF16, &g_GPUBC6HEncoder ) ) )
            {
                printf("\nFailed BC6HS encoding %S\n", argv[i] );
                nReturn = 1;
                continue; 
            }
        }
    }

    Cleanup();

    return nReturn;
}

