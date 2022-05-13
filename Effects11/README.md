![DirectX Logo](https://raw.githubusercontent.com/wiki/Microsoft/FX11/Dx_logo.GIF)

# Effects for Direct3D 11 (FX11)

http://go.microsoft.com/fwlink/?LinkId=271568

Copyright (c) Microsoft Corporation. All rights reserved.

**December 2, 2021**

Effects for Direct3D 11 (FX11) is a management runtime for authoring HLSL shaders, render state, and runtime variables together.

This code is designed to build with Visual Studio 2017 ([15.9](https://walbourn.github.io/vs-2017-15-9-update/)) or Visual Studio 2019. It is recommended that you make use of the Windows 10 May 2020 Update SDK ([19041](https://walbourn.github.io/windows-10-may-2020-update-sdk/)).

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

## Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/FX11/wiki).

## Notices

*This project is 'archived'. It is still available for use for legacy projects, but use of it for new projects is not recommended.*

All content and source code for this package are subject to the terms of the [MIT License](https://github.com/microsoft/FX11/blob/main/LICENSE).

## Contributing

This project welcomes contributions and suggestions. Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

## Credits

The Effects library for Direct3D 9 (FX9) was the work of Loren McQuade with contributions from Relja Markovic.

The Effects library for Direct3D 10 (FX10) was the work of John Rapp and Kutta Srinivasan as a rewrite of the FX9 library with contributions from Anuj Gosalia, Kev Gee, Sam Glassenberg, Relja Markovic, and Ian McIntyre.

The Effects library for Direct3D 11 (FX11) is the work of Ian McIntyre based on FX10 with contributions from Michael Oneppo and Chuck Walbourn.

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
D3DCompile API (46), is supported but generates a deprecation warning with D3DCompile API (47). The fx profiles
are not supported by the DXIL (DXC.EXE) compiler.
