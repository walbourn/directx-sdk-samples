//--------------------------------------------------------------------------------------
// File: ComputeShaderSort11.cpp
//
// Demonstrates how to use compute shaders to perform sorting on the GPU with DirectX 11.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <assert.h>
#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <vector>
#include <algorithm>
#include <random>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

// The number of elements to sort is limited to an even power of 2
// At minimum 8,192 elements - BITONIC_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE
// At maximum 262,144 elements - BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE
const UINT NUM_ELEMENTS = 512 * 512;
const UINT BITONIC_BLOCK_SIZE = 512;
const UINT TRANSPOSE_BLOCK_SIZE = 16;
const UINT MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
const UINT MATRIX_HEIGHT = NUM_ELEMENTS / BITONIC_BLOCK_SIZE;

std::vector<UINT> data( NUM_ELEMENTS );
std::vector<UINT> results( NUM_ELEMENTS );

ID3D11Device*               g_pd3dDevice = nullptr;
ID3D11DeviceContext*        g_pd3dImmediateContext = nullptr;
ID3D11ComputeShader*        g_pComputeShaderBitonic = nullptr;
ID3D11ComputeShader*        g_pComputeShaderTranspose = nullptr;
ID3D11Buffer*               g_pCB = nullptr;
ID3D11Buffer*               g_pBuffer1 = nullptr;
ID3D11ShaderResourceView*   g_pBuffer1SRV = nullptr;
ID3D11UnorderedAccessView*  g_pBuffer1UAV = nullptr;
ID3D11Buffer*               g_pBuffer2 = nullptr;
ID3D11ShaderResourceView*   g_pBuffer2SRV = nullptr;
ID3D11UnorderedAccessView*  g_pBuffer2UAV = nullptr;
ID3D11Buffer*               g_pReadBackBuffer = nullptr;

// Constant Buffer Layout
struct CB
{
    UINT iLevel;
    UINT iLevelMask;
    UINT iWidth;
    UINT iHeight;
};


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( _In_z_ const WCHAR* szFileName, _In_z_ LPCSTR szEntryPoint, _In_z_ LPCSTR szShaderModel, _Outptr_ ID3DBlob** ppBlobOut );
HRESULT FindDXSDKShaderFileCch( _Out_writes_(cchDest) WCHAR* strDestPath,
                                _In_ int cchDest, 
                                _In_z_ LPCWSTR strFilename );

//--------------------------------------------------------------------------------------
// Create the Device
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL fl[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    // Create a hardware Direct3D 11 device
    // This DXUT helper calls D3D11CreateDevice via LoadLibrary
    hr = D3D11CreateDevice( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                            fl, ARRAYSIZE(fl), D3D11_SDK_VERSION,
                            &g_pd3dDevice, nullptr, &g_pd3dImmediateContext );
    if( FAILED( hr ) )
        return hr;

    // Check if the hardware device supports Compute Shader 4.0
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
    g_pd3dDevice->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
    if( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
    {
        SAFE_RELEASE( g_pd3dImmediateContext );
        SAFE_RELEASE( g_pd3dDevice );

        int result = MessageBox( 0, L"This program needs to use the Direct3D 11 reference device.  This device implements the entire Direct3D 11 feature set, but runs very slowly.  Do you wish to continue?",
                                 L"Compute Shader Sort", MB_ICONQUESTION | MB_YESNO );
        if( result == IDNO )
            return E_FAIL;
        
        // Create a reference device if hardware is not available
        // This DXUT helper calls D3D11CreateDevice via LoadLibrary
        hr = D3D11CreateDevice( nullptr, D3D_DRIVER_TYPE_REFERENCE, nullptr, createDeviceFlags,
                                fl, ARRAYSIZE(fl), D3D11_SDK_VERSION,
                                &g_pd3dDevice, nullptr, &g_pd3dImmediateContext );
        if( FAILED( hr ) )
            return hr;

        printf( "Using Direct3D 11 Reference Device\n" );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Find and compile the specified shader
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CompileShaderFromFile( const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    if ( !ppBlobOut )
        return E_INVALIDARG;

    // find the file
    WCHAR str[MAX_PATH];
    HRESULT hr = FindDXSDKShaderFileCch( str, MAX_PATH, szFileName );
    if ( FAILED(hr) )
        return hr;

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

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile( str, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szShaderModel, 
                             dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
    if( FAILED(hr) )
    {
        if( pErrorBlob )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );

        SAFE_RELEASE( pErrorBlob );

        return hr;
    }

    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create the Resources
//--------------------------------------------------------------------------------------
HRESULT CreateResources()
{
    HRESULT hr = S_OK;
    ID3DBlob* pBlob = nullptr;

    // Compile the Bitonic Sort Compute Shader
    hr = CompileShaderFromFile( L"ComputeShaderSort11.hlsl", "BitonicSort", "cs_4_0", &pBlob );
    if( FAILED( hr ) )
        return hr;

    // Create the Bitonic Sort Compute Shader
    hr = g_pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &g_pComputeShaderBitonic );
    if( FAILED( hr ) )
        return hr;
    SAFE_RELEASE( pBlob );
#if defined(_DEBUG) || defined(PROFILE)
    g_pComputeShaderBitonic->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BitonicSort" ) - 1, "BitonicSort" );
#endif

    // Compile the Matrix Transpose Compute Shader
    hr = CompileShaderFromFile( L"ComputeShaderSort11.hlsl", "MatrixTranspose", "cs_4_0", &pBlob );
    if( FAILED( hr ) )
        return hr;

    // Create the Matrix Transpose Compute Shader
    hr = g_pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &g_pComputeShaderTranspose );
    if( FAILED( hr ) )
        return hr;
    SAFE_RELEASE( pBlob );
#if defined(_DEBUG) || defined(PROFILE)
    g_pComputeShaderTranspose->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "MatrixTranspose" ) - 1, "MatrixTranspose" );
#endif

    // Create the Const Buffer
    D3D11_BUFFER_DESC constant_buffer_desc = {};
    constant_buffer_desc.ByteWidth = sizeof(CB);
    constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer( &constant_buffer_desc, nullptr, &g_pCB );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pCB->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "CB" ) - 1, "CB" );
#endif

    // Create the Buffer of Elements
    // Create 2 buffers for switching between when performing the transpose
    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth = NUM_ELEMENTS * sizeof(UINT);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buffer_desc.StructureByteStride = sizeof(UINT);
    hr = g_pd3dDevice->CreateBuffer( &buffer_desc, nullptr, &g_pBuffer1 );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pBuffer1->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer1" ) - 1, "Buffer1" );
#endif

    hr = g_pd3dDevice->CreateBuffer( &buffer_desc, nullptr, &g_pBuffer2 );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pBuffer2->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer2" ) - 1, "Buffer2" );
#endif

    // Create the Shader Resource View for the Buffers
    // This is used for reading the buffer during the transpose
    D3D11_SHADER_RESOURCE_VIEW_DESC srvbuffer_desc = {};
    srvbuffer_desc.Format = DXGI_FORMAT_UNKNOWN;
    srvbuffer_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvbuffer_desc.Buffer.ElementWidth = NUM_ELEMENTS;
    hr = g_pd3dDevice->CreateShaderResourceView( g_pBuffer1, &srvbuffer_desc, &g_pBuffer1SRV );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pBuffer1SRV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer1 SRV" ) - 1, "Buffer1 SRV" );
#endif

    hr = g_pd3dDevice->CreateShaderResourceView( g_pBuffer2, &srvbuffer_desc, &g_pBuffer2SRV );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pBuffer2SRV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer2 SRV" ) - 1, "Buffer2 SRV" );
#endif

    // Create the Unordered Access View for the Buffers
    // This is used for writing the buffer during the sort and transpose
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavbuffer_desc = {};
    uavbuffer_desc.Format = DXGI_FORMAT_UNKNOWN;
    uavbuffer_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavbuffer_desc.Buffer.NumElements = NUM_ELEMENTS;
    hr = g_pd3dDevice->CreateUnorderedAccessView( g_pBuffer1, &uavbuffer_desc, &g_pBuffer1UAV );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
     g_pBuffer1UAV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer1 UAV" ) - 1, "Buffer1 UAV" );
#endif

    hr = g_pd3dDevice->CreateUnorderedAccessView( g_pBuffer2, &uavbuffer_desc, &g_pBuffer2UAV );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
    g_pBuffer2UAV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "Buffer2 UAV" ) - 1, "Buffer2 UAV" );
#endif

    // Create the Readback Buffer
    // This is used to read the results back to the CPU
    D3D11_BUFFER_DESC readback_buffer_desc = {};
    readback_buffer_desc.ByteWidth = NUM_ELEMENTS * sizeof(UINT);
    readback_buffer_desc.Usage = D3D11_USAGE_STAGING;
    readback_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    readback_buffer_desc.StructureByteStride = sizeof(UINT);
    hr = g_pd3dDevice->CreateBuffer( &readback_buffer_desc, nullptr, &g_pReadBackBuffer );
    if( FAILED( hr ) )
        return hr;
#if defined(_DEBUG) || defined(PROFILE)
     g_pReadBackBuffer->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "ReadBack" ) - 1, "ReadBack" );
#endif

    return hr;
}


//--------------------------------------------------------------------------------------
// Helper to set the compute shader constants
//--------------------------------------------------------------------------------------
void SetConstants( UINT iLevel, UINT iLevelMask, UINT iWidth, UINT iHeight )
{
    CB cb = { iLevel, iLevelMask, iWidth, iHeight };
    g_pd3dImmediateContext->UpdateSubresource( g_pCB, 0, nullptr, &cb, 0, 0 );
    g_pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &g_pCB );
}


//--------------------------------------------------------------------------------------
// GPU Bitonic Sort
//--------------------------------------------------------------------------------------
void GPUSort()
{
    // Upload the data
    g_pd3dImmediateContext->UpdateSubresource( g_pBuffer1, 0, nullptr, &data[0], 0, 0 );

    // Sort the data
    // First sort the rows for the levels <= to the block size
    for( UINT level = 2 ; level <= BITONIC_BLOCK_SIZE ; level = level * 2 )
    {
        SetConstants( level, level, MATRIX_HEIGHT, MATRIX_WIDTH );

        // Sort the row data
        g_pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &g_pBuffer1UAV, nullptr );
        g_pd3dImmediateContext->CSSetShader( g_pComputeShaderBitonic, nullptr, 0 );
        g_pd3dImmediateContext->Dispatch( NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1 );
    }
    
    // Then sort the rows and columns for the levels > than the block size
    // Transpose. Sort the Columns. Transpose. Sort the Rows.
    for( UINT level = (BITONIC_BLOCK_SIZE * 2) ; level <= NUM_ELEMENTS ; level = level * 2 )
    {
        SetConstants( (level / BITONIC_BLOCK_SIZE), (level & ~NUM_ELEMENTS) / BITONIC_BLOCK_SIZE, MATRIX_WIDTH, MATRIX_HEIGHT );

        // Transpose the data from buffer 1 into buffer 2
        ID3D11ShaderResourceView* pViewnullptr = nullptr;
        g_pd3dImmediateContext->CSSetShaderResources( 0, 1, &pViewnullptr );
        g_pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &g_pBuffer2UAV, nullptr );
        g_pd3dImmediateContext->CSSetShaderResources( 0, 1, &g_pBuffer1SRV );
        g_pd3dImmediateContext->CSSetShader( g_pComputeShaderTranspose, nullptr, 0 );
        g_pd3dImmediateContext->Dispatch( MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, 1 );

        // Sort the transposed column data
        g_pd3dImmediateContext->CSSetShader( g_pComputeShaderBitonic, nullptr, 0 );
        g_pd3dImmediateContext->Dispatch( NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1 );

        SetConstants( BITONIC_BLOCK_SIZE, level, MATRIX_HEIGHT, MATRIX_WIDTH );

        // Transpose the data from buffer 2 back into buffer 1
        g_pd3dImmediateContext->CSSetShaderResources( 0, 1, &pViewnullptr );
        g_pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &g_pBuffer1UAV, nullptr );
        g_pd3dImmediateContext->CSSetShaderResources( 0, 1, &g_pBuffer2SRV );
        g_pd3dImmediateContext->CSSetShader( g_pComputeShaderTranspose, nullptr, 0 );
        g_pd3dImmediateContext->Dispatch( MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, 1 );

        // Sort the row data
        g_pd3dImmediateContext->CSSetShader( g_pComputeShaderBitonic, nullptr, 0 );
        g_pd3dImmediateContext->Dispatch( NUM_ELEMENTS / BITONIC_BLOCK_SIZE, 1, 1 );
    }

    // Download the data
    D3D11_MAPPED_SUBRESOURCE MappedResource = {0}; 
    g_pd3dImmediateContext->CopyResource( g_pReadBackBuffer, g_pBuffer1 );
    if( SUCCEEDED( g_pd3dImmediateContext->Map( g_pReadBackBuffer, 0, D3D11_MAP_READ, 0, &MappedResource ) ) )
    {
        _Analysis_assume_( MappedResource.pData);
        assert( MappedResource.pData );
        memcpy( &results[0], MappedResource.pData, NUM_ELEMENTS * sizeof(UINT) );
        g_pd3dImmediateContext->Unmap( g_pReadBackBuffer, 0 );
    }
}


//--------------------------------------------------------------------------------------
// CPU Sort
//--------------------------------------------------------------------------------------
void CPUSort()
{
    std::sort( data.begin(), data.end() );
}


//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int __cdecl wmain()
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Generate a random list of numbers to sort
    std::random_device rd;
    std::mt19937 mt(rd());
    std::generate( data.begin(), data.end(), [&] { return mt(); } );

    // Create the device
    HRESULT hr = InitDevice();
    if ( FAILED (hr) ) 
    {
        printf( "Failed to create the device.  Exiting.\n" );
        return 1;
    }

    // Create the buffers and shaders
    if( FAILED( CreateResources() ) )
    {
        printf( "Failed to create resources.  Exiting.\n" );
        return 1;
    }

    printf( "Sorting %u Elements\n", NUM_ELEMENTS );

    // GPU Bitonic Sort
    printf( "Starting GPU Bitonic Sort...\n" );
    GPUSort();
    printf( "...GPU Bitonic Sort Finished\n" );

    // Sort the data on the CPU to compare for correctness
    printf( "Starting CPU Sort...\n" );
    CPUSort();
    printf( "...CPU Sort Finished\n" );

    // Compare the results for correctness
    bool bComparisonSucceeded = true;
    for( UINT i = 0 ; i < NUM_ELEMENTS ; ++i )
    {
        if( data[i] != results[i] )
        {
            bComparisonSucceeded = false;
            break;
        }
    }
    printf( "Comparison %s\n", (bComparisonSucceeded)? "Succeeded" : "FAILED" );

    // Cleanup the resources
    SAFE_RELEASE( g_pReadBackBuffer );
    SAFE_RELEASE( g_pBuffer2UAV );
    SAFE_RELEASE( g_pBuffer1UAV );
    SAFE_RELEASE( g_pBuffer2SRV );
    SAFE_RELEASE( g_pBuffer1SRV );
    SAFE_RELEASE( g_pBuffer2 );
    SAFE_RELEASE( g_pBuffer1 );
    SAFE_RELEASE( g_pCB );
    SAFE_RELEASE( g_pComputeShaderTranspose );
    SAFE_RELEASE( g_pComputeShaderBitonic );
    SAFE_RELEASE( g_pd3dImmediateContext );
    SAFE_RELEASE( g_pd3dDevice );

    return 0;
}

//--------------------------------------------------------------------------------------
// Tries to find the location of the shader file
// This is a trimmed down version of DXUTFindDXSDKMediaFileCch.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT FindDXSDKShaderFileCch( WCHAR* strDestPath,
                                int cchDest, 
                                LPCWSTR strFilename )
{
    if( !strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10 )
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] =
    {
        0
    };
    WCHAR strExeName[MAX_PATH] =
    {
        0
    };
    WCHAR* strLastSlash = nullptr;
    GetModuleFileName( nullptr, strExePath, MAX_PATH );
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr( strExePath, TEXT( '\\' ) );
    if( strLastSlash )
    {
        wcscpy_s( strExeName, MAX_PATH, &strLastSlash[1] );

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr( strExeName, TEXT( '.' ) );
        if( strLastSlash )
            *strLastSlash = 0;
    }

    // Search in directories:
    //      .\
    //      %EXE_DIR%\..\..\%EXE_NAME%

    wcscpy_s( strDestPath, cchDest, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;

    swprintf_s( strDestPath, cchDest, L"%s\\..\\..\\%s\\%s", strExePath, strExeName, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;    

    // On failure, return the file as the path but also return an error code
    wcscpy_s( strDestPath, cchDest, strFilename );

    return E_FAIL;
}