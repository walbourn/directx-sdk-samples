//--------------------------------------------------------------------------------------
// File: GDFData.cpp
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <rpcsal.h>
#include <gameux.h>
#include <crtdbg.h>
#include <stdio.h>
#include <assert.h>
#include <shellapi.h>
#include <shlobj.h>

#include "GDFData.h"
#include "GDFParse.h"
#include "RatingsDB.h"

WCHAR* ConvertLangIDtoString( WORD wLang );
HRESULT GetGDFData( GDFData* pGDFData, CGDFParse& gdfParse );

//--------------------------------------------------------------------------------------
HRESULT GetGDFDataFromGDF( GDFData* pGDFData, const WCHAR* strGDFPath)
{
    CGDFParse gdfParse;
    gdfParse.ValidateGDF( strGDFPath, pGDFData->strValidation, MAX_VAL );

    HRESULT hr = gdfParse.LoadXML( strGDFPath );
    if ( FAILED(hr) )
        return E_FAIL;

    pGDFData->wLanguage = LANG_NEUTRAL;
    wcscpy_s( pGDFData->strLanguage, MAX_LEN, L"LANG_UNKNOWN" );

    return GetGDFData( pGDFData, gdfParse);
}

//--------------------------------------------------------------------------------------
HRESULT GetGDFDataFromBIN( GDFData* pGDFData, const WCHAR* strGDFBinPath, WORD wLanguage )
{
    CGDFParse gdfParse;
    gdfParse.ValidateXML( strGDFBinPath, wLanguage, pGDFData->strValidation, MAX_VAL );

    gdfParse.ExtractXML( strGDFBinPath, wLanguage );
    pGDFData->wLanguage = wLanguage;
    wcscpy_s( pGDFData->strLanguage, MAX_LEN, ConvertLangIDtoString(wLanguage) );

    return GetGDFData( pGDFData, gdfParse);
}

//--------------------------------------------------------------------------------------
HRESULT GetGDFData( GDFData* pGDFData, CGDFParse& gdfParse )
{
    CRatingsDB ratingsDB;
    ratingsDB.LoadDB();
    
    gdfParse.GetGameID( pGDFData->strGameID, MAX_LEN );
    gdfParse.GetName( pGDFData->strName, MAX_NAME );
    gdfParse.GetDescription( pGDFData->strDescription, MAX_DESC );
    gdfParse.GetReleaseDate( pGDFData->strReleaseDate, MAX_LEN );
    gdfParse.GetGenre( pGDFData->strGenre, MAX_LEN );
    gdfParse.GetVersion( pGDFData->strVersion, MAX_LEN );
    gdfParse.GetSavedGameFolder( pGDFData->strSavedGameFolder, MAX_LEN );
    gdfParse.GetWinSPR( &pGDFData->fSPRMin, &pGDFData->fSPRRecommended );
    gdfParse.GetDeveloper( pGDFData->strDeveloper, MAX_LEN );
    gdfParse.GetDeveloperLink( pGDFData->strDeveloperLink, MAX_LEN );
    gdfParse.GetPublisher( pGDFData->strPublisher, MAX_LEN );
    gdfParse.GetPublisherLink( pGDFData->strPublisherLink, MAX_LEN );
    gdfParse.GetType( pGDFData->strType, MAX_LEN );
    gdfParse.GetRSS( pGDFData->strRSS, MAX_LINK );
    gdfParse.IsV2GDF( &pGDFData->fV2GDF );

    HRESULT hr;
    for( int iRating=0; iRating < MAX_RATINGS; iRating++ )
    {
        GDFRatingData* pGDFRatingData = &pGDFData->ratingData[iRating];
        hr = gdfParse.GetRatingSystem( iRating, pGDFRatingData->strRatingSystemGUID, MAX_LEN );
        if( FAILED(hr) )
            break;
        gdfParse.GetRatingID( iRating, pGDFRatingData->strRatingIDGUID, MAX_LEN );

        ratingsDB.GetRatingSystemName( pGDFRatingData->strRatingSystemGUID, pGDFRatingData->strRatingSystem, MAX_LEN );
        ratingsDB.GetRatingIDName( pGDFRatingData->strRatingSystemGUID, pGDFRatingData->strRatingIDGUID, pGDFRatingData->strRatingID, MAX_LEN );

        for( int iDescriptor=0; iDescriptor < MAX_DESCRIPTOR; iDescriptor++ )
        {
            hr = gdfParse.GetRatingDescriptor( iRating, iDescriptor, pGDFRatingData->strDescriptorGUID[iDescriptor], MAX_LEN );
            if( FAILED(hr) )
                break;
            ratingsDB.GetDescriptorName( pGDFRatingData->strRatingSystemGUID, pGDFRatingData->strDescriptorGUID[iDescriptor], pGDFRatingData->strDescriptor[iDescriptor], MAX_LEN );
        }
    }

    for( int iGame=0; iGame < MAX_GAMES; iGame++ )
    {
        hr = gdfParse.GetGameExe( iGame, pGDFData->strExe[iGame], MAX_EXE );
        if( FAILED(hr) )
            break;
    }

    if ( pGDFData->fV2GDF )
    {
        GDFTask* pGDFTask = &pGDFData->primaryPlayTask;
        pGDFTask->index = 0;
        gdfParse.GetPrimaryPlayTask( pGDFTask->strPathOrLink, MAX_LINK, pGDFTask->strArgs, MAX_PATH, pGDFTask->islink );
        
        for( int iTask=0; iTask < MAX_TASKS; ++iTask )
        {
            GDFTask* pGDFTask2 = &pGDFData->secondaryPlayTasks[ iTask ];
            gdfParse.GetTask( iTask, false, pGDFTask2->strName, MAX_LEN, pGDFTask2->strPathOrLink, MAX_LINK, pGDFTask2->strArgs, MAX_PATH, pGDFTask2->islink, pGDFTask2->index );
        }

        for( int iTask=0; iTask < MAX_TASKS; ++iTask )
        {
            GDFTask* pGDFTask2 = &pGDFData->supportTasks[ iTask ];
            gdfParse.GetTask( iTask, true, pGDFTask2->strName, MAX_LEN, pGDFTask2->strPathOrLink, MAX_LINK, pGDFTask2->strArgs, MAX_PATH, pGDFTask2->islink, pGDFTask2->index );
        }
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
#define CONV_LANGID(x) case x: return L#x;
WCHAR* ConvertLangIDtoString( WORD wLang )
{
    switch( LOBYTE(wLang) )
    {
        CONV_LANGID(LANG_NEUTRAL);
        CONV_LANGID(LANG_INVARIANT);
        CONV_LANGID(LANG_AFRIKAANS);
        CONV_LANGID(LANG_ARABIC);
        CONV_LANGID(LANG_ARMENIAN);
        CONV_LANGID(LANG_ASSAMESE);
        CONV_LANGID(LANG_AZERI);
        CONV_LANGID(LANG_BASQUE);
        CONV_LANGID(LANG_BELARUSIAN);
        CONV_LANGID(LANG_BENGALI);
        CONV_LANGID(LANG_BULGARIAN);
        CONV_LANGID(LANG_CATALAN);
        CONV_LANGID(LANG_CHINESE);
        CONV_LANGID(LANG_CZECH);
        CONV_LANGID(LANG_DANISH);
        CONV_LANGID(LANG_DIVEHI);
        CONV_LANGID(LANG_DUTCH);
        CONV_LANGID(LANG_ENGLISH);
        CONV_LANGID(LANG_ESTONIAN);
        CONV_LANGID(LANG_FAEROESE);
        CONV_LANGID(LANG_FINNISH);
        CONV_LANGID(LANG_FRENCH);
        CONV_LANGID(LANG_GALICIAN);
        CONV_LANGID(LANG_GEORGIAN);
        CONV_LANGID(LANG_GERMAN);
        CONV_LANGID(LANG_GREEK);
        CONV_LANGID(LANG_GUJARATI);
        CONV_LANGID(LANG_HEBREW);
        CONV_LANGID(LANG_HINDI);
        CONV_LANGID(LANG_HUNGARIAN);
        CONV_LANGID(LANG_ICELANDIC);
        CONV_LANGID(LANG_INDONESIAN);
        CONV_LANGID(LANG_ITALIAN);
        CONV_LANGID(LANG_JAPANESE);
        CONV_LANGID(LANG_KANNADA);
        CONV_LANGID(LANG_KAZAK);
        CONV_LANGID(LANG_KONKANI);
        CONV_LANGID(LANG_KOREAN);
        CONV_LANGID(LANG_KYRGYZ);
        CONV_LANGID(LANG_LATVIAN);
        CONV_LANGID(LANG_LITHUANIAN);
        CONV_LANGID(LANG_MACEDONIAN);
        CONV_LANGID(LANG_MALAY);
        CONV_LANGID(LANG_MALAYALAM);
        CONV_LANGID(LANG_MANIPURI);
        CONV_LANGID(LANG_MARATHI);
        CONV_LANGID(LANG_MONGOLIAN);
        CONV_LANGID(LANG_NEPALI);
        CONV_LANGID(LANG_NORWEGIAN);
        CONV_LANGID(LANG_ORIYA);
        CONV_LANGID(LANG_POLISH);
        CONV_LANGID(LANG_PORTUGUESE);
        CONV_LANGID(LANG_PUNJABI);
        CONV_LANGID(LANG_ROMANIAN);
        CONV_LANGID(LANG_RUSSIAN);
        CONV_LANGID(LANG_SANSKRIT);
        CONV_LANGID(LANG_SINDHI);
        CONV_LANGID(LANG_SLOVAK);
        CONV_LANGID(LANG_SLOVENIAN);
        CONV_LANGID(LANG_SPANISH);
        CONV_LANGID(LANG_SWAHILI);
        CONV_LANGID(LANG_SWEDISH);
        CONV_LANGID(LANG_SYRIAC);
        CONV_LANGID(LANG_TAMIL);
        CONV_LANGID(LANG_TATAR);
        CONV_LANGID(LANG_TELUGU);
        CONV_LANGID(LANG_THAI);
        CONV_LANGID(LANG_TURKISH);
        CONV_LANGID(LANG_UKRAINIAN);
        CONV_LANGID(LANG_URDU);
        CONV_LANGID(LANG_UZBEK);
        CONV_LANGID(LANG_VIETNAMESE);
    }
    return L"Unknown";
}


