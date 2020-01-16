![DirectX Logo](https://github.com/Microsoft/DXUT/wiki/Dx_logo.GIF)

# DXUT for Direct3D 11

http://go.microsoft.com/fwlink/?LinkId=320437

Copyright (c) Microsoft Corporation. All rights reserved.

**January 16, 2020**

DXUT is a "GLUT"-like framework for Direct3D 11.x Win32 desktop applications; primarily
samples, demos, and prototypes.

This code is designed to build with Visual Studio 2015, Visual Studio 2017, or Visual Studio 2019. It is recommended that you make use of the latest updates (VS 2015 Update 3, VS 2017 15.9 update, etc.).

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

## Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/DXUT/wiki).

## Notices

*This project is 'archived'. It is still available for use for legacy projects, but use of it for new projects is not recommended.*

All content and source code for this package are subject to the terms of the [MIT License](http://opensource.org/licenses/MIT).

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Samples

* Direct3D Tutorial08 - 10
* BasicHLSL11, EmptyProject11, SimpleSample11
* DXUT+DirectXTK Simple Sample

These are hosted on [GitHub](https://github.com/walbourn/directx-sdk-samples)

## Disclaimer

DXUT is being provided as a porting aid for older code that makes use of the legacy DirectX SDK, the deprecated D3DX9/D3DX11 library, and the DXUT11 framework. It is a cleaned up version of the original DXUT11 that will build with the Windows 8.1 / 10 SDK and does not make use of any legacy DirectX SDK or DirectSetup deployed components.

The DXUT framework is for use in Win32 desktop applications. It not usable for Universal Windows Platform apps, Windows Store apps,
Xbox One apps, or Windows phone.

This version of DXUT only supports Direct3D 11, and therefore is not compatible with Windows XP or early versions of Windows Vista.

## Release Notes

* The VS 2017/2019 projects make use of ``/permissive-`` for improved C++ standard conformance. Use of a Windows 10 SDK prior to the Fall Creators Update (16299) or an Xbox One XDK prior to June 2017 QFE 4 may result in failures due to problems with the system headers. You can work around these by disabling this switch in the project files which is found in the ``<ConformanceMode>`` elements, or in some cases adding ``/Zc:twoPhase-`` to the ``<AdditionalOptions>`` elements.
