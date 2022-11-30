![DirectX Logo](https://raw.githubusercontent.com/wiki/Microsoft/DXUT/Dx_logo.GIF)

# DXUT for Direct3D 11

http://go.microsoft.com/fwlink/?LinkId=320437

Copyright (c) Microsoft Corporation.

**October 24, 2022**

DXUT is a "GLUT"-like framework for Direct3D 11.x Win32 desktop applications; primarily samples, demos, and prototypes.

This code is designed to build with Visual Studio 2019 (16.11 or later) or Visual Studio 2022. Use of the Windows 10 May 2020 Update SDK ([19041](https://walbourn.github.io/windows-10-may-2020-update-sdk/)) or later is required.

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

*This project is 'archived'. It is still available for use for legacy projects or when using older developer education materials, but use of it for new projects is not recommended.*

## Disclaimer

DXUT is being provided as a porting aid for older code that makes use of the legacy DirectX SDK, the deprecated D3DX9/D3DX11 library, and the DXUT11 framework. It is a cleaned up version of the original DXUT11 that will build with the Windows 8.1 / 10 SDK and does not make use of any legacy DirectX SDK or DirectSetup deployed components.

The DXUT framework is for use in Win32 desktop applications. It not usable for Universal Windows Platform apps, Windows Store apps,
Xbox, or Windows phone.

This version of DXUT only supports Direct3D 11, and therefore is not compatible with Windows XP or early versions of Windows Vista.

## Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/DXUT/wiki).

## Notices

All content and source code for this package are subject to the terms of the [MIT License](https://github.com/microsoft/DXUT/blob/main/LICENSE).

For the latest version of DXUT for Direct3D 11, please visit the project site on [GitHub](https://github.com/microsoft/DXUT).

> The legacy versions of **DXUT for DX11/DX9** and **DXUT for DX10/DX9** version are on [GitHub](https://github.com/walbourn/directx-sdk-legacy-samples). These both require using [Microsoft.DXSDK.D3DX](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX).

## Release Notes

* Starting with the July 2022 release, the ``bool forceSRGB`` parameter for DDSTextureLoader ``Ex`` functions is now a ``DDS_LOADER_FLAGS`` typed enum bitmask flag parameter. This may have a *breaking change* impact to client code. Replace ``true`` with ``DDS_LOADER_FORCE_SRGB`` and ``false`` with ``DDS_LOADER_DEFAULT``.

## Support

For questions, consider using [Stack Overflow](https://stackoverflow.com/questions/tagged/dxut) with the *dxut* tag.

## Contributing

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

## Credits

The DXUT library is the work of Shanon Drone, Jason Sandlin, and David Tuft with contributions from David Cook, Kev Gee, Matt Lee, and Chuck Walbourn.

## Samples

* Direct3D Tutorial08 - 10
* BasicHLSL11, EmptyProject11, SimpleSample11
* DXUT+DirectXTK Simple Sample

These are hosted on [GitHub](https://github.com/walbourn/directx-sdk-samples)
