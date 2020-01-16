![DirectX Logo](https://github.com/Microsoft/FX11/wiki/X_jpg.jpg)

# Effects for Direct3D 11 (FX11)

http://go.microsoft.com/fwlink/?LinkId=271568

Copyright (c) Microsoft Corporation. All rights reserved.

**April 26, 2019**

Effects for Direct3D 11 (FX11) is a management runtime for authoring HLSL shaders, render state, and runtime variables together.

This code is designed to build with Visual Studio 2015, Visual Studio 2017, or Visual Studio 2019. It is recommended that you make use of the latest updates (VS 2015 Update 3, VS 2017 15.9 update, etc.).

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

## Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/FX11/wiki).

## Notices

*This project is 'archived'. It is still available for use for legacy projects, but use of it for new projects is not recommended.*

All content and source code for this package are subject to the terms of the [MIT License](http://opensource.org/licenses/MIT).

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Samples

* Direct3D Tutorial 11-14
* BasicHLSLFX11, DynamicShaderLinkageFX11, FixedFuncEMUFX11, InstancingFX11

These are hosted on [GitHub](https://github.com/walbourn/directx-sdk-samples)

## Disclaimer

Effects 11 is being provided as a porting aid for older code that makes use of the Effects 10 (FX10) API or Effects 9 (FX9)
API in the deprecated D3DX9 library. See [Microsoft Docs](https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d11-graphics-programming-guide-effects-differences) for a list of differences compared to the Effects 10 (FX10) library.

The Effects 11 library is for use in Win32 desktop applications. FX11 requires the D3DCompiler API be available at runtime
to provide shader reflection functionality, and this API is not deployable for Windows Store apps on Windows 8.0, Windows RT,
or Windows phone 8.0.

The fx_5_0 profile support in the HLSL compiler is deprecated, and does not fully support DirectX 11.1 HLSL features
such as minimum precision types. It is supported in the Windows 8.1 SDK version of the HLSL compiler (FXC.EXE) and
D3DCompile API (46), is supported but generates a deprecation warning with D3DCompile API (47), and could be removed
in a future update.
