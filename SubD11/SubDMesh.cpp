//--------------------------------------------------------------------------------------
// File: SubDMesh.cpp
//
// This class encapsulates the mesh loading and housekeeping functions for a SubDMesh.
// The mesh loads preprocessed SDKMESH files from disk and stages them for rendering.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SubDMesh.h"
#include "sdkmisc.h"
#include "DXUTRes.h"

#include <DirectXCollision.h>

using namespace DirectX;

ID3D11Texture2D* g_pDefaultDiffuseTexture = nullptr;
ID3D11Texture2D* g_pDefaultNormalTexture = nullptr;
ID3D11Texture2D* g_pDefaultSpecularTexture = nullptr;

ID3D11ShaderResourceView* g_pDefaultDiffuseSRV = nullptr;
ID3D11ShaderResourceView* g_pDefaultNormalSRV = nullptr;
ID3D11ShaderResourceView* g_pDefaultSpecularSRV = nullptr;

struct CB_PER_SUBSET_CONSTANTS
{
    int m_iPatchStartIndex;

    DWORD m_Padding[3];
};

static const UINT g_iBindPerSubset = 3;

//--------------------------------------------------------------------------------------
// Creates a 1x1 uncompressed texture containing the specified color.
//--------------------------------------------------------------------------------------
VOID CreateSolidTexture( ID3D11Device* pd3dDevice, DWORD ColorRGBA, ID3D11Texture2D** ppTexture2D, ID3D11ShaderResourceView** ppSRV )
{
    D3D11_TEXTURE2D_DESC Tex2DDesc = { 0 };
    Tex2DDesc.Width = 1;
    Tex2DDesc.Height = 1;
    Tex2DDesc.ArraySize = 1;
    Tex2DDesc.SampleDesc.Count = 1;
    Tex2DDesc.SampleDesc.Quality = 0;
    Tex2DDesc.MipLevels = 1;
    Tex2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Tex2DDesc.Usage = D3D11_USAGE_DEFAULT;
    Tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA TexData = { 0 };
    TexData.pSysMem = &ColorRGBA;
    TexData.SysMemPitch = 4;
    TexData.SysMemSlicePitch = 4;

    HRESULT hr = pd3dDevice->CreateTexture2D( &Tex2DDesc, &TexData, ppTexture2D );
    assert( SUCCEEDED( hr ) );

    hr = pd3dDevice->CreateShaderResourceView( *ppTexture2D, nullptr, ppSRV );
    assert( SUCCEEDED( hr ) );

#if defined(DEBUG) || defined(PROFILE)
    char clr[16];
    sprintf_s( clr, sizeof(clr), "CLR: %x", ColorRGBA );
    DXUT_SetDebugName( *ppTexture2D, clr );
    DXUT_SetDebugName( *ppSRV, clr );
#endif
}

//--------------------------------------------------------------------------------------
// Creates three default textures to be used to replace missing content in the mesh file.
//--------------------------------------------------------------------------------------
VOID CreateDefaultTextures( ID3D11Device* pd3dDevice )
{
    if( !g_pDefaultDiffuseTexture )
    {
        CreateSolidTexture( pd3dDevice, 0xFF808080, &g_pDefaultDiffuseTexture, &g_pDefaultDiffuseSRV );
    }
    if( !g_pDefaultNormalTexture )
    {
        CreateSolidTexture( pd3dDevice, 0x80FF8080, &g_pDefaultNormalTexture, &g_pDefaultNormalSRV );
    }
    if( !g_pDefaultSpecularTexture )
    {
        CreateSolidTexture( pd3dDevice, 0xFF000000, &g_pDefaultSpecularTexture, &g_pDefaultSpecularSRV );
    }
}

//--------------------------------------------------------------------------------------
// This callback is used by the SDKMESH loader to create vertex buffers.  The callback
// creates each buffer as a shader resource, so it can be bound as a shader resource as
// well as a VB.
//--------------------------------------------------------------------------------------
VOID CALLBACK CreateVertexBufferAndShaderResource( ID3D11Device* pDev, ID3D11Buffer** ppBuffer,
                                                   D3D11_BUFFER_DESC BufferDesc, void* pData, void* pContext )
{
    UNREFERENCED_PARAMETER(pContext);

    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pData;
    HRESULT hr = pDev->CreateBuffer( &BufferDesc, &InitData, ppBuffer );
    assert( SUCCEEDED( hr ) );
    hr;

#if defined(DEBUG) || defined(PROFILE)
    if ( *ppBuffer )
    {
        CHAR strFileA[MAX_PATH];
        WideCharToMultiByte( CP_ACP, 0, (const WCHAR*)pContext, -1, strFileA, MAX_PATH, nullptr, FALSE );
        CHAR* pstrName = strrchr( strFileA, '\\' );
        if( !pstrName )
            pstrName = strFileA;
        else
            pstrName++;
        
        DXUT_SetDebugName( *ppBuffer, pstrName );
    }
#endif
}

ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = nullptr;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    if ( SUCCEEDED(pDevice->CreateBuffer(&desc, nullptr, &debugbuf) ) )
    {
        DXUT_SetDebugName( debugbuf, "Debug" );

        pd3dImmediateContext->CopyResource( debugbuf, pBuffer );
    }

    return debugbuf;
}

//--------------------------------------------------------------------------------------
// Loads a specially constructed SDKMESH file from disk.  This SDKMESH file contains a
// preprocessed Catmull-Clark subdivision surface, complete with topology and adjacency
// data, as well as the typical mesh vertex data.
//--------------------------------------------------------------------------------------
HRESULT CSubDMesh::LoadSubDFromSDKMesh( ID3D11Device* pd3dDevice, const WCHAR* strFileName, const WCHAR* strAnimationFileName, const CHAR* strCameraName )
{
    WCHAR wstr[MAX_PATH];
    HRESULT hr;

    // Find the file
    V_RETURN( DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strFileName ) );

    SDKMESH_CALLBACKS11 SubDLoaderCallbacks = { 0 };
    SubDLoaderCallbacks.pCreateVertexBuffer = (LPCREATEVERTEXBUFFER11)CreateVertexBufferAndShaderResource;
    SubDLoaderCallbacks.pContext = (void*)strFileName;

    m_pMeshFile = new CDXUTSDKMesh();
    assert( m_pMeshFile != nullptr );

    // Load the file
    V_RETURN( m_pMeshFile->Create( pd3dDevice, wstr, &SubDLoaderCallbacks ) );

    // Load the animation file
    if( wcslen( strAnimationFileName ) > 0 )
    {
        hr = DXUTFindDXSDKMediaFileCch( wstr, MAX_PATH, strAnimationFileName );
        if( SUCCEEDED( hr ) )
        {
            V_RETURN( m_pMeshFile->LoadAnimation( wstr ) );
        }
    }

    UINT MeshCount = m_pMeshFile->GetNumMeshes();

    if( MeshCount == 0 )
        return E_FAIL;

    const UINT FrameCount = m_pMeshFile->GetNumFrames();

    // Find camera frame
    m_iCameraFrameIndex = INVALID_FRAME;
    for( UINT i = 0; i < FrameCount; ++i )
    {
        auto pFrame = m_pMeshFile->GetFrame( i );
        if( _stricmp( pFrame->Name, strCameraName ) == 0 )
        {
            m_iCameraFrameIndex = i;
        }
    }

    static UINT nRegular = 0;
    static UINT nHighestVal = 0;
    static UINT nLowestVal = 100;
    static UINT nPatches = 0;
    static UINT nSubsets = 0;

    // Load mesh pieces
    for( UINT i = 0; i < MeshCount; ++i )
    {
        auto pMesh = m_pMeshFile->GetMesh( i );
        assert( pMesh != nullptr );

        nSubsets += pMesh->NumSubsets;

        if( pMesh->NumVertexBuffers == 1 )
        {
            PolyMeshPiece* pPolyMeshPiece = new PolyMeshPiece;
            ZeroMemory( pPolyMeshPiece, sizeof( PolyMeshPiece ) );
            pPolyMeshPiece->m_pMesh = pMesh;
            pPolyMeshPiece->m_MeshIndex = i;

            pPolyMeshPiece->m_pIndexBuffer = m_pMeshFile->GetIB11( pMesh->IndexBuffer );
            pPolyMeshPiece->m_pVertexBuffer = m_pMeshFile->GetVB11( i, 0 );

            // Find frame that corresponds to this mesh
            pPolyMeshPiece->m_iFrameIndex = -1;
            for( UINT j = 0; j < FrameCount; ++j )
            {
                auto pFrame = m_pMeshFile->GetFrame( j );
                if( pFrame->Mesh == pPolyMeshPiece->m_MeshIndex )
                {
                    pPolyMeshPiece->m_iFrameIndex = (INT)j;
                }
            }

            pPolyMeshPiece->m_vCenter = pMesh->BoundingBoxCenter;
            pPolyMeshPiece->m_vExtents = pMesh->BoundingBoxExtents;

            m_PolyMeshPieces.push_back( pPolyMeshPiece );
        }
        else
        {
            // SubD meshes have 2 vertex buffers: a control point VB and a patch data VB
            assert( pMesh->NumVertexBuffers == 2 );
            // Make sure the control point VB has the correct stride
            assert( m_pMeshFile->GetVertexStride( i, 0 ) == sizeof( SUBD_CONTROL_POINT ) );
            // Make sure we have at least one subset
            assert( m_pMeshFile->GetNumSubsets( i ) > 0 );
            // Make sure the first subset is made up of quad patches
            assert( m_pMeshFile->GetSubset( i, 0 )->PrimitiveType == PT_QUAD_PATCH_LIST );
            // Make sure the IB is a multiple of the max point size
            assert( m_pMeshFile->GetNumIndices( i ) % MAX_EXTRAORDINARY_POINTS == 0 );

            // Create a new mesh piece and fill it in with all of the buffer pointers
            auto pPatchPiece = new PatchPiece;
            ZeroMemory( pPatchPiece, sizeof( PatchPiece ) );
            pPatchPiece->m_pMesh = pMesh;
            pPatchPiece->m_MeshIndex = i;

            pPatchPiece->m_pExtraordinaryPatchIB = m_pMeshFile->GetIB11( pMesh->IndexBuffer );
            pPatchPiece->m_pControlPointVB = m_pMeshFile->GetVB11( i, 0 );
            pPatchPiece->m_pPerPatchDataVB = m_pMeshFile->GetVB11( i, 1 );

            INT iNumPatches = (int)m_pMeshFile->GetNumIndices( i ) / MAX_EXTRAORDINARY_POINTS;
            pPatchPiece->m_iPatchCount = iNumPatches;            

            struct PatchData 
            {
                BYTE val[4]; // Valence of this patch
                BYTE pre[4]; // Prefix of this patch
            }* pPatchData;
            pPatchData = (PatchData*)m_pMeshFile->GetRawVerticesAt( pMesh->VertexBuffers[1] ); // This is the same data as what's in pPatchPiece->m_pPerPatchDataVB
            
            nPatches += iNumPatches;

            pPatchPiece->m_iRegularExtraodinarySplitPoint = -1;

            // Loop through all patches inside this patch piece (patch piece is one mesh inside our sdkmesh) to get some statistical data
            for ( INT k = 0; k < iNumPatches; ++k ) 
            {
                // How many regular patches do we have?
                if ( pPatchData[k].val[0] == 4 && pPatchData[k].val[1] == 4 && pPatchData[k].val[2] == 4 && pPatchData[k].val[3] == 4 )
                    ++nRegular;

                // What's the highest and lowest valence?
                for ( INT j = 0; j < 4; ++j )
                {
                    if ( pPatchData[k].val[j] > nHighestVal )
                        nHighestVal = pPatchData[k].val[0];

                    if ( pPatchData[k].val[j] < nLowestVal )
                        nLowestVal = pPatchData[k].val[j];
                }
                
                // Met with the first patch which is extraordinary?
                if ( (pPatchData[k].val[0] != 4 || pPatchData[k].val[1] != 4 || pPatchData[k].val[2] != 4 || pPatchData[k].val[3] != 4) && 
                     pPatchPiece->m_iRegularExtraodinarySplitPoint == -1 )
                {
                    pPatchPiece->m_iRegularExtraodinarySplitPoint = i;
                }

                // Ensure that all patches after the regular-extraordinary split point are extraordinary patches
                if ( pPatchPiece->m_iRegularExtraodinarySplitPoint > 0 )
                {
                    assert( pPatchData[k].val[0] != 4 || pPatchData[k].val[1] != 4 || pPatchData[k].val[2] != 4 || pPatchData[k].val[3] != 4 );
                }
            }            

            
            UINT *pIdx = (UINT*)m_pMeshFile->GetRawIndicesAt( pMesh->IndexBuffer ); // this is the same data as what's in pPatchPiece->m_pExtraordinaryPatchIB//MappedResource.pData;
            

            std::vector<UINT> vRegularIdxBuf;
            std::vector<UINT> vExtraordinaryIdxBuf;
            std::vector<PatchData> vRegularPatchData;
            std::vector<PatchData> vExtraordinaryPatchData;

            UINT nNumSub = m_pMeshFile->GetNumSubsets( pPatchPiece->m_MeshIndex );
            for ( UINT p = 0; p < nNumSub; ++p ) // loop through all subsets inside this patch piece
            {
                auto pSubset = m_pMeshFile->GetSubset( pPatchPiece->m_MeshIndex, p );

                UINT NumIndices = (UINT)pSubset->IndexCount; // this is actually the number of patches in current subset
                UINT StartIndex = (UINT)pSubset->IndexStart; // this is actually the patch start index of current subset in patch data
                
                pPatchPiece->RegularPatchStart.push_back( (UINT)vRegularPatchData.size() );
                pPatchPiece->ExtraordinaryPatchStart.push_back( (UINT)vExtraordinaryPatchData.size() );
                
                for ( UINT j = 0; j < NumIndices; ++j ) // loop through all patches inside this subset
                {
                    if ( pPatchData[j+StartIndex].val[0] == 4 && pPatchData[j+StartIndex].val[1] == 4 &&
                         pPatchData[j+StartIndex].val[2] == 4 && pPatchData[j+StartIndex].val[3] == 4 )
                    {
                        // this patch is regular
                        for ( INT k = 0; k < MAX_EXTRAORDINARY_POINTS; ++k )
                            vRegularIdxBuf.push_back( pIdx[(StartIndex + j) * MAX_EXTRAORDINARY_POINTS + k] );

                        vRegularPatchData.push_back( pPatchData[j+StartIndex] );
                    } else
                    {
                        // this patch is extraordinary
                        for ( INT k = 0; k < MAX_EXTRAORDINARY_POINTS; ++k )
                            vExtraordinaryIdxBuf.push_back( pIdx[(StartIndex + j) * MAX_EXTRAORDINARY_POINTS + k] );

                        vExtraordinaryPatchData.push_back( pPatchData[j+StartIndex] );
                    }
                }

                pPatchPiece->RegularPatchCount.push_back( 
                    UINT( vRegularPatchData.size() - pPatchPiece->RegularPatchStart[pPatchPiece->RegularPatchStart.size()-1] ) );
                pPatchPiece->ExtraordinaryPatchCount.push_back( 
                    UINT( vExtraordinaryPatchData.size() - pPatchPiece->ExtraordinaryPatchStart[pPatchPiece->ExtraordinaryPatchStart.size()-1] ) );
            }

            // Create index buffer for the regular patches
            D3D11_SUBRESOURCE_DATA initdata;
            initdata.pSysMem = &vRegularIdxBuf[0];
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth = UINT( vRegularIdxBuf.size() * sizeof(UINT) );
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;            
            DXUTGetD3D11Device()->CreateBuffer( &desc, &initdata, &pPatchPiece->m_pMyRegularPatchIB );
            DXUT_SetDebugName( pPatchPiece->m_pMyRegularPatchIB, "CSubDMesh IB" );

            // Create index buffer for the extraordinary patches
            desc.ByteWidth = UINT( vExtraordinaryIdxBuf.size() * sizeof(UINT) );
            initdata.pSysMem = &vExtraordinaryIdxBuf[0];
            DXUTGetD3D11Device()->CreateBuffer( &desc, &initdata, &pPatchPiece->m_pMyExtraordinaryPatchIB );
            DXUT_SetDebugName( pPatchPiece->m_pMyExtraordinaryPatchIB, "CSubDMesh Xord IB" );

            // Create per-patch data buffer for regular patches
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.ByteWidth = UINT( vRegularPatchData.size() * sizeof(PatchData) );
            initdata.pSysMem = &vRegularPatchData[0];
            DXUTGetD3D11Device()->CreateBuffer( &desc, &initdata, &pPatchPiece->m_pMyRegularPatchData );
            DXUT_SetDebugName( pPatchPiece->m_pMyRegularPatchData, "CSubDMesh PerPatch" );

            // Create per-patch data buffer for extraordinary patches
            desc.ByteWidth = UINT( vExtraordinaryPatchData.size() * sizeof(PatchData) );
            initdata.pSysMem = &vExtraordinaryPatchData[0];
            DXUTGetD3D11Device()->CreateBuffer( &desc, &initdata, &pPatchPiece->m_pMyExtraordinaryPatchData );
            DXUT_SetDebugName( pPatchPiece->m_pMyExtraordinaryPatchData, "CSubDMesh Xord PerPatch" );
            
            // Create a SRV for the per-patch data            
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            SRVDesc.Buffer.ElementOffset = 0;
            SRVDesc.Buffer.ElementWidth = iNumPatches * 2;
            V_RETURN( pd3dDevice->CreateShaderResourceView( pPatchPiece->m_pPerPatchDataVB, &SRVDesc, &pPatchPiece->m_pPerPatchDataSRV ) );
            DXUT_SetDebugName( pPatchPiece->m_pPerPatchDataSRV, "CSubDMesh PatchVB SRV" );

            // Create SRV for regular per-patch data
            SRVDesc.Buffer.FirstElement = 0;
            SRVDesc.Buffer.NumElements = UINT( vRegularPatchData.size() * 2 );
            V_RETURN( pd3dDevice->CreateShaderResourceView( pPatchPiece->m_pMyRegularPatchData, &SRVDesc, &pPatchPiece->m_pMyRegularPatchDataSRV ) );
            DXUT_SetDebugName( pPatchPiece->m_pMyRegularPatchDataSRV, "CSubDMesh PerPatch SRV" );

            // Create SRV for extraordinary per-patch data
            SRVDesc.Buffer.NumElements = UINT( vExtraordinaryPatchData.size() * 2 );
            V_RETURN( pd3dDevice->CreateShaderResourceView( pPatchPiece->m_pMyExtraordinaryPatchData, &SRVDesc, &pPatchPiece->m_pMyExtraordinaryPatchDataSRV ) );
            DXUT_SetDebugName( pPatchPiece->m_pMyExtraordinaryPatchDataSRV, "CSubDMesh Xord PerPatch SRV" );

            pPatchPiece->m_vCenter = pMesh->BoundingBoxCenter;
            pPatchPiece->m_vExtents = pMesh->BoundingBoxExtents;

            // Find frame that corresponds to this mesh
            pPatchPiece->m_iFrameIndex = -1;
            for( UINT j = 0; j < FrameCount; ++j )
            {
                auto pFrame = m_pMeshFile->GetFrame( j );
                if( pFrame->Mesh == pPatchPiece->m_MeshIndex )
                {
                    pPatchPiece->m_iFrameIndex = (INT)j;
                }
            }

            m_PatchPieces.push_back( pPatchPiece );
        }
    }

    CreateDefaultTextures( pd3dDevice );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_PER_SUBSET_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &m_pPerSubsetCB ) );
    DXUT_SetDebugName( m_pPerSubsetCB, "CSubDMesh CB_PER_SUBSET_CONSTANTS" );

    // Create bind pose
    XMMATRIX matIdentity = XMMatrixIdentity();
    m_pMeshFile->TransformBindPose( matIdentity );
    Update( matIdentity, 0 );


    return S_OK;
}

//--------------------------------------------------------------------------------------
CSubDMesh::~CSubDMesh()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
void CSubDMesh::Update( CXMMATRIX mWorld, double fTime )
{
    m_pMeshFile->TransformMesh( mWorld, fTime );
}

//--------------------------------------------------------------------------------------
bool CSubDMesh::GetCameraViewMatrix( XMMATRIX* pViewMatrix, XMVECTOR* pCameraPosWorld )
{
    if( m_iCameraFrameIndex == INVALID_FRAME || !pViewMatrix || !pCameraPosWorld )
    {
        return false;
    }

    XMMATRIX matRotation = XMMatrixRotationY( XM_PI * 0.5f );

    XMMATRIX mCameraWorld = m_pMeshFile->GetWorldMatrix( m_iCameraFrameIndex );
    *pCameraPosWorld = mCameraWorld.r[3];

    XMMATRIX matCamera = matRotation * mCameraWorld;
    
    *pViewMatrix = XMMatrixInverse( nullptr, matCamera );

    return true;
}

//--------------------------------------------------------------------------------------
FLOAT CSubDMesh::GetAnimationDuration()
{
    UINT iKeyCount = 0;
    FLOAT fFrameTime = 0.0f;
    bool bAnimationPresent = m_pMeshFile->GetAnimationProperties( &iKeyCount, &fFrameTime );
    if( !bAnimationPresent )
    {
        return 0.0f;
    }

    return (FLOAT)iKeyCount * fFrameTime;
}

//--------------------------------------------------------------------------------------
void CSubDMesh::GetBounds( XMVECTOR* pvCenter, XMVECTOR* pvExtents ) const
{
    assert( pvCenter != nullptr && pvExtents != nullptr );

    BoundingBox box( XMFLOAT3( 0, 0, 0 ), XMFLOAT3( 0, 0, 0 ) );

    INT iCount = GetNumPatchPieces();
    for( INT i = 0; i < iCount; ++i )
    {
        const PatchPiece& Piece = *m_PatchPieces[i];

        XMMATRIX matTransform= GetPatchPieceTransform( i );

        XMVECTOR vPieceCenter = XMLoadFloat3( &Piece.m_vCenter );
        XMVECTOR vPieceExtents = XMLoadFloat3( &Piece.m_vExtents );

        vPieceCenter = XMVector3TransformCoord( vPieceCenter, matTransform );
        vPieceExtents = XMVector3TransformNormal( vPieceExtents, matTransform );

        BoundingBox box2;
        XMStoreFloat3( &box2.Center, vPieceCenter );
        XMStoreFloat3( &box2.Extents, vPieceExtents );

        BoundingBox::CreateMerged( box, box, box2 );
    }

    iCount = GetNumPolyMeshPieces();
    for( INT i = 0; i < iCount; ++i )
    {
        const PolyMeshPiece& Piece = *m_PolyMeshPieces[i];
        XMMATRIX matTransform = GetPolyMeshPieceTransform( i );

        XMVECTOR vPieceCenter = XMLoadFloat3( &Piece.m_vCenter );
        XMVECTOR vPieceExtents = XMLoadFloat3( &Piece.m_vExtents );

        vPieceCenter = XMVector3TransformCoord( vPieceCenter, matTransform );
        vPieceExtents = XMVector3TransformNormal( vPieceExtents, matTransform );

        BoundingBox box2;
        XMStoreFloat3( &box2.Center, vPieceCenter );
        XMStoreFloat3( &box2.Extents, vPieceExtents );

        BoundingBox::CreateMerged( box, box, box2 );
    }

    *pvCenter = XMLoadFloat3( &box.Center );
    *pvExtents = XMLoadFloat3( &box.Extents );
}

//--------------------------------------------------------------------------------------
// Renders a single mesh from the SDKMESH data.  Each mesh "piece" is a separate mesh
// within the file, with its own VB and IB buffers.
//--------------------------------------------------------------------------------------

// This only renders the regular patches of the mesh piece
void CSubDMesh::RenderPatchPiece_OnlyRegular( ID3D11DeviceContext* pd3dDeviceContext, int PieceIndex )
{
    PatchPiece* pPiece = m_PatchPieces[PieceIndex];
    assert( pPiece != nullptr );

    // Set the input assembler
    pd3dDeviceContext->IASetIndexBuffer( pPiece->m_pMyRegularPatchIB, DXGI_FORMAT_R32_UINT, 0 );
    UINT Stride = sizeof( SUBD_CONTROL_POINT );
    UINT Offset = 0;
    pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pPiece->m_pControlPointVB, &Stride, &Offset );

    pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST );

    // Bind the per-patch data
    pd3dDeviceContext->HSSetShaderResources( 0, 1, &pPiece->m_pMyRegularPatchDataSRV );

    // Loop through the mesh subsets
    int SubsetCount = m_pMeshFile->GetNumSubsets( pPiece->m_MeshIndex );
    for( int i = 0; i < SubsetCount; ++i )
    {
        auto pSubset = m_pMeshFile->GetSubset( pPiece->m_MeshIndex, i );
        if( pSubset->PrimitiveType != PT_QUAD_PATCH_LIST )
            continue;        

        // Set per-subset constant buffer, so the hull shader references the proper index in the per-patch data
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( m_pPerSubsetCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_PER_SUBSET_CONSTANTS* pData = (CB_PER_SUBSET_CONSTANTS*)MappedResource.pData;
        pData->m_iPatchStartIndex = (int)pPiece->RegularPatchStart[i];
        pd3dDeviceContext->Unmap( m_pPerSubsetCB, 0 );
        pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerSubset, 1, &m_pPerSubsetCB );

        // Set up the material for this subset
        SetupMaterial( pd3dDeviceContext, pSubset->MaterialID );

        // Draw      
        UINT NumIndices = (UINT)pPiece->RegularPatchCount[i] * MAX_EXTRAORDINARY_POINTS;
        UINT StartIndex = (UINT)pPiece->RegularPatchStart[i] * MAX_EXTRAORDINARY_POINTS;        
        pd3dDeviceContext->DrawIndexed( NumIndices, StartIndex, 0 );
    }
}

// This only renders the extraordinary patches of the mesh piece
void CSubDMesh::RenderPatchPiece_OnlyExtraordinary( ID3D11DeviceContext* pd3dDeviceContext, int PieceIndex )
{
    PatchPiece* pPiece = m_PatchPieces[PieceIndex];
    assert( pPiece != nullptr );

    // Set the input assembler
    pd3dDeviceContext->IASetIndexBuffer( pPiece->m_pMyExtraordinaryPatchIB, DXGI_FORMAT_R32_UINT, 0 );
    UINT Stride = sizeof( SUBD_CONTROL_POINT );
    UINT Offset = 0;
    pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pPiece->m_pControlPointVB, &Stride, &Offset );

    pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST );

    // Bind the per-patch data
    pd3dDeviceContext->HSSetShaderResources( 0, 1, &pPiece->m_pMyExtraordinaryPatchDataSRV );

    // Loop through the mesh subsets
    int SubsetCount = m_pMeshFile->GetNumSubsets( pPiece->m_MeshIndex );
    for( int i = 0; i < SubsetCount; ++i )
    {
        auto pSubset = m_pMeshFile->GetSubset( pPiece->m_MeshIndex, i );
        if( pSubset->PrimitiveType != PT_QUAD_PATCH_LIST )
            continue;        

        // Set per-subset constant buffer, so the hull shader references the proper index in the per-patch data
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( m_pPerSubsetCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_PER_SUBSET_CONSTANTS* pData = (CB_PER_SUBSET_CONSTANTS*)MappedResource.pData;
        pData->m_iPatchStartIndex = (int)pPiece->ExtraordinaryPatchStart[i];
        pd3dDeviceContext->Unmap( m_pPerSubsetCB, 0 );
        pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerSubset, 1, &m_pPerSubsetCB );

        // Set up the material for this subset
        SetupMaterial( pd3dDeviceContext, pSubset->MaterialID );

        // Draw       
        UINT NumIndices = (UINT)pPiece->ExtraordinaryPatchCount[i] * MAX_EXTRAORDINARY_POINTS;
        UINT StartIndex = (UINT)pPiece->ExtraordinaryPatchStart[i] * MAX_EXTRAORDINARY_POINTS;        
        pd3dDeviceContext->DrawIndexed( NumIndices, StartIndex, 0 );
    }
}

void CSubDMesh::RenderPolyMeshPiece( ID3D11DeviceContext* pd3dDeviceContext, int PieceIndex )
{
    PolyMeshPiece* pPiece = m_PolyMeshPieces[PieceIndex];
    assert( pPiece != nullptr );

    pd3dDeviceContext->IASetIndexBuffer( pPiece->m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );
    UINT Stride = sizeof( SUBD_CONTROL_POINT );
    UINT Offset = 0;
    pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pPiece->m_pVertexBuffer, &Stride, &Offset );
    pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    int SubsetCount = m_pMeshFile->GetNumSubsets( pPiece->m_MeshIndex );
    for( int i = 0; i < SubsetCount; ++i )
    {
        auto pSubset = m_pMeshFile->GetSubset( pPiece->m_MeshIndex, i );
        if( pSubset->PrimitiveType != PT_TRIANGLE_LIST )
            continue;

        SetupMaterial( pd3dDeviceContext, pSubset->MaterialID );

        pd3dDeviceContext->DrawIndexed( (UINT)pSubset->IndexCount, (UINT)pSubset->IndexStart, 0 );
    }
}

//--------------------------------------------------------------------------------------
// Sets the specified material parameters (textures) into the D3D device.
//--------------------------------------------------------------------------------------
void CSubDMesh::SetupMaterial( ID3D11DeviceContext* pd3dDeviceContext, int MaterialID )
{
    auto pMaterial = m_pMeshFile->GetMaterial( MaterialID );

    ID3D11ShaderResourceView* Resources[] = { pMaterial->pNormalRV11, pMaterial->pDiffuseRV11, pMaterial->pSpecularRV11 };
    if( Resources[0] == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
    {
        Resources[0] = g_pDefaultNormalSRV;
    }
    if( Resources[1] == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
    {
        Resources[1] = g_pDefaultDiffuseSRV;
    }
    if( Resources[2] == (ID3D11ShaderResourceView*)ERROR_RESOURCE_VALUE )
    {
        Resources[2] = g_pDefaultSpecularSRV;
    }
    
    // The domain shader only needs the heightmap, so we only set 1 slot here.
    pd3dDeviceContext->DSSetShaderResources( 0, 1, Resources );

    // The pixel shader samples from all 3 textures.
    pd3dDeviceContext->PSSetShaderResources( 0, 3, Resources );
}


//--------------------------------------------------------------------------------------
void CSubDMesh::Destroy()
{
    int PieceCount = GetNumPatchPieces();
    for( int i = 0; i < PieceCount; ++i )
    {
        PatchPiece* pPiece = m_PatchPieces[i];

        SAFE_RELEASE( pPiece->m_pPerPatchDataSRV );

        SAFE_RELEASE( pPiece->m_pMyExtraordinaryPatchData );
        SAFE_RELEASE( pPiece->m_pMyExtraordinaryPatchDataSRV );
        SAFE_RELEASE( pPiece->m_pMyRegularPatchData );
        SAFE_RELEASE( pPiece->m_pMyRegularPatchDataSRV );
        SAFE_RELEASE( pPiece->m_pMyRegularPatchIB );
        SAFE_RELEASE( pPiece->m_pMyExtraordinaryPatchIB );

        delete pPiece;
    }
    m_PatchPieces.clear();

    PieceCount = GetNumPolyMeshPieces();
    for ( int i = 0; i < PieceCount; ++i )
    {
        SAFE_DELETE( m_PolyMeshPieces[i] );
    }
    m_PolyMeshPieces.clear();

    SAFE_RELEASE( m_pPerSubsetCB );

    SAFE_RELEASE( g_pDefaultDiffuseSRV );
    SAFE_RELEASE( g_pDefaultDiffuseTexture );
    SAFE_RELEASE( g_pDefaultNormalSRV );
    SAFE_RELEASE( g_pDefaultNormalTexture );
    SAFE_RELEASE( g_pDefaultSpecularSRV );
    SAFE_RELEASE( g_pDefaultSpecularTexture );

    if( m_pMeshFile )
    {
        m_pMeshFile->Destroy();
        SAFE_DELETE( m_pMeshFile );
    }
}
