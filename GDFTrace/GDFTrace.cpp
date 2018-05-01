//-----------------------------------------------------------------------------
// File: GDFTrace.cpp
//
// Desc: Code that examines a GDF DLL and displays the contents
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define _WIN32_DCOM
#include <rpcsal.h>
#include <gameux.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wbemidl.h>
#include <objbase.h>
#define NO_SHLWAPI_STRFCNS
#include <shlwapi.h>
#include <wincodec.h>
#include "GDFParse.h"
#include "RatingsDB.h"
#include "GDFData.h"
#include "WICImage.h"

#include <stdio.h>
#include <memory>

struct SETTINGS
{
    WCHAR strGDFBinPath[MAX_PATH];
    bool bGDFInsteadOfBinary;
    bool bMuteGDF;
    bool bMuteWarnings;
    bool bQuiet;    
    bool bStore;
};

SETTINGS g_settings;
int g_nNumberOfWarnings = 0;

bool ParseCommandLine( SETTINGS* pSettings );
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
void DisplayUsage();

//-----------------------------------------------------------------------------
struct SValue
{
    LPWSTR pName;
    DWORD dwValue;
};

static SValue g_ImageContainerList[] =
{
    { L"BMP",            IMAGE_BMP },
    { L"JPG",            IMAGE_JPEG },
    { L"PNG",            IMAGE_PNG },
    { L"TIFF",           IMAGE_TIFF },
    { L"GIF",            IMAGE_GIF },
    { L"WMP",            IMAGE_WMP },
    { nullptr,           IMAGE_NONE }
};

static const LPWSTR FindName(DWORD value, const SValue* list )
{
    for(const SValue *ptr = list; ptr->pName != 0; ptr++)
    {
        if (ptr->dwValue == value)
            return ptr->pName;
    }

    return L"*UNKNOWN*";
}

//-----------------------------------------------------------------------------
struct PFValue
{
    LPWSTR pName;
    GUID pixelFormat;
};

static PFValue g_PixelFormatList[] =
{
    // Just have friendly names for formats supported as encodings for image files (PNG, BMP, JPEG, ICO)
    { L"P1", GUID_WICPixelFormat1bppIndexed },
    { L"P2", GUID_WICPixelFormat2bppIndexed },
    { L"P4", GUID_WICPixelFormat4bppIndexed },
    { L"P8", GUID_WICPixelFormat8bppIndexed },
    { L"R1", GUID_WICPixelFormatBlackWhite }, 
    { L"R2", GUID_WICPixelFormat2bppGray },
    { L"R4", GUID_WICPixelFormat4bppGray },
    { L"R8", GUID_WICPixelFormat8bppGray },
    { L"B5R5G5X1", GUID_WICPixelFormat16bppBGR555 },
    { L"B5G6R5", GUID_WICPixelFormat16bppBGR565 },
    { L"B8G8R8", GUID_WICPixelFormat24bppBGR },
    { L"B8G8R8X8", GUID_WICPixelFormat32bppBGR },
    { L"B8G8R8A8", GUID_WICPixelFormat32bppBGRA },
    { L"B16G16R16", GUID_WICPixelFormat48bppBGR },
    { L"B16G16R16A16", GUID_WICPixelFormat64bppBGRA },
    { L"C8M8Y8K8", GUID_WICPixelFormat32bppCMYK },
    { nullptr, GUID_NULL },
};

static void PixelFormatName( const GUID& pixelFormat, WCHAR *buff, size_t maxsize )
{
    for(PFValue *ptr = g_PixelFormatList; ptr->pName != 0; ++ptr)
    {
        if ( memcmp( &pixelFormat, &ptr->pixelFormat, sizeof(GUID) ) == 0 )
        {
            wcscpy_s( buff, maxsize, ptr->pName );
            return;
        }
    }

    swprintf_s( buff, maxsize, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                pixelFormat.Data1, pixelFormat.Data2, pixelFormat.Data3,
                pixelFormat.Data4[0], pixelFormat.Data4[1], pixelFormat.Data4[2], pixelFormat.Data4[3],
                pixelFormat.Data4[4], pixelFormat.Data4[5], pixelFormat.Data4[6], pixelFormat.Data4[7] );
}

//-----------------------------------------------------------------------------
WCHAR* ConvertTypeToString( WCHAR* strType )
{
    if( wcscmp(strType, L"1") == 0)
        return L"Provider";
    else 
        return L"Game";
}

//-----------------------------------------------------------------------------
void OutputGDFData( GDFData* pGDFData, ImageInfo* pImage )
{
    wprintf( L"Language: %s (0x%0.4x)\n", pGDFData->strLanguage, pGDFData->wLanguage );
    if (*pGDFData->strName) wprintf( L"\tName: %s\n", pGDFData->strName );
    if (*pGDFData->strDescription) wprintf( L"\tDescription: %s\n", pGDFData->strDescription );
    if (*pGDFData->strReleaseDate) wprintf( L"\tRelease Date: %s\n", pGDFData->strReleaseDate );
    if (*pGDFData->strGenre) wprintf( L"\tGenre: %s\n", pGDFData->strGenre );

    for( int iRating=0; iRating < MAX_RATINGS; iRating++ )
    {
        if( pGDFData->ratingData[iRating].strRatingSystem[0] == 0 )
            break;
        wprintf( L"\tRating: %s, %s", pGDFData->ratingData[iRating].strRatingSystem, pGDFData->ratingData[iRating].strRatingID );

        for( int iDescriptor=0; iDescriptor < MAX_DESCRIPTOR; iDescriptor++ )
        {
            if( pGDFData->ratingData[iRating].strDescriptor[iDescriptor][0] == 0 )
                break;
            wprintf( L", %s", pGDFData->ratingData[iRating].strDescriptor[iDescriptor] );
        }

        wprintf( L"\n" );
    }

    if (*pGDFData->strVersion) wprintf( L"\tVersion: %s\n", pGDFData->strVersion );
    if (*pGDFData->strSavedGameFolder) wprintf( L"\tSaved Game Folder:\n\t\t%s\n", pGDFData->strSavedGameFolder );
    if (pGDFData->fSPRMin != 0) wprintf( L"\tWinSPR Min: %.1f\n", pGDFData->fSPRMin );
    if (pGDFData->fSPRRecommended != 0) wprintf( L"\tWinSPR Recommended: %.1f\n", pGDFData->fSPRRecommended );
    if (*pGDFData->strDeveloper) wprintf( L"\tDeveloper: %s\n", pGDFData->strDeveloper );
    if (*pGDFData->strDeveloperLink) wprintf( L"\tDeveloper Link:\n\t\t%s\n", pGDFData->strDeveloperLink );
    if (*pGDFData->strPublisher) wprintf( L"\tPublisher: %s\n", pGDFData->strPublisher );
    if (*pGDFData->strPublisherLink) wprintf( L"\tPublisher Link:\n\t\t%s\n", pGDFData->strPublisherLink );
    wprintf( L"\tType: %s\n", ConvertTypeToString(pGDFData->strType) );
    if( pGDFData->strType[0] == L'1' && *pGDFData->strRSS )
        wprintf( L"\tRSS: %s\n", pGDFData->strRSS );

    for( int iGame=0; iGame < MAX_GAMES; iGame++ )
    {   
        if( pGDFData->strExe[iGame][0] == 0 ) 
            break;
        wprintf( L"\tEXE: %s\n", pGDFData->strExe[iGame] );
    }

    if ( pImage && pImage->container != IMAGE_NONE )
    {
        WCHAR pfbuff[128] =  {0};
        PixelFormatName( pImage->pixelFormat, pfbuff, 128 );

        wprintf( L"\tThumbnail image: %ux%u (%s) %s\n", pImage->width, pImage->height, pfbuff,
                                                           FindName(pImage->container, g_ImageContainerList) );
    }

    if ( pGDFData->fV2GDF )
    {
        // Primary task (required)
        if ( pGDFData->primaryPlayTask.islink )
        {
            wprintf( L"\tPrimary Play Task:\n\t\t%s\n", pGDFData->primaryPlayTask.strPathOrLink );
        }
        else
        {
            wprintf( L"\tPrimary Play Task:\n\t\t%s %s\n", pGDFData->primaryPlayTask.strPathOrLink, pGDFData->primaryPlayTask.strArgs );
        }

        // Secondary play tasks (optional)
        for( size_t iTask=0; iTask < MAX_TASKS; ++iTask )
        {
            GDFTask* pGDFTask = &pGDFData->secondaryPlayTasks[ iTask ];
            if ( pGDFTask->strPathOrLink[0] == 0 )
                break;

            if ( pGDFTask->islink )
            {
                wprintf( L"\tPlay Task \"%s\" (#%u):\n\t\t%s\n", pGDFTask->strName, pGDFTask->index, pGDFTask->strPathOrLink );
            }
            else
            {
                wprintf( L"\tPlay Task \"%s\" (#%u):\n\t\t%s\t%s\n", pGDFTask->strName, pGDFTask->index, pGDFTask->strPathOrLink, pGDFTask->strArgs );
            }
        }

        // Support tasks (optional)
        for( size_t iTask=0; iTask < MAX_TASKS; ++iTask )
        {
            GDFTask* pGDFTask = &pGDFData->supportTasks[ iTask ];
            if ( pGDFTask->strPathOrLink[0] == 0 )
                break;

            if ( pGDFTask->islink )
            {
                wprintf( L"\tSupport Task \"%s\" (#%u):\n\t\t%s\n", pGDFTask->strName, pGDFTask->index, pGDFTask->strPathOrLink );
            }
            else
            {
                wprintf( L"\tSupport Task \"%s\" (#%u):\n\t\t%s\t%s\n", pGDFTask->strName, pGDFTask->index, pGDFTask->strPathOrLink, pGDFTask->strArgs );
            }
        }
    }
}

//-----------------------------------------------------------------------------
void OutputWarning( LPCWSTR strMsg, ... )
{
    WCHAR strBuffer[1024];   
    va_list args;
    va_start(args, strMsg);
    vswprintf_s( strBuffer, 1024, strMsg, args );
    strBuffer[1023] = L'\0';
    va_end(args);
    wprintf( strBuffer );

    g_nNumberOfWarnings++;
}

//-----------------------------------------------------------------------------
bool FindRatingSystem( const WCHAR* strRatingSystemGUID, const GDFData* pGDFData, int* pRatingIndex )
{
    for( int iRating=0; iRating < MAX_RATINGS; iRating++ )
    {
        if( pGDFData->ratingData[iRating].strRatingSystemGUID[0] == 0 )
            return false;

        if( _wcsnicmp( strRatingSystemGUID, pGDFData->ratingData[iRating].strRatingSystemGUID, MAX_LEN ) == 0 )
        {
            *pRatingIndex = iRating;
            return true;
        }
    }

    return false;
}


//-----------------------------------------------------------------------------
bool g_bOuputLangHeader = false;
void EnsureOutputRatingHeader( const GDFData* pGDFData1, const GDFData* pGDFData2 )
{
    if( !g_bOuputLangHeader ) 
    {
        wprintf( L"\tComparing %s [0x%0.4x] with %s [0x%0.4x]\n", pGDFData1->strLanguage, pGDFData1->wLanguage, pGDFData2->strLanguage, pGDFData2->wLanguage );
        g_bOuputLangHeader = true;
    }
}

//-----------------------------------------------------------------------------
void OutputRatingWarning( const GDFData* pGDFData1, const GDFData* pGDFData2, LPCWSTR strMsg, ... )
{
    WCHAR strBuffer[1024];
    EnsureOutputRatingHeader( pGDFData1, pGDFData2 );
    
    va_list args;
    va_start(args, strMsg);
    vswprintf_s( strBuffer, 1024, strMsg, args );
    strBuffer[1023] = L'\0';
    va_end(args);

    OutputWarning( strBuffer );
}


//-----------------------------------------------------------------------------
bool CompareRatingSystems( const GDFData* pGDFData1, const GDFData* pGDFData2 )
{
    bool bWarningsFound = false;

    for( int iRating1=0; iRating1 < MAX_RATINGS; iRating1++ )
    {
        const GDFRatingData* pRating1 = &pGDFData1->ratingData[iRating1];
        if( pRating1->strRatingSystemGUID[0] == 0 )
            break;

        int iRating2 = 0;
        if( FindRatingSystem( pRating1->strRatingSystemGUID, pGDFData2, &iRating2 ) )
        {
            //wprintf( L"\t\tInfo: Rating system %s found in %s lang\n", pRating1->strRatingSystem, pGDFData2->strLanguage );

            const GDFRatingData* pRating2 = &pGDFData2->ratingData[iRating2];
            if( _wcsnicmp( pRating1->strRatingID, pRating2->strRatingID, MAX_LEN ) != 0 )
            {
                OutputRatingWarning( pGDFData1, pGDFData2, L"\tWarning: %s rating mismatch: %s vs %s \n", pRating1->strRatingSystem, pRating1->strRatingID, pRating2->strRatingID );
                bWarningsFound = true;
            }                    
            else
            {
                //wprintf( L"\t\tInfo: %s rating match: %s vs %s \n", pRating1->strRatingSystem, pRating1->strRatingID, pRating2->strRatingID );
            }

            for( int iDescriptor1=0; iDescriptor1 < MAX_DESCRIPTOR; iDescriptor1++ )
            {
                if( pRating1->strDescriptor[iDescriptor1][0] == 0 )
                    break;

                bool bFound = false;
                for( int iDescriptor2=0; iDescriptor2 < MAX_DESCRIPTOR; iDescriptor2++ )
                {
                    if( pRating2->strDescriptor[iDescriptor2][0] == 0 )
                        break;

                    if( _wcsnicmp( pRating1->strDescriptor[iDescriptor1], pRating2->strDescriptor[iDescriptor2], MAX_LEN ) == 0 )
                    {
                        bFound = true;
                        break;
                    }
                }
                if( !bFound )
                {
                    OutputRatingWarning( pGDFData1, pGDFData2, L"\tWarning: %s rating descriptor not found: %s\n", pRating1->strRatingSystem, pRating1->strDescriptor[iDescriptor1] );
                    bWarningsFound = true;
                }
                else
                {
                    //wprintf( L"\t\tInfo: %s rating descriptor found: %s\n", pRating1->strRatingSystem, pRating1->strDescriptor[iDescriptor1] );
                }
            }
        }
        else
        {
            OutputRatingWarning( pGDFData1, pGDFData2, L"\tWarning: Rating system %s not found in %s lang\n", pRating1->strRatingSystem, pGDFData2->strLanguage );
            bWarningsFound = true;
        }
    }

    return bWarningsFound;
}


//-----------------------------------------------------------------------------
void CompareGDFData( const GDFData* pGDFData1, const GDFData* pGDFData2, bool bQuiet )
{
    g_bOuputLangHeader = false;
    bool bSPRWarningsFound = false;
    bool bGameIDWarningsFound = false;

    if( pGDFData1->fSPRMin != pGDFData2->fSPRMin )
    {
        bSPRWarningsFound = true;
        OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Mismatched SPR min: %.1f vs %.1f\n", pGDFData1->fSPRMin, pGDFData2->fSPRMin );
    }
    if( pGDFData1->fSPRRecommended != pGDFData2->fSPRRecommended )
    {
        bSPRWarningsFound = true;
        OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Mismatched SPR recommended: %.1f vs %.1f\n", pGDFData1->fSPRRecommended, pGDFData2->fSPRRecommended );
    }

    if( _wcsnicmp( pGDFData1->strGameID, pGDFData2->strGameID, MAX_LEN ) != 0 )
    {
        bGameIDWarningsFound = true;
        OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Mismatched game ID guid: %s vs %s\n", pGDFData1->strGameID, pGDFData2->strGameID );
    }

    if ( pGDFData1->strReleaseDate != 0 || pGDFData2->strReleaseDate != 0 )
    {
        if ( _wcsnicmp( pGDFData1->strReleaseDate, pGDFData2->strReleaseDate, MAX_LEN ) != 0 )
        {
            bGameIDWarningsFound = true;
            OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Mismatched release dates: %s vs %s\n", pGDFData1->strReleaseDate, pGDFData2->strReleaseDate );
        }
    }

    bool bExeWarningsFound = false;
    for( int iGame=0; iGame < MAX_GAMES; iGame++ )
    {   
        if( pGDFData1->strExe[iGame][0] == 0 && pGDFData2->strExe[iGame][0] == 0 )
            break;
        if( _wcsnicmp( pGDFData1->strExe[iGame], pGDFData2->strExe[iGame], MAX_EXE ) != 0 )
        {
            bExeWarningsFound = true;
            OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Game EXE mismatch: %s vs %s\n", pGDFData1->strExe[iGame], pGDFData2->strExe[iGame] );
        }
    }

    bool bProviderWarningsFound = false;
    if( _wcsnicmp( pGDFData1->strType, pGDFData2->strType, MAX_LEN ) != 0 )
    {
        bProviderWarningsFound = true;
        OutputRatingWarning( pGDFData1, pGDFData2, L"\t\tWarning: Type (Game/Provider) mismatch between languages.\n" );
    }

    bool bWarningsFound1 = CompareRatingSystems( pGDFData1, pGDFData2 );
    bool bWarningsFound2 = CompareRatingSystems( pGDFData2, pGDFData1  );

    if( !bWarningsFound1 && !bWarningsFound2 && !bExeWarningsFound && !bSPRWarningsFound && !bGameIDWarningsFound && !bProviderWarningsFound )
    {
        // Matching Game ID, ratings, exes, and SPR data
        if( !bQuiet )
        {
            EnsureOutputRatingHeader( pGDFData1, pGDFData2 );
            wprintf( L"\t\tNo data mismatch found\n" );    
        }
    }
}

//-----------------------------------------------------------------------------
static bool ContainsPathReservedChars( const WCHAR* str, size_t cchStr )
{
    const wchar_t* ch = str;
    for( size_t count=0; *ch != 0 && count < cchStr; ++ch, ++count )
    {
        if ( wcschr( L"<>:\"/\\|?*", *ch ) != 0 )
            return true;
    }

    return false;
}

//-----------------------------------------------------------------------------
HRESULT ScanForWarnings( const GDFData* pGDFDataList, const ImageInfo* pImageList, BOOL** ppIconEightBits, BOOL** ppIconThirtyTwoBits, int nNumLangs, bool bQuiet, bool bStore, bool bWarnMissingNEU )
{
    wprintf( L"Warnings:\n" );

    if ( bWarnMissingNEU )
    {
       // Loop through all languages and warn if there's no language neutral 
       bool bFoundNeutral = false;
       bool bFoundSublangNeutral = false;
       bool bFoundNonNeutral = false;
       for( int iLang1=0; iLang1 < nNumLangs; iLang1++ )
       {
           const GDFData* pGDFData1 = &pGDFDataList[iLang1];
           if( LOBYTE(pGDFData1->wLanguage) == LANG_NEUTRAL ) 
           {
               bFoundNeutral = true;

               if ( HIBYTE(pGDFData1->wLanguage) == SUBLANG_NEUTRAL )
                   bFoundSublangNeutral = true;
           }
           else
               bFoundNonNeutral = true;
       }
       if( !bFoundNeutral ) 
           OutputWarning( L"\tWarning: Language neutral not found.  Adding one is highly recommended to cover all other languages\n" );
       else
       {
           if ( bFoundSublangNeutral )
               OutputWarning( L"\tWarning: Language neutral resource is marked SUBLANG_NEUTRAL, recommend using SUBLANG_DEFAULT instead\n" );

           if ( !bFoundNonNeutral && !bStore )
               OutputWarning( L"\tWarning: Found only language neutral resource, recommend using at least one non-neutral language in addition\n" );
       }
    }

    // Warn if there's any missing data or if there were XML validation warnings
    for( int iLang=0; iLang < nNumLangs; iLang++ )
    {
        WCHAR strHeader[256];
        swprintf_s( strHeader, 256, L"\t%s (0x%0.4x): ", pGDFDataList[iLang].strLanguage, pGDFDataList[iLang].wLanguage );

        if( !pGDFDataList[iLang].fV2GDF )
            wprintf( L"%sThis GDF is using the v1 schema. Use of GDF v2 is recommended.\n", strHeader );

        if( pGDFDataList[iLang].strValidation[0] != 0  )
            OutputWarning( L"%s%s\n", strHeader, pGDFDataList[iLang].strValidation );
        else if( !bQuiet )
            wprintf( L"%sNo validation warnings found\n", strHeader );

        if ( !bStore )
        {
            if( pGDFDataList[iLang].strPublisher[0] == 0
                || wcsnlen_s(pGDFDataList[iLang].strPublisher, MAX_LEN) == 0 )
                OutputWarning( L"%sPublisher field is blank\n", strHeader );
            if( pGDFDataList[iLang].strPublisherLink[0] == 0
                || wcsnlen_s(pGDFDataList[iLang].strPublisherLink, MAX_LEN) == 0 )
                OutputWarning( L"%sPublisher link field is blank\n", strHeader );
            if( pGDFDataList[iLang].strDeveloper[0] == 0
                || wcsnlen_s(pGDFDataList[iLang].strDeveloper, MAX_LEN) == 0 )
                OutputWarning( L"%sDeveloper field is blank\n", strHeader );
            if( pGDFDataList[iLang].strDeveloperLink[0] == 0 
                || wcsnlen_s(pGDFDataList[iLang].strDeveloperLink, MAX_LEN) == 0 )
                OutputWarning( L"%sDeveloper link field is blank\n", strHeader );
            if( pGDFDataList[iLang].strGenre[0] == 0
                || wcsnlen_s(pGDFDataList[iLang].strGenre, MAX_LEN) == 0 )
                OutputWarning( L"%sGenre field is blank\n", strHeader );
            if( pGDFDataList[iLang].strDescription[0] == 0
                || wcsnlen_s(pGDFDataList[iLang].strDescription, MAX_DESC) == 0 )
                OutputWarning( L"%sDescription field is blank\n", strHeader );
            if( pGDFDataList[iLang].fSPRMin == 0 || pGDFDataList[iLang].fSPRRecommended == 0  )
            {
                if ( pGDFDataList[iLang].fSPRMin == 0 && pGDFDataList[iLang].fSPRRecommended == 0 )
                    OutputWarning( L"%sWinSPR minimum and recommended values not specified.\n", strHeader );
                else if ( pGDFDataList[iLang].fSPRMin == 0 )
                    OutputWarning( L"%sWinSPR minimum value not specified.\n", strHeader );
                else if ( pGDFDataList[iLang].fSPRRecommended == 0 )
                    OutputWarning( L"%sWinSPR recommended value not specified.\n", strHeader );
            }
            else if( pGDFDataList[iLang].fSPRMin == pGDFDataList[iLang].fSPRRecommended )
                OutputWarning( L"%sWinSPR minimum and recommended are the same.  Ensure this is intended.\n", strHeader );
            if( pGDFDataList[iLang].fSPRMin > pGDFDataList[iLang].fSPRRecommended )
                OutputWarning( L"%sWinSPR minimum should be less than or equal to recommended.\n", strHeader );
            if( pGDFDataList[iLang].strExe[0][0] == 0 )
                OutputWarning( L"%sNo EXEs listed\n", strHeader );
        }

        if( pGDFDataList[iLang].ratingData[0].strRatingSystemGUID[0] == 0 )
            OutputWarning( L"%sNo ratings data found\n", strHeader );
        if( pGDFDataList[iLang].strType[0] == L'1'
            && ( pGDFDataList[iLang].strRSS[0] == 0
                 || wcsnlen_s(pGDFDataList[iLang].strRSS, MAX_LINK) == 0 ) )
            OutputWarning( L"%sRSS link field is blank\n", strHeader );

        if ( pGDFDataList[iLang].fV2GDF )
        {
            // Check secondary play tasks
            bool playTaskIndex0 = false;
            bool playTaskDup = false;
            for( int iTask=0; iTask < MAX_TASKS; ++iTask )
            {
                const GDFTask* task = &pGDFDataList[iLang].secondaryPlayTasks[ iTask ];
                if ( task->strPathOrLink[0] == 0 )
                    break;
                
                if ( ContainsPathReservedChars( task->strName, MAX_LEN ) )
                    OutputWarning( L"%sSecondary Play Task \"%s\" (#%d) name contains invalid reserved path characters <>:\"/\\|?*\n", strHeader, task->strName, task->index );

                if ( task->index == 0 )
                    playTaskIndex0 = true;

                for( int iTask2=iTask+1; iTask2 < MAX_TASKS; ++iTask2 )
                {
                    const GDFTask* task2 = &pGDFDataList[iLang].secondaryPlayTasks[ iTask2 ];
                    if ( task2->strPathOrLink[0] == 0 )
                        break;

                    if ( task->index == task2->index )
                        playTaskDup = true;
                }
            }

            if ( playTaskIndex0 )
                OutputWarning( L"%sSecondary Play Task index should start at 1, not 0\n", strHeader );

            if ( playTaskDup )
                OutputWarning( L"%sDuplicate Secondary Play Task indices found\n", strHeader );

            // Check support tasks
            bool supportTaskDup = false;
            for( int iTask=0; iTask < MAX_TASKS; ++iTask )
            {
                const GDFTask* task = &pGDFDataList[iLang].supportTasks[ iTask ];
                if ( task->strPathOrLink[0] == 0 )
                    break;

                if ( ContainsPathReservedChars( task->strName, MAX_LEN ) )
                    OutputWarning( L"%sSupport Task \"%s\" (#%d) name contains invalid reserved path characters <>:\"/\\|?*\n", strHeader, task->strName, task->index );

                for( int iTask2=iTask+1; iTask2 < MAX_TASKS; ++iTask2 )
                {
                    const GDFTask* task2 = &pGDFDataList[iLang].supportTasks[ iTask2 ];
                    if ( task2->strPathOrLink[0] == 0 )
                        break;

                    if ( task->index == task2->index )
                        supportTaskDup = true;
                }
            }

            if ( supportTaskDup )
                OutputWarning( L"%sDuplicate Support Task indices found\n", strHeader );
        }

        // Windows Parental Controls warnings
        bool bPEGIFound = false;
        bool bPEGIFinlandFound = false;
        bool bOFLC = false;

        for( int iRating=0; iRating < MAX_RATINGS; iRating++ )
        {
            const WCHAR* ratingSystem = pGDFDataList[iLang].ratingData[iRating].strRatingSystemGUID;

            if( *ratingSystem == 0 )
                break;

            if( _wcsnicmp( ratingSystem, L"{36798944-B235-48AC-BF21-E25671F597EE}", MAX_LEN ) == 0 )
            {
                bPEGIFound = true;
            }

            if( _wcsnicmp( ratingSystem, L"{7F2A4D3A-23A8-4123-90E7-D986BF1D9718}", MAX_LEN ) == 0 )
            {
                bPEGIFinlandFound = true;
            }

            if( _wcsnicmp( ratingSystem, L"{EC290BBB-D618-4CB9-9963-1CAAE515443E}", MAX_LEN ) == 0 )
            {
                bOFLC = true;
            }

            const WCHAR* rating = pGDFDataList[iLang].ratingData[iRating].strRatingIDGUID;

            // CSRR deprecations
            if( _wcsnicmp( ratingSystem, L"{B305AB16-9FF2-40f5-A658-C014566500DE}", MAX_LEN ) == 0 )
            {
                if( _wcsnicmp( rating, L"{DC079638-B397-4dd5-9E45-6483401DC9C5}", MAX_LEN ) == 0 )
                {
                    OutputWarning( L"%sDeprecated CSRR rating PG rating found. Use PG12 or PG15 instead.\n", strHeader );
                }
            }

            // OFLC-NZ deprecations
            if( _wcsnicmp( ratingSystem, L"{03CF34A3-D6AA-49CF-8C6C-547ECC507CCF}", MAX_LEN ) == 0 )
            {
                if( _wcsnicmp( rating, L"{54AEBA1B-6AF7-4565-B18E-72A8A61F0DBC}", MAX_LEN ) == 0 )
                {
                    OutputWarning( L"%sDeprecated OFLC-NZ rating R rating found. Use R13, R15, R16, or R18 instead.\n", strHeader );
                }
            }

            // PEGI/BBFC deprecations
            if( _wcsnicmp( ratingSystem, L"{5B39D1B8-ED49-4055-8A47-04B29A579AD6}", MAX_LEN ) == 0 )
            {
                if( _wcsnicmp( rating, L"{C1EFDB71-BF02-440d-8663-F93ABD09437F}", MAX_LEN ) == 0 )
                {
                    OutputWarning( L"%sDeprecated PEGI/BBFC rating R18 rating found. Use 18+ instead.\n", strHeader );
                }
            }

            // CERO deprecations
            if( _wcsnicmp( ratingSystem, L"{30D34ABD-C6B3-4802-924E-F0C9FC65022B}", MAX_LEN ) == 0 )
            {
                if( _wcsnicmp( rating, L"{6B9EB3C0-B49A-4708-A6E6-F5476CE7567B}", MAX_LEN ) == 0 )
                {
                    OutputWarning( L"%sUnsupported CERO rating found.  Use latest GDFMaker to fix.\n", strHeader );
                }

                if( _wcsnicmp( rating, L"{17A01A46-0FF6-4693-9F18-D162C2A5C703}", MAX_LEN ) == 0
                    || _wcsnicmp( rating, L"{CA12E389-7F8F-4C3E-AC0D-E2762653A9DB}", MAX_LEN ) == 0
                    || _wcsnicmp( rating, L"{AEB8A50F-BC53-4005-8701-D9EF48A80A63}", MAX_LEN ) == 0
                    || _wcsnicmp( rating, L"{6B9EB3C0-B49A-4708-A6E6-F5476CE7567B}", MAX_LEN ) == 0 )
                {
                    OutputWarning( L"%sDeprecated CERO rating E, 12, 15, or 18 found. Use A, B, C, D, or Z instead.\n", strHeader );
                }
            }
            
            // ESRB deprecations
            if( _wcsnicmp( ratingSystem, L"{768BD93D-63BE-46A9-8994-0B53C4B5248F}", MAX_LEN ) == 0 )
            {
                for( int iDescriptor=0; iDescriptor < MAX_DESCRIPTOR; iDescriptor++ )
                {
                    const WCHAR* descriptor = pGDFDataList[iLang].ratingData[iRating].strDescriptor[iDescriptor];
                    if( *descriptor == 0 )
                        break;
                    if( _wcsnicmp( descriptor, L"{5990705B-1E85-4435-AE11-129B9319FF09}", MAX_LEN ) == 0 )
                    {                               
                        OutputWarning( L"%sDeprecated ESRB 'Gambling' descriptor found.  Use 'Simulated Gambling' instead.\n", strHeader );
                    }
                    if( _wcsnicmp( descriptor, L"{E9476FB8-0B11-4209-9A7D-CBA553C1555D}", MAX_LEN ) == 0 )
                    {                               
                        OutputWarning( L"%sDeprecated ESRB 'Mature Sexual Themes' descriptor found.  Use 'Sexual Themes' instead.\n", strHeader );
                    }
                    if( _wcsnicmp( descriptor, L"{1A796A5D-1E25-4862-9443-1550578FF4C4}", MAX_LEN ) == 0 )
                    {                               
                        OutputWarning( L"%sDeprecated ESRB 'Mild Language' descriptor found.  Use 'Language' instead.\n", strHeader );
                    }
                    if( _wcsnicmp( descriptor, L"{40B262D1-11AA-43C2-B7BA-63A9F5756A06}", MAX_LEN ) == 0 )
                    {                               
                        OutputWarning( L"%sDeprecated ESRB 'Mild Lyrics' descriptor found.  Use 'Lyrics' instead.\n", strHeader );
                    }
                }
            }
        }

        if ( bPEGIFinlandFound)
        {
            if ( bPEGIFound )
            {
                OutputWarning( L"%sThe PEGI-fi rating system has been deprecated and should be removed from the project.\n", strHeader);
            }
            else
            {
                OutputWarning( L"%sThe PEGI-fi rating system has been deprecated, PEGI is now the Finnish locale rating system.\n", strHeader);
            }
        }

        if ( bOFLC )
        {
            OutputWarning( L"%sThe OFLC rating system has been deprecated, COB-AU replaces it for Australia and OFLC-NZ replaces it for New Zealand.\n", strHeader);
        }
        
        // Icon/Thumbnail warnings
        if ( !bStore )
        {
            if ( pImageList )
            {
                if ( pImageList[iLang].container != IMAGE_NONE)
                {
                    OutputWarning( L"%sThumbnail image is not recommended, please use a 256x256 icon.\n", strHeader );

                    if ( pImageList[iLang].container != IMAGE_PNG )
                        OutputWarning( L"%sPNG format is recommended for GE thumbnail image data.\n", strHeader );

                    if ( pImageList[iLang].width < 256 || pImageList[iLang].height < 256 )
                        OutputWarning( L"%s256x256 is the recommended size of GE thumbnail image data (%d x %d).\n",
                                       strHeader, pImageList[iLang].width, pImageList[iLang].height );
                }
            }

            if (ppIconEightBits)
            {
                if (ppIconEightBits[iLang])
                {
                    for (int res=0; res < 4; res++)
                    {
                        if (!ppIconEightBits[iLang][res])
                        {
                            OutputWarning( L"%s%dx%d 8bits icon is missing.\n", strHeader, iconResolution[res], iconResolution[res] );
                        }
                    }
                }
            }

            if (ppIconThirtyTwoBits)
            {
                if (ppIconThirtyTwoBits[iLang])
                {
                    for (int res=0; res < 4; res++)
                    {
                        if (!ppIconThirtyTwoBits[iLang][res])
                        {
                            OutputWarning( L"%s%dx%d 32bits icon is missing.\n", strHeader, iconResolution[res], iconResolution[res] );
                        }
                    }
                }
            }

        }    

        wprintf( L"\n" );
    }

    // Loop through all languages comparing GDF data and printing warnings
    for( int iLang1=0; iLang1 < nNumLangs; iLang1++ )
    {
        for( int iLang2=iLang1+1; iLang2 < nNumLangs; iLang2++ )
        {
            const GDFData* pGDFData1 = &pGDFDataList[iLang1];
            const GDFData* pGDFData2 = &pGDFDataList[iLang2];

            CompareGDFData( pGDFData1, pGDFData2, bQuiet );
        }
    }

    if( g_nNumberOfWarnings == 0 )
    {
        wprintf( L"\tNo warnings found\n" );
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
HRESULT ProcessGDF( WCHAR* strGDFPath, bool bMuteWarnings, bool bMuteGDF, bool bQuiet, bool bStore )
{
    HRESULT hr;

    CRatingsDB ratingsDB;
    ratingsDB.LoadDB();

    CGDFParse gdfParse;

    auto pGDFData = std::make_unique<GDFData>();
    ZeroMemory( pGDFData.get(), sizeof(GDFData) );

    hr = GetGDFDataFromGDF( pGDFData.get(), strGDFPath );
    if( FAILED(hr) )
    {
        wprintf( L"Couldn't load GDF XML data from: %s\n", strGDFPath );
        if( pGDFData->strValidation[0] != 0 )
        {
            wprintf( L"%s\n", pGDFData->strValidation );
        }
        return E_FAIL;
    }

    if( !bMuteGDF )
    {
        OutputGDFData( pGDFData.get(), nullptr );
    }

    if( !bMuteWarnings )
        ScanForWarnings( pGDFData.get(), nullptr, nullptr, nullptr, 1, bQuiet, bStore, false );

    return S_OK;
}

//-----------------------------------------------------------------------------
HRESULT ProcessBIN( WCHAR* strGDFBinPath, bool bMuteWarnings, bool bMuteGDF, bool bQuiet, bool bStore )
{
    HRESULT hr;

    CRatingsDB ratingsDB;
    ratingsDB.LoadDB();

    CGDFParse gdfParse;
    hr = gdfParse.EnumLangs( strGDFBinPath );

    if ( FAILED(hr) )
    {
        LPVOID buff = 0;
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                       nullptr, (hr & 0xffff), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR) &buff, 0, nullptr );
        wprintf( L"Failed processing binary: %s\n(0x%08x) %s\n", strGDFBinPath, hr, (LPWSTR) buff );
        LocalFree( buff );
        return E_FAIL;
    }

    if ( gdfParse.GetNumLangs() == 0 )
    {
        wprintf( L"Could not locate any GDF language resources in binary: %s\n", strGDFBinPath );
        return E_FAIL;
    }

    const int nLangs = gdfParse.GetNumLangs();

    // Load GDF XML data
    auto pGDFDataList = std::make_unique <GDFData[]>(nLangs);
    ZeroMemory( pGDFDataList.get(), sizeof(GDFData)*nLangs );

    for( int iLang=0; iLang < nLangs; iLang++ )
    {
        WORD wLang = gdfParse.GetLang( iLang );

        hr = GetGDFDataFromBIN( &pGDFDataList[iLang], strGDFBinPath, wLang );
        if( FAILED(hr) )
        {
            wprintf( L"Couldn't load GDF XML data from: %s (wLang:0x%0.4x)\n", strGDFBinPath, wLang );
            if( pGDFDataList[iLang].strValidation[0] != 0 )
            {
                wprintf( L"%s\n", pGDFDataList[iLang].strValidation );
            }
            continue;
        }
    }

    // Load GDF Thumbnail data
    auto pImageList = std::make_unique<ImageInfo[]>(nLangs);
    ZeroMemory( pImageList.get(), sizeof(ImageInfo)*nLangs );
    
    for( int iLang=0; iLang < nLangs; iLang++ )
    {
        WORD wLang = gdfParse.GetLang( iLang );

        if ( FAILED(gdfParse.ExtractGDFThumbnail( strGDFBinPath, &pImageList[iLang], wLang )) )
        {
            pImageList[iLang].container = IMAGE_NONE;
        }       
    }

    BOOL** ppIconEightBits = new BOOL*[nLangs];
    for( int iLang=0; iLang < nLangs; iLang++ )
    {
        ppIconEightBits[iLang] = new BOOL[4];
        ZeroMemory( ppIconEightBits[iLang], sizeof(BOOL)*4 );
    }
    
    BOOL** ppIconThirtyTwoBits = new BOOL*[nLangs];
    for( int iLang=0; iLang < nLangs; iLang++ )
    {
        ppIconThirtyTwoBits[iLang] = new BOOL[4];
        ZeroMemory( ppIconThirtyTwoBits[iLang], sizeof(BOOL)*4 );
    }

    if( !bMuteGDF )
    {
        for( int iLang=0; iLang < nLangs; iLang++ )
        {
            OutputGDFData( &pGDFDataList[iLang], &pImageList[iLang] );
            
            WORD wLang = gdfParse.GetLang( iLang );
            gdfParse.OutputGDFIconInfo( strGDFBinPath, ppIconEightBits[iLang], ppIconThirtyTwoBits[iLang], wLang );
        }
    }

    if( !bMuteWarnings )
    {
        ScanForWarnings( pGDFDataList.get(), pImageList.get(), ppIconEightBits, ppIconThirtyTwoBits, nLangs, bQuiet, bStore, true );

        if ( !bStore && !gdfParse.IsIconPresent( strGDFBinPath ) )
            OutputWarning( L"\tWarning: Icon not found. Adding one is highly recommended!\n" );
    }
    
    for( int iLang=0; iLang < nLangs; iLang++ )
    {
        delete [] ppIconEightBits[iLang];
        delete [] ppIconThirtyTwoBits[iLang];
    }
    
    delete [] ppIconEightBits;
    delete [] ppIconThirtyTwoBits;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Entry point to the program. Initializes everything, and pops
// up a message box with the results of the GameuxInstallHelper calls
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    SETTINGS settings;
    memset(&settings, 0, sizeof(SETTINGS));
    settings.bQuiet = true;

    if( !ParseCommandLine( &settings ) )
        return 0;

    if ( FAILED(CoInitialize(0)) )
    {
        wprintf( L"ERROR: Failed to initialize COM\n");
        return 1;
    }

    HRESULT hr;

    if (settings.bGDFInsteadOfBinary)
    {
        hr = ProcessGDF( settings.strGDFBinPath, settings.bMuteWarnings, settings.bMuteGDF, settings.bQuiet, settings.bStore );
    }
    else
    {
        hr = ProcessBIN( settings.strGDFBinPath, settings.bMuteWarnings, settings.bMuteGDF, settings.bQuiet, settings.bStore );
    }

    if( SUCCEEDED(hr) && g_nNumberOfWarnings == 0 )
        return 0;
    else
        return 1;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
bool ParseCommandLine( SETTINGS* pSettings )
{
    WCHAR* strCmdLine;
//    WCHAR* strArg;

    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( GetCommandLine(), &nNumArgs );
    for( int iArg=1; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( IsNextArg( strCmdLine, L"gdf" ) )
            {
                pSettings->bGDFInsteadOfBinary = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"mutegdf" ) )
            {
                pSettings->bMuteGDF = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"mutewarnings" ) )
            {
                pSettings->bMuteWarnings = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"noisy" ) )
            {
                pSettings->bQuiet = false;
                continue;
            }

            if( IsNextArg( strCmdLine, L"store") )
            {
                pSettings->bStore = true;
                continue;
            }

            if( IsNextArg( strCmdLine, L"?" ) )
            {
                DisplayUsage();
                return false;
            }
        }
        else 
        {
            // Handle non-flag args as separate input files
            swprintf_s( pSettings->strGDFBinPath, MAX_PATH, L"%s", strCmdLine );
            continue;
        }
    }

    if( pSettings->strGDFBinPath[0] == 0 )
    {
        DisplayUsage();
        return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
    int nArgLen = (int) wcslen(strArg);
    if( _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && strCmdLine[nArgLen] == 0 )
        return true;

    return false;
}


//--------------------------------------------------------------------------------------
void DisplayUsage()
{
    wprintf( L"\n" );
    wprintf( L"GDFTrace - a command line tool that displays GDF metadata contained\n" );
    wprintf( L"           in a binary and highlights any warnings\n" );
    wprintf( L"\n" );
    wprintf( L"Usage: GDFTrace.exe [options] <gdf binary>\n" );
    wprintf( L"\n" );
    wprintf( L"where:\n" ); 
    wprintf( L"\n" ); 
    wprintf( L"  [/mutegdf]     \tmutes output of GDF data\n" );
    wprintf( L"  [/mutewarnings]\tmutes output of warnings\n" );
    wprintf( L"  [/store]       \tmutes warnings not relevant to Windows Store apps\n" );
    wprintf( L"  [/noisy]       \tenables output of success\n" );
    wprintf( L"  [/gdf]         \ttest .gdf file directly instead of embedded binary\n" );
    wprintf( L"  <gdf binary>\tThe path to the GDF binary\n" );
    wprintf( L"\n" );
    wprintf( L"After running, %%ERRORLEVEL%% will be 0 if no warnings are found,\n" );
    wprintf( L"and 1 otherwise.\n" );
    wprintf( L"\n" );
    wprintf( L"As an example, you can use GDFExampleBinary.dll found in the DXSDK.\n" );
}
