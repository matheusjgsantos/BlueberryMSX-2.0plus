BlueberryMSX 2.0 Plus
============
![blueberryMSX](http://i.imgur.com/Tnq9vSY.png "blueberry")

**BlueberryMSX-2.0Plus** is an update from a port of [blueMSX](http://bluemsx.com/) to Raspberry Pi, originally distributed by [Meeso Kim](https://github.com/meesokim).

I took the task to upgrade the whole project to run on the newest Raspberry Pi 4/400, removing all dependencies from the DispmanX library and upgrading the [SDL library from 1.2 to 2.0](https://wiki.libsdl.org/MigrationGuide). The emulator now builds and runs on both 32-bit (armhf) and 64-bit (aarch64) Raspberry Pi OS. You can find the newest GPIO Slot board at [Retro Game Restore](https://retrogamerestore.com/store/rpi400msx/) store, which is the same I used to test this version.

Notice that I have zero experience programming in C/C++, so this process took almost 6 months to result in a running program. Right now I think that I'm starting to grasp the concept of the whole thing :-)

As the previous version, the emulator runs from the command line with the [same arguments as the Windows/SDL versions](http://www.msxblue.com/manual/commandlineargs_c.htm0) and supports joysticks/joypads. Because it draws directly to framebuffer using the [Direct Render Management](https://www.kernel.org/doc/html/v4.15/gpu/introduction.html)/[Kernel Mode Settings](https://www.kernel.org/doc/html/v4.15/gpu/drm-kms.html) it does not need a windowing environment and can be run without X.

Shortcuts:

* Press **F12** to quit
* Press **F11** to disable frame skipping
* Press **F10**, **F9**, **F8** to set frame skipping to 1, 2, 3, respectively
* Press **ALT+F10** to toggle scanline
* Press **CTRL+F10** to toggle 4:3 forced for 16:9 or more resolution.
* Press **F6** to soft reset
* Press **F7** to take a screen shot.

Hardware reference:

The RPMC slot board (cartridge slots, front-panel LEDs, pin-out, bus protocol,
clock, build and test procedure) is documented in **[Doc/RPMC.md](Doc/RPMC.md)**.

Current Status:
--------------

Everything that depended on the [DispmanX](https://raspberry-projects.com/pi/programming-in-c/display/dispmanx-api/dispmanx-api-general) API and SDL version 1.2 was replaced, and there are still a few things that need to be fixed (see "Known issues"), but the emulator runs pretty well — including on 64-bit (aarch64) Raspberry Pi OS, with the RPMC slot board (cartridges and front-panel LEDs) working on both 32-bit and 64-bit.

How to install on a new SD card image:
--------

 - Download and write the latest [RaspiOS image](https://www.raspberrypi.org/software/operating-systems/) in a 2GB+ SD Card. Use the **64-bit (aarch64)** image: the committed `bluemsx-pi` binary is 64-bit and has the bcm2835 GPIO driver statically linked, so no extra GPIO library is needed. (For a 32-bit (armhf) image, build from source or cross-compile — see below.)

 - Boot the RPI4/400 with the SD card then configure the internet connection (wireless or wired, your choice)

 - Update apt packages:

 `$ sudo apt update && sudo apt upgrade -y`

 - Install the libraries needed to run the committed binary: SDL2, GLES, EGL, KMS/DRM, GBM and the audio backends:

  `$ sudo apt install -y libsdl2-2.0-0 libdrm2 libgbm1 libgles2 libegl1 libgl1-mesa-dri libasound2 libpulse0 libsamplerate0`

 - Make sure you have the parameters below configured at the [all] section of the /boot/config.txt file:
 ```
  [all]
  dtparam=audio=on
  dtoverlay=vc4-fkms-v3d
  max_framebuffers=2
 ```
 - Reboot the RaspberryPi if you made any changes in the config.txt file

 - Clone the BlueberryMSX-2.0plus repo

 `$ git clone https://github.com/matheusjgsantos/BlueberryMSX-2.0plus.git`

 - Finally run the BlueberryMSX with GPIO slot support (the slot bus and the
   front-panel LEDs read `/dev/mem` directly, so run as root or add your user
   to the `gpio` group):
 ```
  $ cd ~/BlueberryMSX-2.0plus
  $ sudo ./bluemsx.sh
 ```

How to build from source:
-----------

- Install the following development libs:
`sudo apt install -y libsdl2-dev libdrm-dev libgbm-dev autoconf`
- Configure and build the BCM2835 GPIO lib (a copy is included in this repo):
   `cd bcm2835-1.68`
   `./configure`
   `make && sudo make install`
   
   This installs the **static** `libbcm2835.a` into `/usr/local/lib`, so the
   emulator links the GPIO driver statically — the resulting binary has no
   runtime GPIO dependency.
- Running `make` from `~/BlueberryMSX-2.0plus` should do the trick.
- All source files are available at the `~/BlueberryMSX-2.0plus/Src`, but documentation is still incomplete (see `Doc/RPMC.md` for the board)

Cross-compiling for Raspberry Pi (32-bit/armhf) from x86_64 using Docker
--------------------------------------------------------

Build ARMv7 binaries on an x86_64 host with Docker Buildx and an arm32v7 Debian base. (For 64-bit, build natively on the Pi — the committed binary is aarch64.)

Prerequisites
- Docker with buildx enabled
- `docker buildx build --platform linux/arm/v7`

Base build image
```
docker buildx build --platform linux/arm/v7 -f Dockerfile.arm -t bluemsx-arm-build .
```
`Dockerfile.arm` uses `arm32v7/debian:bookworm` and installs:
`build-essential libsdl2-dev libdrm-dev libgbm-dev autoconf git pkg-config`

The bcm2835 GPIO lib is required for the Pi slot board. It is not in Debian, so the build image compiles the copy that ships in this repo (`bcm2835-1.68`).

Full source build image
```
docker buildx build --platform linux/arm/v7 -f Dockerfile.build.arm -t bluemsx-arm-built .
```
`Dockerfile.build.arm`:
- FROM arm32v7/debian:bookworm
- Install build deps
- WORKDIR /work, `COPY . .`
- `cd bcm2835-1.68 && ./configure && make && sudo make install`
- `RUN make clean && make`

Extract the binary
```
docker create --name bluemsx_tmp bluemsx-arm-built
docker cp bluemsx_tmp:/work/bluemsx-pi ./bluemsx-pi-arm
docker rm bluemsx_tmp
```

The resulting `bluemsx-pi-arm` is an ARMv7 binary ready for Raspberry Pi 3/4 with a 32-bit OS.

Known issues:
-------
 - Emulator only works on HDMI0, I need to add logic to make DRM "discover" which HDMI port is in use and enable it. Zero idea how to do this
 - Sometimes the emulator gets upset and decides to disable sound. Check bluemsx.ini for `sound.masterEnable=yes` entry and fix it if changed to `no`
 - Moonsound / MSX-Music / YM2413 sound is silent: those C++ emulators don't compile on the aarch64 toolchain and are stubbed out for now — the cartridges load, but without sound
 - Improvements, improvements and more improvements

Resolved issues:
-------
 - Screen resolution code is odd, opens a 800x600 screen even when the desired configuration is 640x480. Need to figure out what is happening - **FIXED**
 - RPMC power led
 - Slot 2 is now working, tested with 2 cartridges inserted
 - Black screen on 64-bit (aarch64) Raspberry Pi OS: the Moonsound/MSX-Music ROM mapper stubs were declared `void`, but board init does `success &= romMapper...Create(...)`. On ARM64 the empty stubs never write the return register (X0), so the caller ANDed the *first argument* (a string pointer, always even-aligned) into the success flag and got 0 — the machine was never created. Fixed by giving the stubs the proper `int` return, with correct buffer ownership (see `Src/Pi/stubs.c`) - **FIXED**
 - 64-bit build: WiringPi is not available on aarch64, so all GPIO access (cartridge slot bus and the 74HC595 front-panel LEDs) moved to the **bcm2835** lib. The board's 6 front-panel LEDs (PWR, SLT1, SLT2, I/O, HAN, CAPS) now light on both 32-bit and 64-bit — see `Doc/RPMC.md` - **FIXED**
 - 64-bit build errors: `byte` typedef clash in the OpenMSX sound headers (renamed `byte_t`), missing includes and a wrong `pthread_create` callback signature in `PiUdev.c`, unguarded MIDI file I/O in `MidiIO.c` - **FIXED**
 - Keyboard mapping was broken under SDL2: the event handlers were called with the whole `SDL_Event*` instead of the specific union member, so they read unrelated fields. `PiMain.c` now passes `&event->key` / `&event->jbutton` / `&event->jaxis` - **FIXED**
 - Display: `SDL_Init(SDL_INIT_EVERYTHING)` was fighting the raw DRM/GBM/EGL path for the display — now only the needed subsystems are initialized; EGL binds `EGL_OPENGL_ES_API` instead of `EGL_OPENGL_API`; framebuffer creation uses `drmModeAddFB2` with `DRM_FORMAT_XRGB8888` and reports `drmModeSetCrtc` failures instead of ignoring them - **FIXED**
