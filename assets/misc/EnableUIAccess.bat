@echo off

pushd %~dp0
echo Launching script with administrator privileges...
powershell -command "   Start-Process PowerShell -Verb RunAs \""-ExecutionPolicy Bypass -Command `\""cd '%cd%'; & '.\EnableUIAccess.ps1';`\""\""   "
