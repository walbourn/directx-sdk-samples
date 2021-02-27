//--------------------------------------------------------------------------------------
// File: RatingsDB.h
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#pragma once

class CRatingsDB
{
public:
    CRatingsDB();
    ~CRatingsDB();

    HRESULT LoadDB();
    HRESULT GetRatingSystemName( WCHAR* strRatingSystemGUID, WCHAR* strDest, int cchDest );
    HRESULT GetRatingIDName( WCHAR* strRatingSystemGUID, WCHAR* strRatingIDGUID, WCHAR* strDest, int cchDest );
    HRESULT GetDescriptorName( WCHAR* strRatingSystemGUID, WCHAR* strDescriptorGUID, WCHAR* strDest, int cchDest );

protected:
    HRESULT GetAttribFromNode( IXMLDOMNode* pNode, WCHAR* strAttrib, WCHAR* strDest, int cchDest );

    IXMLDOMNode* m_pRootNode;
    bool m_bCleanupCOM;
};
