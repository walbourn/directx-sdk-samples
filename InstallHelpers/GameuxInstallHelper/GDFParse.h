//--------------------------------------------------------------------------------------
// File: GDFParse.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
class CGDFParse
{
public:
            CGDFParse();
            ~CGDFParse();

    HRESULT ExtractXML( WCHAR* strGDFBinPath );

    // To use these, call ExtractXML() first
    HRESULT GetName( WCHAR* strDest, int cchDest );
    HRESULT GetDescription( WCHAR* strDest, int cchDest );
    HRESULT GetReleaseDate( WCHAR* strDest, int cchDest );
    HRESULT GetGenre( WCHAR* strDest, int cchDest );
    HRESULT GetDeveloper( WCHAR* strDest, int cchDest );
    HRESULT GetPublisher( WCHAR* strDest, int cchDest );
    HRESULT GetWinSPR( int* pnMin, int* pnRecommended );
    HRESULT GetGameID( WCHAR* strDest, int cchDest );
    HRESULT ExtractGDFThumbnail( WCHAR* strGDFBinPath, WCHAR* strDestFilePath );
    HRESULT GetXMLRootNode(IXMLDOMNode** ppRootNode);

protected:
    HRESULT GetXMLValue( WCHAR* strXPath, WCHAR* strValue, int cchValue );
    HRESULT GetXMLAttrib( WCHAR* strXPath, WCHAR* strAttribName, WCHAR* strValue, int cchValue );

    IXMLDOMNode* m_pRootNode;
    bool m_bCleanupCOM;
};
