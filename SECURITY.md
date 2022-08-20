## Security

THESE SAMPLES ARE FROM THE LEGACY DIRECTX SDK AND ARE PROVIDED ON AN "AS IS" BASIS FOR REFERENCE AND DEVELOPER EDUCATION ONLY.

### DirectX Components

These samples use Direct3D 11, DirectInput, XInput 9.1.0, XInput 1.4, and XAudio 2.8/2.9 via the Windows SDK, and are all serviced through the OS.

For down-level support of XAudio 2 on Windows 7, the audio samples make use of the [XAudio2Redist](https://aka.ms/XAudio2Redist) NuGet package.

### Open Source Components

These samples make use of the GitHub versions of [DXUT for Direct3D 11](https://github.com/microsoft/DXUT) and [Effects for Direct3D 11](https://github.com/microsoft/FX11). One sample makes use of [DirectX Tool Kit for DirectX 11](https://github.com/microsoft/DirectXTK). All three libraries are subject to the [Microsoft open source security policy](https://github.com/microsoft/.github/security/policy).

### Legacy DirectX SDK Components

The legacy DirectX SDK is out of its support lifecycle, and no longer receives any security updates. This includes D3DX9, D3DX10, D3DX11, D3DCompiler 43 and prior, XInput 1.1 - 1.3, XAudio 2.7 and earlier, X3DAudio 1.0 - 1.7, and all versions of XACT. All these components are deployed by the end-of-life *DirectX End-User Runtime* (a.k.a. DXSETUP), and the payload DLLs are only available SHA-1 signed. **SHA-1 signing is deprecated due to known security weaknesses**.

*This repository makes no use of the legacy DirectX SDK or the DirectX End-User Runtime.*
