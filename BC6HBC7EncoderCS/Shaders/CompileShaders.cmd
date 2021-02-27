@echo off
rem Copyright (c) Microsoft Corporation.
rem Licensed under the MIT License (MIT).

setlocal
set error=0

call :CompileShader BC7Encode TryMode456CS
call :CompileShader BC7Encode TryMode137CS
call :CompileShader BC7Encode TryMode02CS
call :CompileShader BC7Encode EncodeBlockCS

call :CompileShader BC6HEncode TryModeG10CS
call :CompileShader BC6HEncode TryModeLE10CS
call :CompileShader BC6HEncode EncodeBlockCS

echo.

if %error% == 0 (
    echo Shaders compiled ok
) else (
    echo There were shader compilation errors!
)

endlocal
exit /b

:CompileShader
set fxc=fxc /nologo %1.hlsl /Tcs_4_0 /Zpc /Qstrip_reflect /Qstrip_debug /E%2 /FhCompiled\%1_%2.inc /Vn%1_%2
echo.
echo %fxc%
%fxc% || set error=1
exit /b
