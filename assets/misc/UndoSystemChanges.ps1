#Requires -RunAsAdministrator

$msg = "Desktop+ Cleanup Script
-----------------------
This reverts system-wide changes made by the scripts in the `"misc`" folder of Desktop+.

It removes the following:
- `"DesktopPlus Elevated`" scheduled task
- `"DesktopPlus UIAccess Script`" self-signed certificates
- Desktop+ UIAccess side-by-side manifest

Not reverted are changes which may impact other applications or may not have been different from the start, such as:
- `"User Account Control: Only elevate UIAccess applications that are installed in secure locations`" group policy
"

Write-Host $msg

#Ask before continuing
$opt_yes = New-Object System.Management.Automation.Host.ChoiceDescription "&Yes", "Undo changes made by Desktop+ scripts"
$opt_no  = New-Object System.Management.Automation.Host.ChoiceDescription "&No",  'Cancel'
$options = [System.Management.Automation.Host.ChoiceDescription[]]($opt_yes, $opt_no)

$result = $host.ui.PromptForChoice("", "Continue and undo these changes?", $options, 1)

if ($result -eq 1)
{
    exit
}

#Delete scheduled task
schtasks /Delete /TN "DesktopPlus Elevated" /F

#Remove certificates
Get-ChildItem Cert:\LocalMachine\Root |
Where-Object { $_.Subject -match $CertificateSubject } |
Remove-Item

Get-ChildItem Cert:\LocalMachine\CA |
Where-Object { $_.Subject -match $CertificateSubject } |
Remove-Item

#Restore previously backed-up side-by-side manifest
Move-Item "EnableUIAccessDesktopPlusBackup.manifest" -Destination "..\DesktopPlus.exe.manifest" -Force

Write-Host "Done."
Write-Output "`n"

cmd /c pause