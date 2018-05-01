//-------------------------------------------------------------------------------------
// CoreDetection.cpp
// 
// Main program execution.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <crtdbg.h>
#include <mmsystem.h>
#include <stdio.h>
#include <wchar.h>
#include <process.h>
#include <conio.h>
#include "CpuTopology.h"

#pragma comment( lib, "winmm.lib" )

const DWORD_PTR CpuLoadTime = 10000;// in milliseconds
const DWORD     CpuLoadIndicatorFreq = 200;  // in milliseconds
const DWORD     ConWidth = 80;   // max characters in a line of text

// System/Process Info Table Formatting Constants
// NOTE: Header strings must be longer or equal to the max width of column data
const WCHAR SPITitle[] = L"System/Current Process Info:";
const WCHAR SPICpuCoresStr[] = L" CPU Cores ";
const WCHAR SPILogProcsStr[] = L" Logical Processors ";
const WCHAR SPISystemStr[] = L" SYSTEM ";
const WCHAR SPIProcessStr[] = L" AVAILABLE TO PROCESS ";
const DWORD     SPICpuCoresLen = sizeof( SPICpuCoresStr ) / sizeof( WCHAR );
const DWORD     SPILogProcsLen = sizeof( SPILogProcsStr ) / sizeof( WCHAR );
const DWORD     SPISystemLen = sizeof( SPISystemStr ) / sizeof( WCHAR );
const DWORD     SPIProcessLen = sizeof( SPIProcessStr ) / sizeof( WCHAR );
const DWORD     SPIHalfSystemLen = SPISystemLen / 2;
const DWORD     SPIHalfProcessLen = SPIProcessLen / 2;
const DWORD     SPILeftColumnLen = max( SPICpuCoresLen, SPILogProcsLen );

// Core Info Table Formatting Constants
// NOTE: Header strings must be longer or equal to the max width of column data
const WCHAR CITitle[] = L"Core Info:";
const WCHAR CICoreStr[] = L" CORE# ";
const WCHAR CIEnabledStr[] = L" ENABLED ";
const WCHAR CIAffinityMaskStr[] = L"        CORE AFFINITY MASK        ";
const DWORD     CICoreLen = sizeof( CICoreStr ) / sizeof( WCHAR );
const DWORD     CIEnabledLen = sizeof( CIEnabledStr ) / sizeof( WCHAR );
const DWORD     CIAffinityMaskLen = sizeof( CIAffinityMaskStr ) / sizeof( WCHAR );
const DWORD     CIHalfCoreLen = CICoreLen / 2;
const DWORD     CIHalfEnabledLen = CIEnabledLen / 2;

//-------------------------------------------------------------------------------------
// Name: SpinThreadProc
// Desc: A thread procedure that puts a load on a thread for a time specified in
//       lpParameter (in milliseconds).
//-------------------------------------------------------------------------------------
unsigned int WINAPI SpinThreadProc( void* lpParameter )
{
    DWORD endTime = timeGetTime() + ( DWORD )( DWORD_PTR )lpParameter;
    DWORD curTime;
    do
    {
        curTime = timeGetTime();
    } while( curTime < endTime );
    return endTime;
}

//-------------------------------------------------------------------------------------
// Name: GetAffinityStr
// Desc: Writes a readable binary representation of an affinity mask to a string
//       buffer.
//-------------------------------------------------------------------------------------
LPWSTR GetAffinityStr( DWORD_PTR dwAffinity, LPWSTR str )
{
    WCHAR* p = str;
    for( DWORD msb = ( DWORD )1 << 31; msb; msb >>= 1 )
        *p++ = ( ( msb & ( DWORD )dwAffinity ) ? L'1' : L'0' );
    *p = 0;
    return str;
}

//-------------------------------------------------------------------------------------
// Name: wmain
//-------------------------------------------------------------------------------------
int wmain()
{
    _ASSERT( CICoreLen + CIEnabledLen + CIAffinityMaskLen + 4 < ConWidth );
    _ASSERT( SPILeftColumnLen + SPISystemLen + SPIProcessLen + 4 < ConWidth );

    _putws( L"" );
    _putws( L"This sample displays CPU core information for the current process.  At your" );
    _putws( L"command, a CPU-intensive thread will be created and executed on each enabled" );
    wprintf( L"core resulting in a %zu-second maximum load for the CPU.\n", CpuLoadTime / 1000 );
    _putws( L"" );
    _putws( L"You can view the performance in the Task Manager, as well as experiment with" );
    _putws( L"the process affinity (right-click the CoreDetection.exe process in the Task" );
    _putws( L"Manager and choose \"Set Affinity...\") to see the results in successive runs." );
    _putws( L"" );
    _putws( L"(hit any key to continue)" );
    (void)_getwch();

    CpuTopology cpu;
    // The number of system cores will not change for the duration of
    // the sample.  We only need to query it once.
    const DWORD dwSystemCores = cpu.NumberOfSystemCores();

    WCHAR ch = L'r';
    do
    {
        // The number of process cores can vary during execution of the
        // sample depending on if the process affinity has changed.
        // We need to query it each time through the loop to account
        // for any changes.
        const DWORD dwProcessCores = cpu.NumberOfProcessCores();

        if( ch == L'm' || ch == L'M' )
        {
            //
            // Max the CPU
            //
            wprintf( L"\nMaxing out the CPU for %zu seconds\n", CpuLoadTime / 1000 );
            wprintf( L"(%u Thread%s): ", dwProcessCores,
                     dwProcessCores > 1 ? L"s" : L"" );

            DWORD nThreads = 0;
            HANDLE threads[sizeof( DWORD_PTR ) * 8];
            for( DWORD i = 0; nThreads < dwProcessCores && i < dwSystemCores; ++i )
            {
                DWORD_PTR dwCoreAffinity = cpu.CoreAffinityMask( i );
                if( dwCoreAffinity )
                {
                    threads[nThreads] = ( HANDLE )_beginthreadex( nullptr,
                                                                  0,
                                                                  SpinThreadProc,
                                                                  ( void* )CpuLoadTime,
                                                                  CREATE_SUSPENDED,
                                                                  nullptr );
                    SetThreadAffinityMask( threads[nThreads], dwCoreAffinity );
                    ResumeThread( threads[nThreads] );
                    ++nThreads;
                }
            }

            // Display load indicator.
            do
            {
                wprintf( L"%c", 0xb1 );
            } while( WAIT_TIMEOUT == WaitForMultipleObjects( nThreads, threads, TRUE, CpuLoadIndicatorFreq ) );

            // Cleanup resources.
            for( DWORD i = 0; i < nThreads; ++i )
                CloseHandle( threads[i] );

            _putws( L"\nAll threads have exited.\n" );
        }
        else if( ch == L'r' || ch == L'R' )
        {
            //
            // Refresh the system information
            //
            _wsystem( L"CLS" );  // clear screen

            DWORD_PTR dwSystemAffinity, dwProcessAffinity;
            if( GetProcessAffinityMask( GetCurrentProcess(),
                                        &dwProcessAffinity,
                                        &dwSystemAffinity ) )
            {
                // Get the number of logical processors on the system.
                SYSTEM_INFO si = { 0 };
                GetSystemInfo( &si );
                DWORD dwLogProcs = si.dwNumberOfProcessors;

                // Get the number of logical processors available to the process.
                DWORD dwAvailableLogProcs = 0;
                for( DWORD_PTR lp = 1; lp; lp <<= 1 )
                {
                    if( dwProcessAffinity & lp )
                        ++dwAvailableLogProcs;
                }

                // scratch buffer for a full console line of text
                WCHAR scratch[ConWidth+1];
                WCHAR* p;

                //
                // System/Process Info Table
                //
                _putws( SPITitle );

                // display top border
                p = wmemset( scratch, 0xcd, ConWidth );
                *p = 0xc9;             // left corner
                *( p += SPILeftColumnLen ) = 0xd1;             // separator
                *( p += SPISystemLen ) = 0xd1;             // separator
                *( p += SPIProcessLen ) = 0xbb;             // right corner
                *++p = 0;
                _putws( scratch );

                // display headings
                wprintf( L"%c% *s%c%s%c%s%c\n", 0xba,
                         SPILeftColumnLen - 1,
                         L"",
                         0xb3,
                         SPISystemStr,
                         0xb3,
                         SPIProcessStr,
                         0xba );

                // display central border
                p = wmemset( scratch, 0xc4, ConWidth );
                *p = 0xc7;                 // left border
                *( p += SPILeftColumnLen ) = 0xc5;                 // separator
                *( p += SPISystemLen ) = 0xc5;                 // separator
                *( p += SPIProcessLen ) = 0xb6;                 // right border
                *++p = 0;
                _putws( scratch );

                // display core information
                wprintf( L"%c% *s%c% *u% *s%c% *u% *s%c\n", 0xba,
                         SPILeftColumnLen - 1,
                         SPICpuCoresStr,
                         0xb3,
                         SPIHalfSystemLen,
                         dwSystemCores,
                         SPISystemLen - SPIHalfSystemLen - 1,
                         L"",
                         0xb3,
                         SPIHalfProcessLen,
                         dwProcessCores,
                         SPIProcessLen - SPIHalfProcessLen - 1,
                         L"",
                         0xba );

                // display logical processor information
                wprintf( L"%c% *s%c% *u% *s%c% *u% *s%c\n", 0xba,
                         SPILeftColumnLen - 1,
                         SPILogProcsStr,
                         0xb3,
                         SPIHalfSystemLen,
                         dwLogProcs,
                         SPISystemLen - SPIHalfSystemLen - 1,
                         L"",
                         0xb3,
                         SPIHalfProcessLen,
                         dwAvailableLogProcs,
                         SPIProcessLen - SPIHalfProcessLen - 1,
                         L"",
                         0xba );

                // display bottom border
                p = wmemset( scratch, 0xcd, ConWidth );
                *p = 0xc8;                 // left corner
                *( p += SPILeftColumnLen ) = 0xcf;                 // separator
                *( p += SPISystemLen ) = 0xcf;                 // separator
                *( p += SPIProcessLen ) = 0xbc;                 // right corner
                *++p = 0;
                _putws( scratch );

                //
                // Core Info table
                //
                _putws( CITitle );

                // display top border
                p = wmemset( scratch, 0xcd, ConWidth );
                *p = 0xc9;             // left corner
                *( p += CICoreLen ) = 0xd1;             // separator
                *( p += CIEnabledLen ) = 0xd1;             // separator
                *( p += CIAffinityMaskLen ) = 0xbb;             // right corner
                *++p = 0;
                _putws( scratch );

                // display headings
                wprintf( L"%c%s%c%s%c%s%c\n", 0xba,             // left border
                         CICoreStr,        // core header
                         0xb3,             // separator
                         CIEnabledStr,     // enabled header
                         0xb3,             // separator
                         CIAffinityMaskStr,// affinity mask header
                         0xba );           // right border

                // display central border
                p = wmemset( scratch, 0xc4, ConWidth );
                *p = 0xc7;             // left corner
                *( p += CICoreLen ) = 0xc5;             // separator
                *( p += CIEnabledLen ) = 0xc5;             // separator
                *( p += CIAffinityMaskLen ) = 0xb6;             // right corner
                *++p = 0;
                _putws( scratch );

                for( DWORD coreIdx = 0; coreIdx < dwSystemCores; ++coreIdx )
                {
                    // display the core affinity mask
                    DWORD_PTR dwCoreAffinity = cpu.CoreAffinityMask( coreIdx );

                    WCHAR sAffinity[33];
                    GetAffinityStr( dwCoreAffinity, sAffinity );

                    wprintf( L"%c% *u% *s%c", 0xba,
                             CIHalfCoreLen,
                             coreIdx,
                             CICoreLen - CIHalfCoreLen - 1,
                             L"",
                             0xb3 );

                    wprintf( L"% *s% *s%c", CIHalfEnabledLen,
                             dwCoreAffinity ? L"*" : L"",
                             CIEnabledLen - CIHalfEnabledLen - 1,
                             L"",
                             0xb3 );

                    wprintf( L" %s %c\n", sAffinity, 0xbA );
                }

                // display bottom border
                p = wmemset( scratch, 0xcd, ConWidth );
                *p = 0xc8;         // left corner
                *( p += CICoreLen ) = 0xcf;         // separator
                *( p += CIEnabledLen ) = 0xcf;         // separator
                *( p += CIAffinityMaskLen ) = 0xbc;         // right corner
                *++p = 0;
                _putws( scratch );

                _putws( L"" );
                _putws( L"NOTE: The CORE AFFINITY MASK reported in this sample reflects all logical" );
                _putws( L"processors enabled on a corresponding core (more than 1 bit enabled in a" );
                _putws( L"single core mask indicates SMT/Hyperthreading). A core is not considered" );
                _putws( L"disabled until all corresponding logical processors are disabled. Task" );
                _putws( L"Manager reports each logical processor as a system CPU." );
                _putws( L"" );

            }
        }

        wprintf( L"(R)efresh, (M)ax out cpu for %zu seconds, (Q)uit\n", CpuLoadTime / 1000 );
        ch = _getwch();

    } while( L'q' != ch && L'Q' != ch );

    return 0;
}
