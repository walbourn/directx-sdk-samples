//--------------------------------------------------------------------------------------
// File: RatingsDB.cpp
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=nullptr; } }
#endif    
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=nullptr; } }
#endif    
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=nullptr; } }
#endif
#define ID_RATINGS_XML 2

#include <windows.h>
#include <rpcsal.h>
#include <gameux.h>
#include <crtdbg.h>
#include <stdio.h>
#include <assert.h>
#include <shellapi.h>
#include <shlobj.h>
#include "RatingsDB.h"


//--------------------------------------------------------------------------------------
CRatingsDB::CRatingsDB(void)
{
    HRESULT hr = CoInitialize( 0 );
    m_bCleanupCOM = SUCCEEDED(hr); 
    m_pRootNode = nullptr;
}


//--------------------------------------------------------------------------------------
CRatingsDB::~CRatingsDB(void)
{
    SAFE_RELEASE( m_pRootNode );
    if( m_bCleanupCOM )
        CoUninitialize();
}


//--------------------------------------------------------------------------------------
HRESULT CRatingsDB::LoadDB()
{
    // Find resource will pick the right ID_GDF_XML_STR based on the current language
    HRSRC hrsrc = FindResource( nullptr, MAKEINTRESOURCE(ID_RATINGS_XML), L"DATA" ); 
    if( hrsrc ) 
    { 
        HGLOBAL hgResource = LoadResource( nullptr, hrsrc ); 
        if( hgResource ) 
        { 
            BYTE* pResourceBuffer = (BYTE*)LockResource( hgResource ); 
            if( pResourceBuffer ) 
            { 
                DWORD dwGDFXMLSize = SizeofResource( nullptr, hrsrc );
                if( dwGDFXMLSize )
                {
                    // HGLOBAL from LoadResource() needs to be copied for CreateStreamOnHGlobal() to work
                    HGLOBAL hgResourceCopy = GlobalAlloc( GMEM_MOVEABLE, dwGDFXMLSize );
                    if( hgResourceCopy )
                    {
                        LPVOID pCopy = GlobalLock( hgResourceCopy );
                        if( pCopy )
                        {
                            CopyMemory( pCopy, pResourceBuffer, dwGDFXMLSize );
                            GlobalUnlock( hgResource );

                            IStream* piStream = nullptr;
                            HRESULT hr = CreateStreamOnHGlobal( hgResourceCopy, TRUE, &piStream ); 
                            if( SUCCEEDED(hr) && piStream )
                            {
                                IXMLDOMDocument *pDoc = nullptr;

                                // Load the XML into a IXMLDOMDocument object
                                hr = CoCreateInstance( CLSID_DOMDocument, nullptr, CLSCTX_INPROC_SERVER, 
                                                       IID_IXMLDOMDocument, (void**)&pDoc );
                                if( SUCCEEDED(hr) ) 
                                {
                                    IPersistStreamInit* pPersistStreamInit = nullptr;
                                    hr = pDoc->QueryInterface( IID_IPersistStreamInit, (void**) &pPersistStreamInit );
                                    if( SUCCEEDED(hr) ) 
                                    {
                                        hr = pPersistStreamInit->Load( piStream );
                                        if( SUCCEEDED(hr) ) 
                                        {
                                            // Get the root node to the XML doc and store it 
                                            pDoc->QueryInterface( IID_IXMLDOMNode, (void**)&m_pRootNode );
                                        }
                                        SAFE_RELEASE( pPersistStreamInit );
                                    }
                                    SAFE_RELEASE( pDoc );
                                }
                                SAFE_RELEASE( piStream );
                            }
                        }
                        GlobalFree( hgResourceCopy );
                    }
                }
            } 
        } 
    } 

    if( m_pRootNode )
        return S_OK;
    else
        return E_FAIL;
}


//--------------------------------------------------------------------------------------
HRESULT CRatingsDB::GetRatingSystemName( WCHAR* strRatingSystemGUID, WCHAR* strDest, int cchDest )
{
    HRESULT hr;
    IXMLDOMNode* pNode = nullptr;
    WCHAR str[512];

    wcscpy_s( strDest, cchDest, strRatingSystemGUID );

    WCHAR strRatingSystemGUIDUpper[512];
    wcscpy_s( strRatingSystemGUIDUpper, 512, strRatingSystemGUID );
    _wcsupr_s( strRatingSystemGUIDUpper );

    swprintf_s( str, 512, L"//Ratings/RatingSystem[ @ID = \"%s\" ]", strRatingSystemGUIDUpper );    

    hr = m_pRootNode->selectSingleNode( str, &pNode );
    if( SUCCEEDED(hr) && pNode )
    {
        hr = GetAttribFromNode( pNode, L"Text", strDest, cchDest );
        SAFE_RELEASE( pNode );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CRatingsDB::GetRatingIDName( WCHAR* strRatingSystemGUID, WCHAR* strRatingIDGUID, WCHAR* strDest, int cchDest )
{
    wcscpy_s( strDest, cchDest, strRatingIDGUID );

    WCHAR strRatingSystemGUIDUpper[512];
    wcscpy_s( strRatingSystemGUIDUpper, 512, strRatingSystemGUID );
    _wcsupr_s( strRatingSystemGUIDUpper );

    WCHAR strRatingIDGUIDUpper[512];
    wcscpy_s( strRatingIDGUIDUpper, 512, strRatingIDGUID );
    _wcsupr_s( strRatingIDGUIDUpper );

    HRESULT hr;
    IXMLDOMNode* pRatingSystemNode = nullptr;
    WCHAR str[512];
    swprintf_s( str, 512, L"//Ratings/RatingSystem[@ID='%s']", strRatingSystemGUIDUpper );

    hr = m_pRootNode->selectSingleNode( str, &pRatingSystemNode );
    if( SUCCEEDED(hr) && pRatingSystemNode )
    {
        IXMLDOMNode* pRatingIDNode = nullptr;
        swprintf_s( str, 512, L"Rating[@ID='%s']", strRatingIDGUIDUpper );
        hr = pRatingSystemNode->selectSingleNode( str, &pRatingIDNode );
        if( SUCCEEDED(hr) && pRatingIDNode )
        {
            hr = GetAttribFromNode( pRatingIDNode, L"Text", strDest, cchDest );
            SAFE_RELEASE( pRatingIDNode );
        }
        SAFE_RELEASE( pRatingSystemNode );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CRatingsDB::GetDescriptorName( WCHAR* strRatingSystemGUID, WCHAR* strDescriptorGUID, WCHAR* strDest, int cchDest )
{
    wcscpy_s( strDest, cchDest, strDescriptorGUID );

    WCHAR strRatingSystemGUIDUpper[512];
    wcscpy_s( strRatingSystemGUIDUpper, 512, strRatingSystemGUID );
    _wcsupr_s( strRatingSystemGUIDUpper );

    WCHAR strDescriptorGUIDUpper[512];
    wcscpy_s( strDescriptorGUIDUpper, 512, strDescriptorGUID );
    _wcsupr_s( strDescriptorGUIDUpper );

    HRESULT hr;
    IXMLDOMNode* pRatingSystemNode = nullptr;
    WCHAR str[512];
    swprintf_s( str, 512, L"//Ratings/RatingSystem[@ID='%s']", strRatingSystemGUIDUpper );

    hr = m_pRootNode->selectSingleNode( str, &pRatingSystemNode );
    if( SUCCEEDED(hr) && pRatingSystemNode )
    {
        IXMLDOMNode* pRatingIDNode = nullptr;
        swprintf_s( str, 512, L"Descriptor[@ID='%s']", strDescriptorGUIDUpper );
        hr = pRatingSystemNode->selectSingleNode( str, &pRatingIDNode );
        if( SUCCEEDED(hr) && pRatingIDNode )
        {
            hr = GetAttribFromNode( pRatingIDNode, L"Text", strDest, cchDest );
            SAFE_RELEASE( pRatingIDNode );
        }
        SAFE_RELEASE( pRatingSystemNode );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CRatingsDB::GetAttribFromNode( IXMLDOMNode* pNode, WCHAR* strAttrib, WCHAR* strDest, int cchDest )
{
    IXMLDOMNamedNodeMap *pIXMLDOMNamedNodeMap = nullptr;
    BSTR bstrAttributeName = ::SysAllocString( strAttrib );
    IXMLDOMNode* pIXMLDOMNode = nullptr;    
    bool bFound = false;

    HRESULT hr;
    VARIANT v;
    hr = pNode->get_attributes( &pIXMLDOMNamedNodeMap );
    if(SUCCEEDED(hr) && pIXMLDOMNamedNodeMap)
    {
        hr = pIXMLDOMNamedNodeMap->getNamedItem( bstrAttributeName, &pIXMLDOMNode );
        if(SUCCEEDED(hr) && pIXMLDOMNode)
        {
            pIXMLDOMNode->get_nodeValue(&v);
            if( SUCCEEDED(hr) && v.vt == VT_BSTR )
            {
                wcscpy_s( strDest, cchDest, v.bstrVal );
                bFound = true;
            }
            VariantClear(&v);
            SAFE_RELEASE( pIXMLDOMNode );
        }
        SAFE_RELEASE( pIXMLDOMNamedNodeMap );
    }

    ::SysFreeString(bstrAttributeName);
    bstrAttributeName = nullptr;

    if( !bFound )
        return E_FAIL;
    else
        return S_OK;
}

