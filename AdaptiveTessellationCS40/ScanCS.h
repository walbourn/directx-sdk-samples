//--------------------------------------------------------------------------------------
// File: ScanCS.h
//
// A simple inclusive prefix sum(scan) implemented in CS4.0
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

class CScanCS
{
public:
    CScanCS();
    HRESULT OnD3D11CreateDevice( ID3D11Device* pd3dDevice );
    void OnD3D11DestroyDevice();

    // Both scan input and scanned output are in the buffer resource referred by p0SRV and p0UAV.
    // The buffer resource referred by p1SRV and p1UAV is used as intermediate result, 
    // and should be as large as the input/output buffer
    HRESULT ScanCS( ID3D11DeviceContext* pd3dImmediateContext,

                    // How many elements in the input buffer are to be scanned?
                    INT nNumToScan,

                    // SRV and UAV of the buffer which contains the input data,
                    // and the scanned result when the function returns
                    ID3D11ShaderResourceView* p0SRV,
                    ID3D11UnorderedAccessView* p0UAV,

                    // SRV and UAV of an aux buffer, which must be the same size as the input/output buffer
                    ID3D11ShaderResourceView* p1SRV,
                    ID3D11UnorderedAccessView* p1UAV );

private:
    ID3D11ComputeShader*        m_pScanCS;
    ID3D11ComputeShader*        m_pScan2CS;
    ID3D11ComputeShader*        m_pScan3CS;
    ID3D11Buffer*               m_pcbCS;

    ID3D11Buffer*               m_pAuxBuf;
    ID3D11ShaderResourceView*   m_pAuxBufRV;
    ID3D11UnorderedAccessView*  m_pAuxBufUAV;

    struct CB_CS
    {
        UINT param[4];
    };
};
