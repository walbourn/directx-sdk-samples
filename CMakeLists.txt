# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.20)

project(DirctX-SDK-Samples
  DESCRIPTION "Direct3D 11, XInput, and XAudio2 samples C++ samples from the legacy DirectX SDK."
  HOMEPAGE_URL "https://github.com/walbourn/directx-sdk-samples")

add_subdirectory(AdaptiveTessellationCS40)
add_subdirectory(BasicCompute11)
add_subdirectory(BasicHLSL11)
add_subdirectory(CascadedShadowMaps11)
add_subdirectory(Collision)
add_subdirectory(ComputeShaderSort11)
add_subdirectory(EmptyProject11)
add_subdirectory(SimpleBezier11)
add_subdirectory(SimpleSample11)

add_subdirectory(Direct3D11Tutorials/Tutorial01)
add_subdirectory(Direct3D11Tutorials/Tutorial02)
add_subdirectory(Direct3D11Tutorials/Tutorial03)
add_subdirectory(Direct3D11Tutorials/Tutorial04)
add_subdirectory(Direct3D11Tutorials/Tutorial05)
add_subdirectory(Direct3D11Tutorials/Tutorial06)
add_subdirectory(Direct3D11Tutorials/Tutorial07)

add_subdirectory(Direct3D11TutorialsDXUT/Tutorial08)
add_subdirectory(Direct3D11TutorialsDXUT/Tutorial09)
add_subdirectory(Direct3D11TutorialsDXUT/Tutorial10)

add_subdirectory(XAudio2/XAudio2AsyncStream)
add_subdirectory(XAudio2/XAudio2BasicSound)

if(NOT MINGW)
  # Effects for Direct3D 11 is not compatible with MinGW
  add_subdirectory(BasicHLSLFX11)

  add_subdirectory(Direct3D11TutorialsFX11/Tutorial11)
  add_subdirectory(Direct3D11TutorialsFX11/Tutorial12)
  add_subdirectory(Direct3D11TutorialsFX11/Tutorial13)
  add_subdirectory(Direct3D11TutorialsFX11/Tutorial14)

  # xapobase.lib is not compatible with MinGW
  add_subdirectory(XAudio2/XAudio2CustomAPO)
endif()
