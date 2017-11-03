//--------------------------------------------------------------------------------------
// SampleAPOBase.h
//
// Example custom xAPO template for XAudio2
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include <crtdbg.h>

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <xapobase.h>
#else
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xapobase.h>
#endif

#pragma warning(push)
#pragma warning(disable : 4481)
// VS 2010 considers 'override' to be a extension, but it's part of C++11 as of VS 2012

//--------------------------------------------------------------------------------------
// CSampleXAPOBase
//
// Template class that provides a default class factory implementation and typesafe
// parameter passing for our sample xAPO classes
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
class CSampleXAPOBase
    : public CXAPOParametersBase
{
public:
    static HRESULT CreateInstance( void* pInitData, UINT32 cbInitData, APOClass** ppAPO );

    //
    // IXAPO
    //
    STDMETHOD(LockForProcess) (
        UINT32 InputLockedParameterCount,
        _In_reads_opt_(InputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
        UINT32 OutputLockedParameterCount,
        _In_reads_opt_(OutputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters ) override;

    STDMETHOD_(void, Process) (
        UINT32 InputProcessParameterCount,
        _In_reads_opt_(InputProcessParameterCount) const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters,
        UINT32 OutputProcessParameterCount,
        _Inout_updates_opt_(OutputProcessParameterCount) XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters,
        BOOL IsEnabled) override;

protected:
    CSampleXAPOBase();
    ~CSampleXAPOBase(void);

    //
    // Accessors
    //
    const WAVEFORMATEX& WaveFormat() const { return m_wfx; }

    //
    // Overrides
    //
    void OnSetParameters (_In_reads_bytes_(cbParams) const void* pParams, UINT32 cbParams) override
    {
        _ASSERT( cbParams == sizeof( ParameterClass ) );
        cbParams;
        OnSetParameters( *(ParameterClass*)pParams );
    }

    //
    // Overridables
    //

    // Process a frame of audio. Marked pure virtual because without
    // this function there's not much point in having an xAPO.
    virtual void DoProcess(
        const ParameterClass& params,
        _Inout_updates_all_(cFrames * cChannels) FLOAT32* __restrict pData,
        UINT32 cFrames,
        UINT32 cChannels ) = 0;

    // Do any necessary calculations in response to parameter changes.
    // NOT marked pure virtual because there may not be a reason to
    // do additional calculations when parameters are set.
    virtual void OnSetParameters( const ParameterClass& ){}

private:
    // Ring buffer needed by CXAPOParametersBase
    ParameterClass  m_parameters[3];

    // Format of the audio we're processing
    WAVEFORMATEX    m_wfx;

    // Registration properties defining this xAPO class.
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
};


//--------------------------------------------------------------------------------------
// CSampleAPOBase::m_regProps
//
// Registration properties for the sample xAPO classes.
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
__declspec(selectany) XAPO_REGISTRATION_PROPERTIES CSampleXAPOBase<APOClass, ParameterClass>::m_regProps = {
            __uuidof(APOClass),
            L"SampleAPO",
            L"Copyright (C)2008 Microsoft Corporation",
            1,
            0,
            XAPO_FLAG_INPLACE_REQUIRED
            | XAPO_FLAG_CHANNELS_MUST_MATCH
            | XAPO_FLAG_FRAMERATE_MUST_MATCH
            | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH
            | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH
            | XAPO_FLAG_INPLACE_SUPPORTED,
            1, 1, 1, 1 };


//--------------------------------------------------------------------------------------
// Name: CSampleXAPOBase::CreateInstance
// Desc: Class factory for sample xAPO objects
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
HRESULT CSampleXAPOBase<APOClass, ParameterClass>::CreateInstance( void* pInitData, UINT32 cbInitData, APOClass** ppAPO )
{
    _ASSERT( ppAPO );
    HRESULT hr = S_OK;

    *ppAPO = new APOClass;
    if ( *ppAPO != NULL )
    {
        hr = (*ppAPO)->Initialize( pInitData, cbInitData );
    }
    else
        hr = E_OUTOFMEMORY;

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CSampleXAPOBase::CSampleXAPOBase
// Desc: Constructor
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
CSampleXAPOBase<APOClass, ParameterClass>::CSampleXAPOBase( )
: CXAPOParametersBase( &m_regProps, (BYTE*)m_parameters, sizeof( ParameterClass ), FALSE )
{
#ifdef _XBOX_VER
    XMemSet( (BYTE*)m_parameters, 0, sizeof( m_parameters ) );
#else
    ZeroMemory( m_parameters, sizeof( m_parameters ) );
#endif
}


//--------------------------------------------------------------------------------------
// Name: CSampleXAPOBase::~CSampleXAPOBase
// Desc: Destructor
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
CSampleXAPOBase<APOClass, ParameterClass>::~CSampleXAPOBase()
{

}

//--------------------------------------------------------------------------------------
// Name: CSampleXAPOBase::LockForProcess
// Desc: Overridden so that we can remember the wave format of the signal
//       we're supposed to be processing
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
HRESULT CSampleXAPOBase<APOClass, ParameterClass>::LockForProcess(
    UINT32 InputLockedParameterCount,
    const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
    UINT32 OutputLockedParameterCount,
    const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters )
{
    HRESULT hr = CXAPOParametersBase::LockForProcess(
        InputLockedParameterCount,
        pInputLockedParameters,
        OutputLockedParameterCount,
        pOutputLockedParameters );

    if( SUCCEEDED( hr ) )
    {
        if ( !pInputLockedParameters )
            return E_POINTER;

        // Copy the wave format. Note that we're using memcpy rather than XMemCpy
        // because XMemCpy is really only beneficial for larger blocks of memory.
        memcpy( &m_wfx, pInputLockedParameters[0].pFormat, sizeof( WAVEFORMATEX ) );
    }
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: CSampleXAPOBase::Process
// Desc: Overridden to call this class's typesafe version
//--------------------------------------------------------------------------------------
template<typename APOClass, typename ParameterClass>
void CSampleXAPOBase<APOClass, ParameterClass>::Process(
    UINT32 InputProcessParameterCount,
    const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters,
    UINT32 OutputProcessParameterCount,
    XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters,
    BOOL IsEnabled)
{
    _ASSERT( IsLocked() );
    _ASSERT( InputProcessParameterCount == 1 );
    _ASSERT( OutputProcessParameterCount == 1 );
    _ASSERT( pInputProcessParameters != 0 && pOutputProcessParameters != 0 );
    _Analysis_assume_( pInputProcessParameters != 0 && pOutputProcessParameters != 0 );
    _ASSERT( pInputProcessParameters[0].pBuffer == pOutputProcessParameters[0].pBuffer );

    UNREFERENCED_PARAMETER( OutputProcessParameterCount );
    UNREFERENCED_PARAMETER( InputProcessParameterCount );
    UNREFERENCED_PARAMETER( pOutputProcessParameters );
    UNREFERENCED_PARAMETER( IsEnabled );

    ParameterClass* pParams;
    pParams = (ParameterClass*)BeginProcess();
    if ( pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_SILENT )
    {
        memset( pInputProcessParameters[0].pBuffer, 0,
                pInputProcessParameters[0].ValidFrameCount * m_wfx.nChannels * sizeof(FLOAT32) );

        DoProcess(
            *pParams,
            (FLOAT32* __restrict)pInputProcessParameters[0].pBuffer,
            pInputProcessParameters[0].ValidFrameCount,
            m_wfx.nChannels );
    }
    else if( pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_VALID )
    {
        DoProcess(
            *pParams,
            (FLOAT32* __restrict)pInputProcessParameters[0].pBuffer,
            pInputProcessParameters[0].ValidFrameCount,
            m_wfx.nChannels );
    }
    EndProcess();
}

#pragma warning(pop)
