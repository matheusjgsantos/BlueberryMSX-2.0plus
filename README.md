Blueberry Pi
============

**Blueberry Pi** is a port of [blueMSX][1] to Raspberry Pi.

Emulator runs from the command line, with [same arguments as the Windows/SDL versions][2]. To quit, press **F12**.

Current Status
--------------

Emulator performs best at 900MHz (Medium overclock setting in `raspi-config`). Default configuration uses `frameskip=1`, though most PSG games are more or less fluid with `frameskip=0`.

By default, MoonSound, MSX Audio, and MSX Music are disabled, and there are no plans to get these working. SCC-based games occasionally stutter, though mostly fine with a frameskip setting of 1. 

Current Goals
------------

* Add joypad support
* Add a simple game selection screen
* Get games running at full framerate, including those utilizing SCC

[1]: http://bluemsx.com/
[2]: http://www.msxblue.com/manual/commandlineargs_c.htm
