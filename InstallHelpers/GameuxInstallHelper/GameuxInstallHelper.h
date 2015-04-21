//--------------------------------------------------------------------------------------
// File: GameuxInstallHelper.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <gameux.h>
#include <msi.h>
#include <msiquery.h>

//--------------------------------------------------------------------------------------
// UNICODE/ANSI define mappings
//--------------------------------------------------------------------------------------
#ifdef UNICODE
    #define GameExplorerInstall GameExplorerInstallW
    #define GameExplorerUninstall GameExplorerUninstallW
    #define GameExplorerUpdate GameExplorerUpdateW
#else
    #define GameExplorerInstall GameExplorerInstallA
    #define GameExplorerUninstall GameExplorerUninstallA
    #define GameExplorerUpdate GameExplorerUpdateA
#endif

//--------------------------------------------------------------------------------------
// Given a game path to GDF binary, registers the game with Game Explorer
//
// [in] strGDFBinPath: the full path to the GDF binary 
// [in] strGameInstallPath: the full path to the folder where the game is installed.  
//                          This folder will be under the protection of parental controls after this call
// [in] InstallScope: if the game is being installed for all users or just the current user 
//--------------------------------------------------------------------------------------
STDAPI GameExplorerInstallW( WCHAR* strGDFBinPath, WCHAR* strGameInstallPath, GAME_INSTALL_SCOPE InstallScope );
STDAPI GameExplorerInstallA( CHAR* strGDFBinPath, CHAR* strGameInstallPath, GAME_INSTALL_SCOPE InstallScope );

//--------------------------------------------------------------------------------------
// Given a game path to GDF binary, unregisters the game with Game Explorer
//
// [in] strGDFBinPath: the full path to the GDF binary 
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUninstallW( WCHAR* strGDFBinPath );
STDAPI GameExplorerUninstallA( CHAR* strGDFBinPath );

//--------------------------------------------------------------------------------------
// Given a game path to GDF binary, updates a registered game with Game Explorer
//
// [in] strGDFBinPath: the full path to the GDF binary 
//--------------------------------------------------------------------------------------
STDAPI GameExplorerUpdateW( WCHAR* strGDFBinPath );
STDAPI GameExplorerUpdateA( CHAR* strGDFBinPath );

//--------------------------------------------------------------------------------------
// For use during an MSI custom action install. 
// This sets up the CustomActionData properties for the deferred custom actions. 
//--------------------------------------------------------------------------------------
UINT WINAPI GameExplorerSetMSIProperties( MSIHANDLE hModule );

//--------------------------------------------------------------------------------------
// For use during an MSI custom action install. 
// This adds the game to the Game Explorer
//--------------------------------------------------------------------------------------
UINT WINAPI GameExplorerInstallUsingMSI( MSIHANDLE hModule );

//--------------------------------------------------------------------------------------
// For use during an MSI custom action install. 
// This removes the game to the Game Explorer
//--------------------------------------------------------------------------------------
UINT WINAPI GameExplorerUninstallUsingMSI( MSIHANDLE hModule );
