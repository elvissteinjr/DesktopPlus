Desktop+, an advanced SteamVR Desktop Overlay, by elvissteinjr
--------------------------------------------------------------

Installation (GitHub Version)
-----------------------------
Extract the complete archive if not done yet. Put the files in a location where they can stay.
Run DesktopPlus.exe. This will also launch SteamVR if it's not already running. A VR-HMD must be connected.
If everything is fine, a message will come up to indicate successful first-time setup.
Desktop+ will continue running in the background as a SteamVR overlay application afterwards.
If the message does not come up on the first launch, check the Troubleshooting section.

Desktop+ will register itself as an overlay application to SteamVR and run automatically on following SteamVR launches.
If you move the files of this application, you'll have to repeat these steps.


Installation (Steam Version)
----------------------------
Simply install the application through Steam and launch it. This will also launch SteamVR if it's not already running.
A VR-HMD must be connected. If everything is fine, a message will come up to indicate successful first-time setup.
Desktop+ will continue running in the background as a SteamVR overlay application afterwards.
If the message does not come up on the first launch, check the "Startup / Shutdown" settings in SteamVR and the Troubleshooting
section.


Updates
-------
The latest version of Desktop+ can be found on https://github.com/elvissteinjr/DesktopPlus/ and on Steam.


Uninstallation (GitHub Version)
-------------------------------
Delete all files that came with the archive. 
SteamVR will automatically remove the overlay application entry when the executable isn't present anymore.
Desktop+ does not write to files outside its own directory. However, if you did set up elevated mode, its scheduled task will
remain.


Uninstallation (Steam Version)
------------------------------
Uninstall the application through Steam.
Steam will not delete your configuration and profiles. They can be found in "[Steam Library Path]\SteamApps\common\DesktopPlus"
and be safely deleted if desired. These files are also synced to the Steam Cloud if that feature is enabled.
Additionally, if you did set up elevated mode, its scheduled task will remain.


User Guide
----------
A detailed user guide can be found on https://github.com/elvissteinjr/DesktopPlus/blob/master/docs/user_guide.md
It's recommended to finish reading this document beforehand, however, as the user guide does not cover some topics this readme
covers.


Configuration
-------------
Desktop+ can be fully configured from within VR. If desired, the settings interface can also be used from the desktop, however.
Either run DesktopPlusUI.exe while SteamVR is not running, press [Restart in Desktop Mode] in the Troubleshooting section of the
Settings window, or run DesktopPlusUI.exe with the "--DesktopMode" command line argument to do so.
Some settings are only available while SteamVR is running.

Settings are applied instantly and written to disk when the settings window is dismissed or Desktop+ closes.
The setting slider values can be edited directly by right-clicking the slider.


Overlay Management
------------------
All overlays in Desktop+ are listed in the Overlay Bar that appears in Desktop+'s SteamVR dashboard tab.
They can be rearranged by dragging the icons across the bar. Clicking on an overlay's icon opens a menu to toggle visibility,
duplicate, remove or open the Overlay Properties window for the selected overlay.

The current overlay setup will be remembered automatically between sessions. Overlay profiles can be used to save and restore
multiple of such setups.


Actions
-------
Desktop+ offers so-called actions which can be bound to controller buttons, the Floating UI or just executed from the list.
Actions consist of a series of commands that can control the state of your overlays, simulate input and execute programs.

Custom icons can be added by putting PNG files in the "images/icons" folder. Recommended size is 96x96 pixels.


Global Shortcuts & Input Features
---------------------------------
Actions can be bound to up to 20 different global shortcuts which can be activated by the SteamVR Input bindings.
SteamVR doesn't list overlay applications in the regular application controller configuration list, but the Settings window in
Desktop+ has buttons that lead directly into the input binding screen for Desktop+.

Apart from the global shortcuts, bindings for Desktop+ laser pointer interaction can also be found.
These mirror SteamVR's default laser pointer bindings for known devices, but can be adjusted as desired.

There's also "Enable Global Laser Pointer", which can be used to enable the laser pointer outside of the dashboard.
However, laser pointer auto-activation is enabled by default, making this binding not strictly necessary to use.

Outside of the dashboard/when SteamVR's system laser pointer is not active, Desktop+ uses its own laser pointer implementation.
This is allows for additional features that can be configured in the Laser Pointer section of the Settings window.


Elevated Mode / Enabling UIAccess
---------------------------------
As Desktop+ is subject to User Interface Privilege Isolation (UIPI), it can't simulate input or move the cursor when a higher
privileged application (i.e. running as administrator) is in focus.
Desktop+ offers multiple ways to deal with this, such as elevated mode or enabling UIAccess for the application.
See "misc\!About this folder.txt" for details.


Keyboard
--------
Desktop+ comes with a custom VR keyboard. The used keyboard layout and behavior can be configured in the "Keyboard" section of
the Settings window.

Keys of the Desktop+ keyboard can be right-clicked to toggle their state. The application will not automatically release keys
held down this way while it's running, so keep that in mind.
Desktop+ tries to map the VR keyboard keys to the keyboard layout chosen in the OS to maximize compatibility, before falling back
to string inputs. It's recommended to select a keyboard layout in Desktop+ that matches the one in the OS.

Keyboard layouts can be created or modified in the Keyboard Layout Editor. This editor can only be accessed in desktop mode.
In desktop mode, the editor can be found in the "Keyboard" section of the Settings window, after clicking on the Keyboard Layout
button, [Switch to Keyboard Layout Editor] on the page that follows.


Troubleshooting
---------------
Desktop+ runs as two processes (DesktopPlus.exe & DesktopPlusUI.exe), both of which write log files in the application's install
directory (DesktopPlus.log & DesktopPlusUI.log).

While most errors will be displayed in VR, it is helpful to check the contents of these log files when troubleshooting.
Make sure to include them when seeking help as well.

In general, note that Desktop+ is using APIs which require Windows 8.1 or newer.
Using Graphics Capture overlays requires at least Windows 10 1903 for basic support, Windows 10 2004 or newer for full support,
and Windows 11 to remove the yellow border around captures (only visible on real display).
Additionally, Windows 11 24H2 allows including secondary windows, such as context menus, in the capture.


No first-time setup message / Desktop+ not auto-launching (Steam version):
-
The Steam version detects the need of first time setup by checking if an user configuration existed on launch.
If you previously had Desktop+ installed or the configuration was synced from another machine via Steam Cloud, you can enable
auto-launch manually in the "SteamVR" section of the Settings window or use [Restore Default Settings] in the "Troubleshooting"
section to start fresh.


Black screen with question mark display icon instead of desktop mirror:
-
An error occurred trying to duplicate the desktop. This may happen when displays were disconnected or are unavailable for another
reason. You can try restarting Desktop+ or changing the capture method to Graphics Capture in the Overlay Properties window.


Shaky/Delayed laser pointer:
-
By default, the laser pointed cursor may seem to lag behind a little bit, while other screen updates happen instantly.
This is in order to reduce the CPU load. Enable "[x] Reduce Laser Pointer Latency" in the "Performance" section of the Settings
window to increase the accuracy of the laser pointer.


High GPU load when overlay visible and cursor moving:
-
In order to provide the lowest latency possible, all cursor updates are processed instantly, even if they occur more frequently
than the screen's vertical blanks.
Using the Frame Time limiter in "Update Limiter Mode" in the "Performance" section of either the Settings or Overlay Properties
window with a low limit value can reduce the load from cursor movement while leaving other screen updates unaffected.


Using laser pointer after moving real mouse:
-
By default the laser pointer will be deactivated after the physical mouse was moved. Click on the overlay to activate it again or
disable "[x] Allow Laser Pointer Override" in the "Mouse" section of the Settings window to turn this feature off.


Input not working in certain applications:
-
Input simulated by Desktop+ is subject to User Interface Privilege Isolation (UIPI), see the Elevated Mode readme section for a
workaround.


Overlay is no longer visible:
-
There are several settings controlling overlay visibility and position. Check if they are not set to unexpected values.
Especially of interest are the cropping values. The cropping rectangle is preserved when switching between capture sources, but
that also means it could be invalid for the newly selected mirrored window or desktop.
If that's the case there will be a "(!)" warning next to the Cropping Rectangle section title. Simply reset it then.

Oculus/Meta headsets:
If all overlays, including the SteamVR dashboard disappear, this may be due to running a game that uses the headset's native APIs
and as such bypasses SteamVR entirely.
As Desktop+ relies on SteamVR to function, the only solution to this is to find a way to run the game with SteamVR.
Potential global workaround for this is using Steam Link for Meta Quest or enabling the "Force Use SteamVR" option available in 
OpenVR Advanced Settings.
Workarounds for some individual titles also exist, but are beyond the scope of this ReadMe.


No overlays visible on laptop:
-
On laptops with hybrid-GPU solutions, the desktops are typically rendered on the power-saving integrated GPU. Make sure to have
DesktopPlus.exe set to be running on integrated graphics so it can mirror them.


Not all desktops from multiple GPUs available:
-
Desktop+ does not support Desktop Duplication with desktops distributed across multiple GPUs. It only supports copying one GPU's
set of desktops over to the VR-rendering GPU when necessary.
However, the missing desktops are typically still able to be mirrored by using Graphics Capture as the overlay's capture method.


Warnings
--------
Desktop+ may display several warnings in the Settings window. They are mostly informational and can be safely ignored.
Click on warnings to dismiss or not have them show up again.

"Compositor resolution is below 100%! This affects overlay rendering quality.":
-
The resolution of the VR compositor is based on the auto-resolution calculated by SteamVR, regardless of whether this resolution
has been chosen as the VR render resolution or not. There's no official way to change this. The auto-resolution can be increased
by lowering the HMD's refresh rate or getting a faster GPU.
Unofficially, there are tools such as SteamVR-ForceCompositorScale to combat this behavior.


"Overlay render quality is not set to high!":
-
The overlay render quality is a setting in SteamVR. It is recommended to set it to high to improve the visual clarity of the
overlays.


"Desktop+ is running with administrative privileges!":
-
This message serves as a reminder about Desktop+ being elevated.
Using Desktop+'s elevated mode is recommended over running all of Desktop+ with administrative privileges.


"Elevated mode is active!":
-
This message serves as a reminder about Desktop+ being in elevated mode.
Please keep the security implications of that mode in mind and leave it once it's not needed anymore.


"The application profile for [application name] has overridden the current overlay layout. Changes made to overlays are not saved
automatically while it is active.":
-
This message serves as a reminder that an application profile is active. 
Automatic saving of overlays is disabled while this is the case. Update the assigned overlay profile if you wish to make
permanent changes that go with the application profile.


"An elevated process has focus! Desktop+ is unable to simulate input right now."
-
User Interface Privilege Isolation (UIPI) prevents Desktop+ from simulating input when an elevated process has focus.
This warning persists until the a window with the same or lesser privilege level has gained focus again.
Clicking on this warning also offers the option to have Desktop+ try to open the task switcher to change the focus to another
window or to enter elevated mode (only if the scheduled task is configured).


"An overlay creation failed!":
-
This message will typically appear with "(Maximum Overlay limit exceeded)" appended. It appears when the total overlay limit
in SteamVR has been exceeded. This limit is not set by Desktop+ and other overlay applications can affect how many overlays can
be created by Desktop+.
While the warning can be safely ignored, the overlays that were attempted to be created will be missing.
If this message appears with a different error status appended, it might be because of a bug in Desktop+ or SteamVR. Please
report it in that case.


"An unexpected error occurred in a Graphics Capture thread!":
-
This message appears when a Graphics Capture thread crashed. One or more overlays using Graphics Capture will not be updated
anymore. While this shouldn't ever happen, this message can be safely ignored if it only appears once. The affected overlays will
need to have their source be set again from either changing it or reloading a profile.


"Desktop+ is no longer running with UIAccess privileges!":
-
This message appears when Desktop+ was previously configured to run with UIAccess privileges but no longer is.
UIAccess is enabled by patching the DesktopPlus.exe executable and as such is not retained across application updates.
Simply redo the process of enabling UIAccess to fix this.


"Browser overlays are being used, but the Desktop+ Browser component is currently not available.":
-
Browser overlays require the optional Desktop+ Browser application component.
It can be installed from https://github.com/elvissteinjr/DesktopPlusBrowser or Steam (as DLC for Desktop+).


"The installed Desktop+ Browser component is incompatible with this version of Desktop+!":
-
The installed version of Desktop+ Browser must be compatible with the running version of Desktop+. While not all updates require
a new version to be installed, it is generally a recommended to use the latest builds of both.


License
-------
Desktop+ is licensed under the GPL 3.0.

For the third-party licenses, see third-party_licenses.txt.