starrem
rem To simplify debugging of this MSI DLL, make this Visual Studio's debug command like so
rem     cmd.exe /c updatemsi.bat
rem Then debugging the project will run this batch file which will 
rem auto-uninstall the MSI, then update it with the new DLL, and then run the new MSI
rem

msiexec /q /uninstall D3D11InstallHelper.msi
call cscript WiStream.vbs D3D11InstallHelper.msi Debug\D3D11InstallHelper.dll Binary.D3D11IH
D3D11InstallHelper.msi