Desktop+, an advanced SteamVR Desktop Overlay, by elvissteinjr
--------------------------------------------------------------

Installation
------------
Extract the complete archive if not done yet. Put the files in a location where they can stay.
Run DesktopPlus.exe. This will launch SteamVR if it's not already running. A VR-HMD must be connected.
If everything is fine, a message box will come up to indicate successful first-time setup.
Desktop+ will continue running as a SteamVR overlay application afterwards.
If the message does not come up on the first launch, check the Troubleshooting section.

Desktop+ will register itself as an overlay application to SteamVR and run automatically on following SteamVR launches.
If you move the files of this application, you'll have to repeat these steps.


Updates
-------
The latest version of Desktop+ can be found on https://github.com/elvissteinjr/DesktopPlus/


Uninstallation
--------------
Delete all files that came with the archive. 
SteamVR will automatically remove the overlay application entry when the executable isn't present anymore.
Desktop+ does not write to files outside its own directory. However, if you did set up elevated mode, its scheduled task will
remain.


User Guide
----------
A detailed user guide can be found on https://github.com/elvissteinjr/DesktopPlus/blob/master/docs/user_guide.md
It's recommended to finish reading this document beforehand, however, as the user guide does not cover some topics this readme
covers.


Configuration
-------------
Desktop+ can be fully configured from within VR. If desired, the settings interface can also be used from the desktop, however.
Either run DesktopPlusUI.exe while SteamVR is not running, press the [Misc|Troubleshooting|Desktop+ UI: Restart in Desktop Mode]
button in the VR settings interface, or run DesktopPlusUI.exe with the "-DesktopMode" command line argument to do so.
Some settings are only available while SteamVR is running.

Settings are applied instantly and written to disk when the settings window is dismissed or Desktop+ UI closes.
The setting slider values can be edited directly by right-clicking the slider.


Overlay Management
------------------
To use multiple overlays in Desktop+, click on the "Manage" button at the top-right of the Overlay settings page. This will open
a pop-up in which overlays can be added, removed and organized.
This and several other pop-ups can also be dismissed by simple clicking outside of it, in addition to the "Done" button.
Overlays can be renamed by right-clicking the drop-down selector at the top of the Overlay settings page.
The current overlay setup will be remembered automatically between sessions. Overlay profiles can be used to save and restore
multiple of such setups.


Actions
-------
Desktop+ offers built-in and user-definable actions which can be bound to controller buttons, the main bar or just executed from
the list.
Custom icons can be added by putting PNG files in the "images/icons" folder. Recommended size is 96x96 pixels.

About language support for action and profile names:
To save memory, only common Latin characters are loaded by default. Most symbols and eastern characters can be used, but will
appear as the placeholder "?" character at first. Additional characters will be loaded and display once the action or profile is
saved.


Global Shortcuts & Input Features
---------------------------------
Actions can be bound to 3 different global shortcuts which can be activated by the SteamVR Input bindings.
Desktop+ does not come with default SteamVR Input bindings. These can be set up by going to the Desktop+ controller bindings.
SteamVR currently only lists overlay applications in the old input binding interface, so that one has to be used instead.

Apart from the 3 global shortcuts, a function for floating overlay interaction can also be bound.
"Enable Floating Overlay Interaction": Can be used to enable the laser pointer outside of the dashboard.


Elevated Mode
-------------
As Desktop+ is subject to User Interface Privilege Isolation (UIPI), it can't simulate input or
move the cursor when a higher privileged application (i.e. running as administrator) is in focus.
Desktop+ offers an easy way to quickly switch into elevated mode to get around this restriction when needed.

! Please be aware of the security implications of this. Desktop+ takes no additional security measures in this mode.
! An attacker could spoof OpenVR input, replace DesktopPlus.exe or change config.ini to have actions run their applications.
! The author of Desktop+ shall not be liable for any damage caused by the use of this mode.

In order to use elevated mode, a scheduled task has to be set up to allow Desktop+ to be launched as an administrator without
requiring approval through an UAC prompt. To do this, run "misc\CreateElevatedTask.bat".
It can then be accessed in [Misc|Troubleshooting|Desktop+: Restart Elevated].
If you move Desktop+' files, the scheduled task will break but the button remains. Re-run the batch file to fix this.

Note that actions launching applications will run them with the same privileges as Desktop+, so be careful if you don't intend
them to have administrator rights.

In general, if you don't know what you're doing, reconsider just activating this. This mode is provided as a convenience to
prevent users just running it elevated at all times instead.
Switch back into normal mode as soon as possible.


Keyboard Extension
------------------
The keyboard extension adds additional keyboard modifier toggles, tab, function and arrow keys below the SteamVR keyboard when
used with the Desktop+ overlay.
It can be disabled in [Input|Keyboard|Enable Keyboard Extension] should issues from it arise or future SteamVR changes break it.
Note that the modifier toggles directly manipulate the keyboard state, so using them and the hardware keyboard at the same time
may conflict.


Troubleshooting
---------------
As long as DesktopPlusUI.exe is running (launched automatically alongside Desktop+), critical errors will be displayed in VR
with an option to restart Desktop+. Very early runtime errors may not be displayed in VR, but can be read in error.log when they
occur.

In general, note that Desktop+ is using APIs which require Windows 8 or newer.
Using Graphics Capture overlays requires at least Windows 10 1803 for basic support, Windows 10 2004 or newer for full support.


Black screen with question mark display icon instead of desktop mirror:
-
An error occurred trying to duplicate the desktop. This may happen when displays were disconnected or are unavailable for another
reason. You can try restarting Desktop+ or switch to another desktop if you have multiple displays.


Shaky/Delayed laser pointer:
-
By default, the laser pointed cursor may seem to lag behind a little bit, while other screen updates happen instantly.
This is in order to reduce the CPU load. Enable [Performance|Misc|Rapid Laser Pointer Updates] to increase the accuracy of the
laser pointer.


High GPU load when overlay visible and cursor moving:
-
In order to provide the lowest latency possible, all cursor updates are processed instantly, even if they occur more frequently
than the screen's vertical blanks.
Using the Frame Time limiter in [Performance|Update Limiter|Limiter Mode] with a low limit value can reduce the load from cursor
movement while leaving other screen updates unaffected.


Laser pointer not working when dashboard opened via HMD button:
-
By default the laser pointer will be deactivated if the dashboard had been opened via the HMD button and the physical
mouse was moved. Re-open the overlay to activate it again or disable [Input|Mouse|HMD-Pointer Override] to turn this feature off.


Input not working in certain applications:
-
Input simulated by Desktop+ is subject to User Interface Privilege Isolation (UIPI), see the Elevated Mode section for a
workaround.


Overlay is no longer visible:
-
There are several settings controlling overlay visibility and position. Check if they are not set to unexpected values.
Especially of interest are the cropping values. The cropping rectangle is preserved when switching between capture sources, but
that also means it could be invalid for the newly selected mirrored window or desktop.
If that's the case there will be a "(!)" warning next to the Cropping Rectangle section title. Simply reset it then.

Loading the Default overlay profile also works as a quick way to restore the initial overlay state.


Overlay settings are not applying:
-
The overlay settings page always applies to the current overlay. Make sure to have selected the right overlay as current when
making changes.


No overlays visible on laptop:
-
On laptops with hybrid-GPU solutions, the desktops are typically rendered on the power-saving integrated GPU. Make sure to have
DesktopPlus.exe set to be running on integrated graphics so it can mirror them.


Warnings
--------
Desktop+ may display several warnings in its settings interface. They are mostly informational and can be safely ignored.
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
This message serves as a reminder about Desktop+ being in elevated mode.
Please keep the security implications of that mode in mind and don't run the application like this when not needed.


"An elevated process has focus! Desktop+ is unable to simulate input right now."
-
User Interface Privilege Isolation (UIPI) prevents Desktop+ from simulating input when an elevated process has focus.
This warning persists until the a window with the same or lesser privilege level has gained focus again.
Clicking on this warning also offers the option to have Desktop+ try to change the focus to another window itself or to restart
in elevated mode (only if the scheduled task is configured).


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
This message appears when a Graphics Capture thread crashed. One or more overlays using Graphics Capture will not be updated anymore.
While this shouldn't ever happen, this message can be safely ignored if it only appears once. The affected overlays will need to 
have their source be set again from either changing it or reloading a profile.


License
-------
Desktop+ is licensed under the GPL 3.0.

For the third-party licenses, see third-party_licenses.txt.