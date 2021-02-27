//--------------------------------------------------------------------------------------
// File: GDFData.h
//
// GDFTrace - Game Definition File trace utility
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#pragma once

#define MAX_DESCRIPTOR 128
#define MAX_GAMES 32
#define MAX_RATINGS 16

#define MAX_LEN 256
#define MAX_EXE 512
#define MAX_NAME 512
#define MAX_VAL 512
#define MAX_DESC 1025
#define MAX_LINK 2083
#define MAX_TASKS 5

struct GDFRatingData
{
    WCHAR strRatingSystemGUID[MAX_LEN];
    WCHAR strRatingSystem[MAX_LEN];
    WCHAR strRatingIDGUID[MAX_LEN];
    WCHAR strRatingID[MAX_LEN];
    WCHAR strDescriptorGUID[MAX_DESCRIPTOR][MAX_LEN];
    WCHAR strDescriptor[MAX_DESCRIPTOR][MAX_LEN];
};

struct GDFTask
{
    bool islink;
    UINT index;
    WCHAR strName[ MAX_LEN ];
    WCHAR strPathOrLink[ MAX_LINK ];
    WCHAR strArgs[ MAX_PATH ];
};

struct GDFData
{
    WORD wLanguage;
    WCHAR strLanguage[MAX_LEN];
    WCHAR strValidation[MAX_VAL];

    GDFRatingData ratingData[MAX_RATINGS];

    GDFTask primaryPlayTask;
    GDFTask secondaryPlayTasks[ MAX_TASKS ];
    GDFTask supportTasks[ MAX_TASKS ];

    WCHAR strGameID[MAX_LEN];
    WCHAR strName[MAX_NAME];
    WCHAR strDescription[MAX_DESC];
    WCHAR strReleaseDate[MAX_LEN];
    WCHAR strGenre[MAX_LEN];
    WCHAR strVersion[MAX_LEN];
    WCHAR strSavedGameFolder[MAX_LEN];
    float fSPRMin;
    float fSPRRecommended;
    WCHAR strDeveloper[MAX_LEN];
    WCHAR strDeveloperLink[MAX_LEN];
    WCHAR strPublisher[MAX_LEN];
    WCHAR strPublisherLink[MAX_LEN];
    WCHAR strType[MAX_LEN];
    WCHAR strRSS[MAX_LINK];
    BOOL  fV2GDF;

    WCHAR strExe[MAX_GAMES][MAX_EXE];
};


HRESULT GetGDFDataFromGDF( GDFData* pGDFData, const WCHAR* strGDFPath);
HRESULT GetGDFDataFromBIN( GDFData* pGDFData, const WCHAR* strGDFBinPath, WORD wLanguage );

