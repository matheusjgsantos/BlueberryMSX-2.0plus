blueberryMSX
============

**blueberryMSX** is a port of [blueMSX][1] to Raspberry Pi.

Emulator runs from the command line, with [same arguments as the Windows/SDL versions][2] and supports joysticks/joypads. Because it draws directly to framebuffer, it does not need a windowing environment and can be run without X.

Shortcuts:

* Press **F12** to quit
* Press **F11** to disable frame skipping
* Press **F10**, **F9**, **F8** to set frame skipping to 1, 2, 3, respectively

Current Status
--------------

Emulator performs best at 900MHz (Medium overclock setting in `raspi-config`). At this speed, most games run smoothly at full framerate; however certain SCC games (including Metal Gear 2) stutter unless run at `frameskip=1`.

By default, MoonSound, MSX Audio, and MSX Music are disabled, and there are no plans to get these working.

GPIO
----

blueberryMSX can be compiled to take advantage of Raspberry Pi's GPIO pins. For now this is limited to dedicated LED's for MSX Power, and FDD0/FDD1 activity. See the schematic in [Doc/GPIO_schema.png] (/Doc/GPIO_schema.png).

To compile with GPIO support:

1. Download and install the [wiringPi library] (http://wiringpi.com/download-and-install/)
2. Include the `wiringPi` library when linking: `LIBS += -lwiringPi` (line 45 in the [Makefile] (/Makefile))
3. Add the `RASPI_GPIO` macro to the list of flags: `CFLAGS += -DRASPI_GPIO` (line 46)

Note that the executables compiled with `RASPI_GPIO` **require superuser privileges to run** (e.g. `sudo bluemsx-pi`).

Current Goals
------------

* Add a simple game selection screen
* Get games running at full framerate, including those utilizing SCC

[1]: http://bluemsx.com/
[2]: http://www.msxblue.com/manual/commandlineargs_c.htm
