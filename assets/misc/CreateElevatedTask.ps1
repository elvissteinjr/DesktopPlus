#Requires -RunAsAdministrator

$msg = 'Desktop+ Elevated Scheduled Task Creation Script
------------------------------------------------
This creates an elevated scheduled task for Desktop+ to allow it to simulate inputs with administrator rights without UAC prompt.
Desktop+ will only use this task when the button in the UI is used to enter elevated mode.
Please keep the security implications of doing this in mind.
At the very least consider restricting write access to the Desktop+ executable file.
'

Write-Host $msg

#We use an XML file with schtasks instead of the Powershell cmdlets as we can create the task from an elevated session without additional password input this way.
$xml = '<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Author>Desktop+</Author>
    <Description>This scheduled task is run manually by Desktop+ in order to allow it to be run with adminstrator rights without UAC prompt. 
Desktop+ will only use it when the button in the settings is used to switch into elevated mode.</Description>
  </RegistrationInfo>
  <Triggers />
  <Principals>
    <Principal id="Author">
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>HighestAvailable</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>false</AllowHardTerminate>
    <StartWhenAvailable>false</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>"'+ $PSScriptRoot + '\..\DesktopPlus.exe"</Command>
	  <Arguments>-ElevatedMode</Arguments>
    </Exec>
  </Actions>
</Task>'

#Write XML to file
$xml | Set-Content .\DesktopPlusElevatedTask.xml

#Create task
schtasks /Create /TN "DesktopPlus Elevated" /XML "DesktopPlusElevatedTask.xml" /F

#Delete the XML file after we're done
Remove-Item DesktopPlusElevatedTask.xml

Write-Output "`n"

cmd /c pause
