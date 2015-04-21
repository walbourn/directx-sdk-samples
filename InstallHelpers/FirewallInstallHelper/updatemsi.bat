rem
rem To simplify debugging of this MSI DLL, make this Visual Studio's debug command like so
rem     cmd.exe /c updatemsi.bat
rem Then debugging the project will run this batch file which will 
rem auto-uninstall the MSI, then update it with the new DLL, and then run the new MSI
rem

msiexec /q /uninstall FirewallInstallHelper.msi
call cscript WiStream.vbs FirewallInstallHelper.msi Debug\FirewallInstallHelper.dll Binary.FIREWALL
FirewallInstallHelper.msi