//--------------------------------------------------------------------------------------
// File: WICImage.cpp
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4005)
#include <wincodec.h>
#pragma warning(pop)

#include "WICImage.h"

//---------------------------------------------------------------------------------
template<class T> class ScopedObject
{
public:
    explicit ScopedObject( T *p = 0 ) : _pointer(p) {}
    ~ScopedObject()
    {
        if ( _pointer )
        {
            _pointer->Release();
            _pointer = nullptr;
        }
    }

    bool IsNull() const { return (!_pointer); }

    T& operator*() { return *_pointer; }
    T* operator->() { return _pointer; }
    T** operator&() { return &_pointer; }

    void Reset(T *p = 0) { if ( _pointer ) { _pointer->Release(); } _pointer = p; }

    T* Get() const { return _pointer; }

private:
    ScopedObject(const ScopedObject&);
    ScopedObject& operator=(const ScopedObject&);
        
    T* _pointer;
};

//--------------------------------------------------------------------------------------
static IWICImagingFactory* _GetWIC()
{
    static IWICImagingFactory* s_Factory = nullptr;

    if ( s_Factory )
        return s_Factory;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory),
        (LPVOID*)&s_Factory
        );

    if ( FAILED(hr) )
    {
        s_Factory = nullptr;
        return nullptr;
    }

    return s_Factory;
}

//--------------------------------------------------------------------------------------
HRESULT GetImageInfoFromMemory( _In_bytecount_(wicDataSize) const BYTE* wicData, _In_ size_t wicDataSize, _Out_ ImageInfo& info )
{
    memset( &info, 0, sizeof(info) );

    if ( !wicData )
    {
        return E_INVALIDARG;
    }

    if ( !wicDataSize )
    {
        return E_FAIL;
    }

#ifdef _M_AMD64
    if ( wicDataSize > 0xFFFFFFFF )
        return HRESULT_FROM_WIN32( ERROR_FILE_TOO_LARGE );
#endif

    IWICImagingFactory* pWIC = _GetWIC();
    if ( !pWIC )
        return E_NOINTERFACE;

    // Create input stream for memory
    ScopedObject<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream( &stream );
    if ( FAILED(hr) )
        return hr;

    hr = stream->InitializeFromMemory( const_cast<BYTE*>( wicData ), static_cast<DWORD>( wicDataSize ) );
    if ( FAILED(hr) )
        return hr;

    // Initialize WIC
    ScopedObject<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream( stream.Get(), 0, WICDecodeMetadataCacheOnDemand, &decoder );
    if ( FAILED(hr) )
        return hr;

    GUID containerID;
    hr = decoder->GetContainerFormat( &containerID );
    if ( FAILED(hr) )
        return hr;

    if ( memcmp( &containerID, &GUID_ContainerFormatBmp, sizeof(GUID) ) == 0 )
        info.container = IMAGE_BMP;
    else if ( memcmp( &containerID, &GUID_ContainerFormatPng, sizeof(GUID) ) == 0 )
        info.container = IMAGE_PNG;
    else if ( memcmp( &containerID, &GUID_ContainerFormatIco, sizeof(GUID) ) == 0 )
        info.container = IMAGE_ICO;
    else if ( memcmp( &containerID, &GUID_ContainerFormatJpeg, sizeof(GUID) ) == 0 )
        info.container = IMAGE_JPEG;
    else if ( memcmp( &containerID, &GUID_ContainerFormatGif, sizeof(GUID) ) == 0 )
        info.container = IMAGE_GIF;
    else if ( memcmp( &containerID, &GUID_ContainerFormatTiff, sizeof(GUID) ) == 0 )
        info.container = IMAGE_TIFF;
    else if ( memcmp( &containerID, &GUID_ContainerFormatWmp, sizeof(GUID) ) == 0 )
        info.container = IMAGE_WMP;
    
    ScopedObject<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame( 0, &frame );
    if ( FAILED(hr) )
        return hr;

    UINT width, height;
    hr = frame->GetSize( &width, &height );
    if ( FAILED(hr) )
        return hr;

    info.width = width;
    info.height = height;

    WICPixelFormatGUID pixelFormat;
    hr = frame->GetPixelFormat( &pixelFormat );
    if ( FAILED(hr) )
        return hr;

    memcpy( &info.pixelFormat, &pixelFormat, sizeof(GUID) );
     
    ScopedObject<IWICComponentInfo> cinfo;
    if ( FAILED( pWIC->CreateComponentInfo( pixelFormat, &cinfo ) ) )
        return 0;

    WICComponentType type;
    if ( SUCCEEDED( cinfo->GetComponentType( &type ) ) )
    {
        if ( type == WICPixelFormat )
        {
            ScopedObject<IWICPixelFormatInfo> pfinfo;
            if ( SUCCEEDED( cinfo->QueryInterface( __uuidof(IWICPixelFormatInfo), reinterpret_cast<void**>( &pfinfo )  ) ) )
            {
                UINT bpp;
                if ( SUCCEEDED( pfinfo->GetBitsPerPixel( &bpp ) ) )
                    info.bitDepth = bpp;
            }
        }
    }

    return S_OK;
}
