#Requires -RunAsAdministrator

$msg = "Desktop+ UIAccess Setup Script
------------------------------
This enables UIAccess rights for Desktop+ to lift interaction restrictions with higher privileged applications and UAC prompts.
This is done by signing the DesktopPlus.exe executable with a self-signed certificate, using a different side-by-side manifest and changing group policies if needed.
The private key of the certificate created in this script is deleted afterwards and does not remain on the system.
Nevertheless it's recommended to inspect what this script does and use your own judgement before just running this.
The script needs to be run again each time Desktop+ has been updated or otherwise modified.
"

Write-Host $msg

#Ask before continuing
$opt_yes = New-Object System.Management.Automation.Host.ChoiceDescription "&Yes", "Apply changes and enable UIAccess for Desktop+"
$opt_no  = New-Object System.Management.Automation.Host.ChoiceDescription "&No",  'Cancel'
$options = [System.Management.Automation.Host.ChoiceDescription[]]($opt_yes, $opt_no)

$result = $host.ui.PromptForChoice("", "Continue and apply these changes?", $options, 1)

if ($result -eq 1)
{
    exit
}

#Apply the changes
$CertificateSubject = "DesktopPlus UIAccess Script"

#Remove old certificates if they exist
Get-ChildItem Cert:\LocalMachine\Root |
Where-Object { $_.Subject -match $CertificateSubject } |
Remove-Item

Get-ChildItem Cert:\LocalMachine\CA |
Where-Object { $_.Subject -match $CertificateSubject } |
Remove-Item


#Create the new self-signed certificate
$cert = New-SelfSignedCertificate -CertStoreLocation Cert:\LocalMachine\My -Type CodeSigningCert -Subject $CertificateSubject -NotAfter (Get-Date).AddYears(5)

#Export the certificate to import it again as a root certificate, but without the private key
Export-Certificate -Cert $cert -FilePath ".\DesktopPlusCert.cer" | Out-Null
Import-Certificate -FilePath ".\DesktopPlusCert.cer" -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null


#Sign DesktopPlus.exe
Set-AuthenticodeSignature -FilePath "..\DesktopPlus.exe" -Certificate $cert | Out-Null


#Backup normal side-by-side manifest (doesn't overwrite if already exists)
Move-Item "..\DesktopPlus.exe.manifest" -Destination "EnableUIAccessDesktopPlusBackup.manifest" -ErrorAction SilentlyContinue
#Replace side-by-side manifest with the UIAccess enabled one
Copy-Item "EnableUIAccessDesktopPlus.manifest" -Destination "..\DesktopPlus.exe.manifest"


#Disable "User Account Control: Only elevate UIAccess applications that are installed in secure locations" group policy if we are outside of the Program Files directory
if (!$pwd.path.StartsWith($env:ProgramFiles)) 
{
    Set-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System" -Name "EnableSecureUIAPaths" -Value 0
}


#Cleanup
Remove-Item ".\DesktopPlusCert.cer"

#We do not leave the certificate with the private key on the system. Signing with it was a one-time thing.
Set-Location "Cert:"
$cert | Remove-Item -DeleteKey

Write-Host "Done."
Write-Output "`n"

cmd /c pause