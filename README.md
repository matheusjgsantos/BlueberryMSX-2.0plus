blueberryMSX
============

**blueberryMSX** is a port of [blueMSX][1] to Raspberry Pi.

Emulator runs from the command line, with [same arguments as the Windows/SDL versions][2] and supports joysticks/joypads.

Shortcuts:

* Press **F12** to quit
* Press **F11** to disable frame skipping
* Press **F10**, **F9**, **F8** to set frame skipping to 1, 2, 3, respectively

Current Status
--------------

Emulator performs best at 900MHz (Medium overclock setting in `raspi-config`). Default configuration uses `frameskip=1`, though most MSX1/PSG games are more or less fluid with `frameskip=0`.

By default, MoonSound, MSX Audio, and MSX Music are disabled, and there are no plans to get these working. SCC-based games stutter, though mostly fine with a frameskip setting of 1 or 2.

Current Goals
------------

* Add a simple game selection screen
* Get games running at full framerate, including those utilizing SCC

[1]: http://bluemsx.com/
[2]: http://www.msxblue.com/manual/commandlineargs_c.htm
