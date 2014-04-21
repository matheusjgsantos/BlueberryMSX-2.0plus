Blueberry Pi
============

**Blueberry Pi** is a port of [blueMSX][1] to Raspberry Pi.

Current Status
--------------

Emulator runs from the command line, with [same arguments as the Windows/SDL versions][2].

Most MSX1 and MSX2 games run at full speed, with a single frameskip. MoonSound, MSX Audio, and MSX Music are disabled and not supported - they are too taxing on the hardware. SCC emulation is not perfect - games that utilize the SCC chip (e.g. Metal Gear 2) run slow and sound stutters.

Current Goals
------------

* Get games running at full framerate, including those utilizing SCC
* Add a simple game selection screen

[1]: http://bluemsx.com/
[2]: http://www.msxblue.com/manual/commandlineargs_c.htm
