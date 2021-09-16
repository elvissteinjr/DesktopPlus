#pragma once

#define NOMINMAX
#include <windows.h>

//This runs the process in elevated input command mode, in which it only takes select window messages and passes them to InputSimulator.
//
//This isn't secure at all, I'm well aware.
//There are certainly other more complex ways of implementation to make this seem more secure than the current one, but as far as I'm concerned we're fighting a battle that cannot be won.
//As long as we're open source, not enforcing signed binaries and access rights to the application directory, there will always be an attack vector.
//There's no way to actually trust the unelevated process requesting the inputs. Even if we isolated DesktopPlus.exe, we still couldn't trust SteamVR to not be compromised.
//Both of these live in an environment where they are expected to be updated automatically by an unelevated process as well.
//
//The attack vector is at least smaller than when running Steam and everything related elevated as well.
//Targeted attacks would be fairly easy if such code found the way of an user's machine, though.

int ElevatedModeEnter(HINSTANCE hinstance);