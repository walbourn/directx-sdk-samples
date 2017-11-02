DXUT FOR DIRECT3D 11
--------------------

Copyright (c) Microsoft Corporation. All rights reserved.

November 2, 2017

DXUT is a "GLUT"-like framework for Direct3D 11.x Win32 desktop applications; primarily
samples, demos, and prototypes.

This code is designed to build with Visual Studio 2013 Update 5, Visual Studio 2015 Update 3,
or Visual Studio 2017. It is recommended that you make use of VS 2015 Update 3, Windows Tools
1.4.1, and the Windows 10 Anniversary Update SDK (14393) or VS 2017 with the Windows 10
Fall Creators Update SDK (16299).

These components are designed to work without requiring any content from the DirectX SDK. For details,
see "Where is the DirectX SDK?" <http://msdn.microsoft.com/en-us/library/ee663275.aspx>.

All content and source code for this package are subject to the terms of the MIT License.
<http://opensource.org/licenses/MIT>.

For the latest version of DXUT11, more detailed documentation, etc., please visit the project site.

http://go.microsoft.com/fwlink/?LinkId=320437

This project has adopted the Microsoft Open Source Code of Conduct. For more information see the
Code of Conduct FAQ or contact opencode@microsoft.com with any additional questions or comments.

https://opensource.microsoft.com/codeofconduct/


-------
SAMPLES
-------

Direct3D Tutorial08 - 10
BasicHLSL11, EmptyProject11, SimpleSample11
DXUT+DirectXTK Simple Sample

These are hosted on GitHub <https://github.com/walbourn/directx-sdk-samples>


----------
DISCLAIMER
----------

DXUT is being provided as a porting aid for older code that makes use of the legacy DirectX SDK, the deprecated D3DX9/D3DX11
library, and the DXUT11 framework. It is a cleaned up version of the original DXUT11 that will build with the Windows 8.1 SDK
and does not make use of any legacy DirectX SDK or DirectSetup deployed components.

The DXUT framework is for use in Win32 desktop applications. It not usable for Universal Windows Platform apps, Windows Store apps,
Xbox One apps, or Windows phone.
 
This version of DXUT only supports Direct3D 11, and therefore is not compatible with Windows XP or early versions of Windows Vista.


-------------
RELEASE NOTES
-------------

* The VS 2017 projects make use of /permissive- for improved C++ standard conformance. Use of a Windows 10 SDK prior to
  the Fall Creators Update (16299) may result in failures due to problems with the system headers. You can work around
  these by deleting /permissive- from the project files which is found in the <AdditionalOptions> element.


---------------
RELEASE HISTORY
---------------

November 2, 2017 (11.17)
    VS 2017 updated for Windows 10 Fall Creators Update SDK (16299)
    Optional support for Direct3D 11.4 (define USE_DIRECT3D11_4 in projects using the 14393 or later Windows 10 SDK)

October 13, 2017 (11.16)
    Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
    Updated for VS 2017 update 15.1 - 15.3 and Windows 10 SDK (15063)    

March 10, 2017 (11.15)
    Add VS 2017 projects
    Minor code cleanup

September 15, 2016 (11.14)
    Updated WICTextureLoader and ScreenGrab

August 2, 2016 (11.13)
    Updated for VS 2015 Update 3 and Windows 10 SDK (14393)

April 26, 2016 (11.12)
    Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
    Retired VS 2012 projects and obsolete adapter code
    Minor code and project file cleanup

November 30, 2015 (11.11)
    Updated DDSTextureLoader, ScreenGrab, DXERR
    Updated for VS 2015 Update 1 and Windows 10 SDK (10586)

July 29, 2015 (11.10)
    Updated for VS 2015 and Windows 10 SDK RTM
    Retired VS 2010 projects

June 16, 2015 (11.09)
    Optional support for Direct3D 11.3 (define USE_DIRECT3D11_3 in VS 2015 projects)

April 14, 2015 (11.08)
    Fix for auto-gen of volume textures
    More updates for VS 2015

November 24, 2014 (11.07)
    Minor fix for Present usage
    Minor fix for CBaseCamera::GetInput
    Minor fix for WIC usage of IWICFormatConverter
    Updates for Visual Studio 2015 Technical Preview

July 28, 2014 (11.06)
    Optional support for Direct3D 11.2 (define USE_DIRECT3D11_2 in VS 2013 projects)
    Fixes for various UI and F2 device settings dialog issues
    Fixes for device and format enumeration
    Changed default resolution to 800x600
    Code review fixes

January 24, 2014 (11.05)
    Added use of DXGI debugging when available
    Resolved CRT heap leak report
    Fixed compile bug in DXUTLockFreePipe
    Fixed bug reported in DXUT's sprite implementation
    Code cleanup (removed DXGI_1_2_FORMATS control define; ScopedObject typedef removed)

October 21, 2013 (11.04)
    Updated for Visual Studio 2013 and Windows 8.1 SDK RTM
    Minor fixes for systems which only have a "Microsoft Basic Renderer" device

September 2013 (11.03)
    Removed dependencies on the D3DX9 and D3DX11 libraries, so DXUT no longer requires the legacy DirectX SDK to build.
    It does require the d3dcompiler.h header from the Windows 8.x SDK.
    Includes standalone DDSTextureLoader, WICTexureLoader, ScreenGrab, and DxErr modules.
    Removed support for Direct3D 9 and Windows XP
    Deleted the DXUTDevice9.h/.cpp, SDKSound.h/.cpp, and SDKWaveFile.h/.cpp files
    Deleted legacy support for MCE relaunch
    General C++ code cleanups (nullptr, auto keyword, C++ style casting, Safer CRT, etc.) which are
        compatible with Visual C++ 2010 and 2012
    SAL2 annotation and /analyze cleanup
    Added DXUTCompileFromFile, DXUTCreateShaderResourceViewFromFile, DXUTCreateTextureFromFile, DXUTSaveTextureToFile helpers
    Added '-forcewarp' command-line switch
    Added support for DXGI 1.1 and 1.2 formats
    Added Direct3D 11.1 Device/Context state
    Support Feature Level 11.1 when available

June 2010 (11.02)
    The DirectX SDK (June 2010) included an update to DXUT11. This is the last version to support Visual Studio 2008,
    Windows XP, or Direct3D 9. The source code is located in Samples\C++\DXUT11.

February 2010 (11.01)
    An update was shipped with the DirectX SDK (February 2010). This is the last version to support Visual Studio 2005.
    The source code is located in Samples\C++\DXUT11.

August 2009 (11.00)
    The initial release of DXUT11 was in DirectX SDK (August 2009). The source code is located in Samples\C++\DXUT11.
    This was a port of the original DXUT which supported Direct3D 10 / Direct3D 9 applications on Windows XP and Windows Vista.
