//--------------------------------------------------------------------------------------
// File: WICImage.h
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <sal.h>

enum ImageContainer
{
    IMAGE_UNKNOWN = 0,
    IMAGE_BMP,
    IMAGE_PNG,
    IMAGE_ICO,
    IMAGE_JPEG,
    IMAGE_GIF,
    IMAGE_TIFF,
    IMAGE_WMP,
    IMAGE_NONE = 0xffff,
};

struct ImageInfo
{
    ImageContainer container;
    UINT height;
    UINT width;
    UINT bitDepth;
    GUID pixelFormat;
};

HRESULT GetImageInfoFromMemory( _In_bytecount_(wicDataSize) const BYTE* wicData, _In_ size_t wicDataSize, _Out_ ImageInfo& info );